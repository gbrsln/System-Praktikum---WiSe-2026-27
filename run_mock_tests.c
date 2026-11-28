#include "mock_server.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SCENARIO_DIRECTORY "tests/scenarios"
#define LOG_DIRECTORY "tests/logs"
#define MOCK_CLIENT_PATH "./sysprak-client-mock"
#define VALID_GAME_ID "abcdefghij123"

#define SERVER_TIMEOUT_SECONDS 5
#define CLIENT_TIMEOUT_SECONDS 10
#define CLIENT_POLL_INTERVAL_MILLISECONDS 100

#define MAX_SCENARIO_FILES 64
#define MAX_FILE_NAME_LENGTH 256
#define MAX_PATH_LENGTH 512
#define MAX_CLIENT_ARGUMENTS 16

typedef enum {
    CLIENT_SUCCEEDED,
    CLIENT_FAILED,
    CLIENT_CRASHED,
    CLIENT_HUNG,
    CLIENT_STATUS_UNKNOWN
} ClientOutcome;

static int passedTests = 0;
static int failedTests = 0;

static void reportResult(const char *testName, bool testPassed, const char *failureReason) {
    if (testPassed) {
        passedTests++;
        printf("ok    %s\n", testName);
    } else {
        failedTests++;
        printf("FEHL  %s: %s\n", testName, failureReason);
    }
}

static void sleepMilliseconds(int milliseconds) {
    struct timespec sleepDuration = {.tv_sec = milliseconds / 1000, .tv_nsec = (milliseconds % 1000) * 1000000L};
    nanosleep(&sleepDuration, NULL);
}

static pid_t startClient(char *clientArguments[], const char *clientLogPath) {
    pid_t clientPid = fork();
    if (clientPid != 0) {
        return clientPid;
    }

    int clientLog = open(clientLogPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (clientLog >= 0) {
        dup2(clientLog, STDOUT_FILENO);
        dup2(clientLog, STDERR_FILENO);
        close(clientLog);
    }
    execv(MOCK_CLIENT_PATH, clientArguments);
    perror("execv");
    _exit(127);
}

static ClientOutcome waitForClient(pid_t clientPid) {
    int clientStatus = 0;
    int waitedMilliseconds = 0;

    while (true) {
        pid_t waitResult = waitpid(clientPid, &clientStatus, WNOHANG);
        if (waitResult == clientPid) {
            break;
        }
        if (waitResult < 0 && errno != EINTR) {
            perror("waitpid");
            return CLIENT_STATUS_UNKNOWN;
        }
        if (waitedMilliseconds >= CLIENT_TIMEOUT_SECONDS * 1000) {
            kill(clientPid, SIGKILL);
            waitpid(clientPid, &clientStatus, 0);
            return CLIENT_HUNG;
        }
        sleepMilliseconds(CLIENT_POLL_INTERVAL_MILLISECONDS);
        waitedMilliseconds += CLIENT_POLL_INTERVAL_MILLISECONDS;
    }

    if (WIFSIGNALED(clientStatus)) {
        return CLIENT_CRASHED;
    }
    return WEXITSTATUS(clientStatus) == 0 ? CLIENT_SUCCEEDED : CLIENT_FAILED;
}

/* Bei Fehlschlag steht der Grund in failureReason. */
static bool clientOutcomeIsExpected(ClientOutcome outcome, bool clientShouldSucceed,
                                    const char *clientLogPath, char *failureReason, size_t failureReasonSize) {
    switch (outcome) {
    case CLIENT_HUNG:
        snprintf(failureReason, failureReasonSize, "Client hängt länger als %d s (siehe %s)",
                 CLIENT_TIMEOUT_SECONDS, clientLogPath);
        return false;
    case CLIENT_CRASHED:
        snprintf(failureReason, failureReasonSize, "Client ist abgestürzt (siehe %s)", clientLogPath);
        return false;
    case CLIENT_STATUS_UNKNOWN:
        snprintf(failureReason, failureReasonSize, "Ende des Clients konnte nicht abgefragt werden");
        return false;
    case CLIENT_SUCCEEDED:
        if (!clientShouldSucceed) {
            snprintf(failureReason, failureReasonSize,
                     "Client endet mit Exit-Code 0, obwohl ein Fehler erwartet war (siehe %s)", clientLogPath);
            return false;
        }
        return true;
    case CLIENT_FAILED:
        if (clientShouldSucceed) {
            snprintf(failureReason, failureReasonSize,
                     "Client endet mit einem Fehler, obwohl Erfolg erwartet war (siehe %s)", clientLogPath);
            return false;
        }
        return true;
    }
    return false;
}

static void runClientWithoutServer(const char *testName, char *clientArguments[]) {
    char clientLogPath[MAX_PATH_LENGTH];
    snprintf(clientLogPath, sizeof clientLogPath, "%s/%s.client.log", LOG_DIRECTORY, testName);

    pid_t clientPid = startClient(clientArguments, clientLogPath);
    if (clientPid < 0) {
        reportResult(testName, false, "fork ist fehlgeschlagen");
        return;
    }

    char failureReason[MAX_PATH_LENGTH * 2] = "";
    bool testPassed = clientOutcomeIsExpected(waitForClient(clientPid), false, clientLogPath,
                                              failureReason, sizeof failureReason);
    reportResult(testName, testPassed, failureReason);
}

static int splitClientArguments(char *argumentText, char *clientArguments[]) {
    int argumentCount = 0;
    clientArguments[argumentCount++] = MOCK_CLIENT_PATH;

    for (char *argument = strtok(argumentText, " "); argument != NULL; argument = strtok(NULL, " ")) {
        if (argumentCount == MAX_CLIENT_ARGUMENTS - 1) {
            return -1;
        }
        clientArguments[argumentCount++] = argument;
    }
    clientArguments[argumentCount] = NULL;
    return 0;
}

static void runScenario(const char *scenarioFileName) {
    char testName[MAX_FILE_NAME_LENGTH];
    snprintf(testName, sizeof testName, "%.*s", (int) (strlen(scenarioFileName) - strlen(".txt")), scenarioFileName);

    char scenarioPath[MAX_PATH_LENGTH];
    char serverLogPath[MAX_PATH_LENGTH];
    char clientLogPath[MAX_PATH_LENGTH];
    snprintf(scenarioPath, sizeof scenarioPath, "%s/%s", SCENARIO_DIRECTORY, scenarioFileName);
    snprintf(serverLogPath, sizeof serverLogPath, "%s/%s.server.log", LOG_DIRECTORY, testName);
    snprintf(clientLogPath, sizeof clientLogPath, "%s/%s.client.log", LOG_DIRECTORY, testName);

    Scenario scenario;
    if (loadScenario(scenarioPath, &scenario) != 0) {
        reportResult(testName, false, "Szenario-Datei ist fehlerhaft");
        return;
    }

    char argumentText[MAX_LINE_LENGTH];
    char *clientArguments[MAX_CLIENT_ARGUMENTS];
    snprintf(argumentText, sizeof argumentText, "%s", scenario.clientArguments);
    if (splitClientArguments(argumentText, clientArguments) != 0) {
        reportResult(testName, false, "zu viele Argumente in \"# args:\"");
        return;
    }

    /* "e" (close-on-exec), damit der gestartete Client die Log-Datei nicht erbt. */
    FILE *serverLog = fopen(serverLogPath, "we");
    if (serverLog == NULL) {
        perror(serverLogPath);
        reportResult(testName, false, "Server-Log kann nicht angelegt werden");
        return;
    }

    int listeningSocket = openListeningSocket(MOCK_PORT);
    if (listeningSocket < 0) {
        fclose(serverLog);
        reportResult(testName, false, "Port für den Mock-Server ist belegt");
        return;
    }

    pid_t clientPid = startClient(clientArguments, clientLogPath);
    if (clientPid < 0) {
        close(listeningSocket);
        fclose(serverLog);
        reportResult(testName, false, "fork ist fehlgeschlagen");
        return;
    }

    bool clientFollowedScenario = false;
    int clientSocket = acceptClient(listeningSocket, SERVER_TIMEOUT_SECONDS);
    if (clientSocket < 0) {
        fprintf(serverLog, "FEHLER: Kein Client hat sich innerhalb von %d s verbunden\n", SERVER_TIMEOUT_SECONDS);
    } else {
        clientFollowedScenario = playScenario(clientSocket, &scenario, SERVER_TIMEOUT_SECONDS, serverLog);
        close(clientSocket);
    }
    close(listeningSocket);
    fclose(serverLog);

    ClientOutcome outcome = waitForClient(clientPid);

    char failureReason[MAX_PATH_LENGTH * 2] = "";
    bool testPassed;
    bool clientEndedOnItsOwn = outcome == CLIENT_SUCCEEDED || outcome == CLIENT_FAILED;
    if (clientEndedOnItsOwn && !clientFollowedScenario) {
        snprintf(failureReason, sizeof failureReason, "Client hat anders reagiert als im Szenario (siehe %s)",
                 serverLogPath);
        testPassed = false;
    } else {
        testPassed = clientOutcomeIsExpected(outcome, scenario.clientShouldSucceed, clientLogPath,
                                             failureReason, sizeof failureReason);
    }
    reportResult(testName, testPassed, failureReason);
}

static int compareFileNames(const void *firstName, const void *secondName) {
    return strcmp((const char *) firstName, (const char *) secondName);
}

static bool hasTxtEnding(const char *fileName) {
    size_t nameLength = strlen(fileName);
    return nameLength > strlen(".txt") && strcmp(fileName + nameLength - strlen(".txt"), ".txt") == 0;
}

static void runAllScenarios(void) {
    DIR *scenarioDirectory = opendir(SCENARIO_DIRECTORY);
    if (scenarioDirectory == NULL) {
        perror(SCENARIO_DIRECTORY);
        reportResult("Szenarien", false, "Ordner " SCENARIO_DIRECTORY " fehlt");
        return;
    }

    char scenarioFileNames[MAX_SCENARIO_FILES][MAX_FILE_NAME_LENGTH];
    int scenarioCount = 0;
    struct dirent *directoryEntry;
    while ((directoryEntry = readdir(scenarioDirectory)) != NULL && scenarioCount < MAX_SCENARIO_FILES) {
        if (hasTxtEnding(directoryEntry->d_name)) {
            snprintf(scenarioFileNames[scenarioCount], MAX_FILE_NAME_LENGTH, "%s", directoryEntry->d_name);
            scenarioCount++;
        }
    }
    closedir(scenarioDirectory);

    qsort(scenarioFileNames, (size_t) scenarioCount, MAX_FILE_NAME_LENGTH, compareFileNames);
    for (int scenarioIndex = 0; scenarioIndex < scenarioCount; scenarioIndex++) {
        runScenario(scenarioFileNames[scenarioIndex]);
    }
}

int main(void) {
    if (access(MOCK_CLIENT_PATH, X_OK) != 0) {
        fprintf(stderr, "%s fehlt. Die Tests bitte mit \"make mocktest\" im Projektordner starten.\n",
                MOCK_CLIENT_PATH);
        return EXIT_FAILURE;
    }
    if (mkdir(LOG_DIRECTORY, 0755) != 0 && errno != EEXIST) {
        perror(LOG_DIRECTORY);
        return EXIT_FAILURE;
    }

    char *noArguments[] = {MOCK_CLIENT_PATH, NULL};
    char *validArguments[] = {MOCK_CLIENT_PATH, "-g", VALID_GAME_ID, NULL};

    printf("Kommandozeile:\n");
    runClientWithoutServer("args_missing_everything", noArguments);

    printf("Ohne laufenden Server:\n");
    runClientWithoutServer("server_not_running", validArguments);

    printf("Szenarien:\n");
    runAllScenarios();

    printf("\n%d bestanden, %d fehlgeschlagen\n", passedTests, failedTests);
    return failedTests == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
