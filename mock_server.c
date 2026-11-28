#include "mock_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define CLIENT_RECEIVE_BUFFER_SIZE (4 * MAX_LINE_LENGTH)
#define PAUSE_BETWEEN_CHUNKS_MILLISECONDS 10

typedef enum {
    LINE_RECEIVED,
    CONNECTION_CLOSED,
    CLIENT_TIMED_OUT,
    LINE_TOO_LONG
} ReadResult;

typedef struct {
    int socket;
    char buffer[CLIENT_RECEIVE_BUFFER_SIZE];
    size_t length;
} ClientLineReader;

typedef struct {
    bool clientArgumentsGiven;
    bool expectedResultGiven;
} RequiredHeaderLines;

/* Mit ^( )$ muss die ganze Zeile auf das Muster passen, nicht nur ein Teil davon. */
static bool compileWholeLinePattern(const char *pattern, regex_t *compiledPattern) {
    char wholeLinePattern[MAX_LINE_LENGTH + 8];
    snprintf(wholeLinePattern, sizeof wholeLinePattern, "^(%s)$", pattern);
    return regcomp(compiledPattern, wholeLinePattern, REG_EXTENDED | REG_NOSUB) == 0;
}

static bool isValidPattern(const char *pattern) {
    regex_t compiledPattern;
    if (!compileWholeLinePattern(pattern, &compiledPattern)) {
        return false;
    }
    regfree(&compiledPattern);
    return true;
}

static bool matchesPattern(const char *pattern, const char *text) {
    regex_t compiledPattern;
    if (!compileWholeLinePattern(pattern, &compiledPattern)) {
        return false;
    }
    bool textMatches = regexec(&compiledPattern, text, 0, NULL, 0) == 0;
    regfree(&compiledPattern);
    return textMatches;
}

static bool startsWith(const char *text, const char *prefix) {
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static void removeLineEnding(char *line) {
    line[strcspn(line, "\r\n")] = '\0';
}

static int addStep(Scenario *scenario, StepKind kind, const char *text) {
    if (scenario->stepCount == MAX_SCENARIO_STEPS) {
        return -1;
    }
    ScenarioStep *step = &scenario->steps[scenario->stepCount];
    step->kind = kind;
    snprintf(step->text, sizeof step->text, "%s", text);
    scenario->stepCount++;
    return 0;
}

static int parseScenarioLine(Scenario *scenario, const char *line, RequiredHeaderLines *headerLines) {
    if (line[0] == '\0') {
        return 0;
    }
    if (startsWith(line, "# args: ")) {
        snprintf(scenario->clientArguments, sizeof scenario->clientArguments, "%s", line + strlen("# args: "));
        headerLines->clientArgumentsGiven = true;
        return 0;
    }
    if (startsWith(line, "# expect-exit: ")) {
        const char *expectedResult = line + strlen("# expect-exit: ");
        if (strcmp(expectedResult, "success") != 0 && strcmp(expectedResult, "failure") != 0) {
            return -1;
        }
        scenario->clientShouldSucceed = strcmp(expectedResult, "success") == 0;
        headerLines->expectedResultGiven = true;
        return 0;
    }
    if (startsWith(line, "# chunk-size: ")) {
        char *endOfNumber;
        long chunkSize = strtol(line + strlen("# chunk-size: "), &endOfNumber, 10);
        if (*endOfNumber != '\0' || chunkSize < 1 || chunkSize > MAX_LINE_LENGTH) {
            return -1;
        }
        scenario->chunkSize = (int) chunkSize;
        return 0;
    }
    if (line[0] == '#') {
        return 0;
    }
    if (strcmp(line, "CLOSE") == 0) {
        return addStep(scenario, STEP_CLIENT_CLOSES, "");
    }
    if (startsWith(line, "S: ")) {
        return addStep(scenario, STEP_SERVER_SENDS, line + strlen("S: "));
    }
    if (startsWith(line, "C: ")) {
        return addStep(scenario, STEP_CLIENT_SENDS_EXACTLY, line + strlen("C: "));
    }
    if (startsWith(line, "C~ ") && isValidPattern(line + strlen("C~ "))) {
        return addStep(scenario, STEP_CLIENT_SENDS_MATCHING, line + strlen("C~ "));
    }
    return -1;
}

int loadScenario(const char *scenarioPath, Scenario *scenario) {
    memset(scenario, 0, sizeof *scenario);

    FILE *scenarioFile = fopen(scenarioPath, "r");
    if (scenarioFile == NULL) {
        perror(scenarioPath);
        return -1;
    }

    char line[MAX_LINE_LENGTH];
    int lineNumber = 0;
    RequiredHeaderLines headerLines = {.clientArgumentsGiven = false, .expectedResultGiven = false};
    int loadResult = 0;

    while (fgets(line, sizeof line, scenarioFile) != NULL) {
        lineNumber++;
        if (strchr(line, '\n') == NULL && !feof(scenarioFile)) {
            fprintf(stderr, "%s:%d: Zeile ist länger als %d Zeichen\n", scenarioPath, lineNumber, MAX_LINE_LENGTH - 2);
            loadResult = -1;
            break;
        }
        removeLineEnding(line);
        if (parseScenarioLine(scenario, line, &headerLines) != 0) {
            fprintf(stderr, "%s:%d: ungültige Zeile: \"%s\"\n", scenarioPath, lineNumber, line);
            loadResult = -1;
            break;
        }
    }
    fclose(scenarioFile);

    if (loadResult == 0 && !headerLines.clientArgumentsGiven) {
        fprintf(stderr, "%s: Zeile \"# args: ...\" fehlt\n", scenarioPath);
        loadResult = -1;
    }
    if (loadResult == 0 && !headerLines.expectedResultGiven) {
        fprintf(stderr, "%s: Zeile \"# expect-exit: success\" oder \"# expect-exit: failure\" fehlt\n", scenarioPath);
        loadResult = -1;
    }
    return loadResult;
}

int openListeningSocket(int port) {
    /* SOCK_CLOEXEC: gestartete Client-Prozesse sollen den Socket nicht erben. */
    int listeningSocket = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (listeningSocket < 0) {
        perror("socket");
        return -1;
    }

    int reuseAddress = 1;
    if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &reuseAddress, sizeof reuseAddress) != 0) {
        perror("setsockopt");
        close(listeningSocket);
        return -1;
    }

    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof serverAddress);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons((uint16_t) port);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(listeningSocket, (struct sockaddr *) &serverAddress, sizeof serverAddress) != 0) {
        perror("bind");
        close(listeningSocket);
        return -1;
    }
    if (listen(listeningSocket, 1) != 0) {
        perror("listen");
        close(listeningSocket);
        return -1;
    }
    return listeningSocket;
}

int acceptClient(int listeningSocket, int timeoutSeconds) {
    struct pollfd waitingSocket = {.fd = listeningSocket, .events = POLLIN};
    if (poll(&waitingSocket, 1, timeoutSeconds * 1000) <= 0) {
        return -1;
    }
    return accept(listeningSocket, NULL, NULL);
}

static void pauseBetweenChunks(void) {
    struct timespec pauseDuration = {.tv_sec = 0, .tv_nsec = PAUSE_BETWEEN_CHUNKS_MILLISECONDS * 1000000L};
    nanosleep(&pauseDuration, NULL);
}

static int sendInChunks(int clientSocket, const char *data, size_t dataLength, int chunkSize) {
    size_t sentBytes = 0;
    while (sentBytes < dataLength) {
        size_t bytesToSend = dataLength - sentBytes;
        if (chunkSize > 0 && bytesToSend > (size_t) chunkSize) {
            bytesToSend = (size_t) chunkSize;
        }
        ssize_t result = send(clientSocket, data + sentBytes, bytesToSend, MSG_NOSIGNAL);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sentBytes += (size_t) result;
        if (chunkSize > 0) {
            pauseBetweenChunks();
        }
    }
    return 0;
}

/* Sendet die Serverzeilen der Schritte firstStep bis endStep - 1 als einen Block. */
static int sendServerLines(int clientSocket, const Scenario *scenario, int firstStep, int endStep, FILE *log) {
    char data[MAX_SCENARIO_STEPS * MAX_LINE_LENGTH];
    size_t dataLength = 0;

    for (int stepIndex = firstStep; stepIndex < endStep; stepIndex++) {
        const char *serverLine = scenario->steps[stepIndex].text;
        fprintf(log, "S: %s\n", serverLine);
        dataLength += (size_t) snprintf(data + dataLength, sizeof data - dataLength, "%s\n", serverLine);
    }
    fflush(log);
    return sendInChunks(clientSocket, data, dataLength, scenario->chunkSize);
}

static ReadResult readClientLine(ClientLineReader *reader, char *line, int timeoutSeconds) {
    while (true) {
        char *newline = memchr(reader->buffer, '\n', reader->length);
        if (newline != NULL) {
            size_t lineLength = (size_t) (newline - reader->buffer);
            if (lineLength >= MAX_LINE_LENGTH) {
                return LINE_TOO_LONG;
            }
            memcpy(line, reader->buffer, lineLength);
            line[lineLength] = '\0';

            size_t remainingLength = reader->length - lineLength - 1;
            memmove(reader->buffer, newline + 1, remainingLength);
            reader->length = remainingLength;
            return LINE_RECEIVED;
        }
        if (reader->length == sizeof reader->buffer) {
            return LINE_TOO_LONG;
        }

        struct pollfd waitingSocket = {.fd = reader->socket, .events = POLLIN};
        int pollResult = poll(&waitingSocket, 1, timeoutSeconds * 1000);
        if (pollResult == 0) {
            return CLIENT_TIMED_OUT;
        }
        if (pollResult < 0) {
            if (errno == EINTR) {
                continue;
            }
            return CONNECTION_CLOSED;
        }

        ssize_t receivedBytes = recv(reader->socket, reader->buffer + reader->length,
                                     sizeof reader->buffer - reader->length, 0);
        if (receivedBytes < 0 && errno == EINTR) {
            continue;
        }
        if (receivedBytes <= 0) {
            return CONNECTION_CLOSED;
        }
        reader->length += (size_t) receivedBytes;
    }
}

static const char *describeExpectation(const ScenarioStep *step) {
    return step->kind == STEP_CLIENT_CLOSES ? "Verbindungsende" : step->text;
}

static bool clientReactionIsCorrect(const ScenarioStep *step, ReadResult readResult, const char *clientLine,
                                    int timeoutSeconds, FILE *log) {
    switch (readResult) {
    case CLIENT_TIMED_OUT:
        fprintf(log, "FEHLER: Client hat nach %d s nicht reagiert, erwartet: \"%s\"\n",
                timeoutSeconds, describeExpectation(step));
        return false;
    case LINE_TOO_LONG:
        fprintf(log, "FEHLER: Client hat eine Zeile mit mehr als %d Zeichen gesendet\n", MAX_LINE_LENGTH - 1);
        return false;
    case CONNECTION_CLOSED:
        if (step->kind == STEP_CLIENT_CLOSES) {
            fprintf(log, "Client hat die Verbindung wie erwartet geschlossen\n");
            return true;
        }
        fprintf(log, "FEHLER: Client hat die Verbindung geschlossen, erwartet: \"%s\"\n", step->text);
        return false;
    case LINE_RECEIVED:
        break;
    }

    fprintf(log, "C: %s\n", clientLine);
    if (step->kind == STEP_CLIENT_CLOSES) {
        fprintf(log, "FEHLER: Client hätte die Verbindung schließen müssen, hat aber weiter gesendet\n");
        return false;
    }

    bool lineMatches = step->kind == STEP_CLIENT_SENDS_EXACTLY
                           ? strcmp(clientLine, step->text) == 0
                           : matchesPattern(step->text, clientLine);
    if (!lineMatches) {
        fprintf(log, "FEHLER: erwartet \"%s\", erhalten \"%s\"\n", step->text, clientLine);
    }
    return lineMatches;
}

bool playScenario(int clientSocket, const Scenario *scenario, int timeoutSeconds, FILE *log) {
    ClientLineReader reader = {.socket = clientSocket, .length = 0};
    int firstUnsentServerStep = 0;
    bool clientFollowedScenario = true;

    for (int stepIndex = 0; stepIndex < scenario->stepCount && clientFollowedScenario; stepIndex++) {
        const ScenarioStep *step = &scenario->steps[stepIndex];
        if (step->kind == STEP_SERVER_SENDS) {
            continue;
        }

        char clientLine[MAX_LINE_LENGTH] = "";
        ReadResult readResult;
        if (sendServerLines(clientSocket, scenario, firstUnsentServerStep, stepIndex, log) != 0) {
            readResult = CONNECTION_CLOSED;
        } else {
            readResult = readClientLine(&reader, clientLine, timeoutSeconds);
        }
        firstUnsentServerStep = stepIndex + 1;

        clientFollowedScenario = clientReactionIsCorrect(step, readResult, clientLine, timeoutSeconds, log);
    }

    if (clientFollowedScenario
        && sendServerLines(clientSocket, scenario, firstUnsentServerStep, scenario->stepCount, log) != 0) {
        fprintf(log, "Hinweis: Client hat die Verbindung vor den letzten Serverzeilen geschlossen\n");
    }
    fflush(log);
    return clientFollowedScenario;
}
