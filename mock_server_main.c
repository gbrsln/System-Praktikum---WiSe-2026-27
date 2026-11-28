#include "mock_server.h"

#include <stdlib.h>
#include <unistd.h>

#define WAIT_FOR_CLIENT_SECONDS 60
#define CLIENT_TIMEOUT_SECONDS 30

static void printMockServerUsage(const char *programName) {
    fprintf(stderr, "Aufruf: %s -s <Szenario-Datei>\n", programName);
    fprintf(stderr, "Wartet auf Port %d auf einen Client, z. B. ./sysprak-client-mock -g <GAME-ID>\n", MOCK_PORT);
}

int main(int argc, char *argv[]) {
    const char *scenarioPath = NULL;
    int option;
    opterr = 0;
    while ((option = getopt(argc, argv, "s:")) != -1) {
        if (option != 's') {
            printMockServerUsage(argv[0]);
            return EXIT_FAILURE;
        }
        scenarioPath = optarg;
    }
    if (scenarioPath == NULL || optind < argc) {
        printMockServerUsage(argv[0]);
        return EXIT_FAILURE;
    }

    Scenario scenario;
    if (loadScenario(scenarioPath, &scenario) != 0) {
        return EXIT_FAILURE;
    }

    int listeningSocket = openListeningSocket(MOCK_PORT);
    if (listeningSocket < 0) {
        return EXIT_FAILURE;
    }
    printf("Mock-Server wartet auf Port %d (Szenario %s)\n", MOCK_PORT, scenarioPath);
    fflush(stdout);

    int clientSocket = acceptClient(listeningSocket, WAIT_FOR_CLIENT_SECONDS);
    close(listeningSocket);
    if (clientSocket < 0) {
        fprintf(stderr, "Kein Client hat sich innerhalb von %d s verbunden\n", WAIT_FOR_CLIENT_SECONDS);
        return EXIT_FAILURE;
    }

    bool clientFollowedScenario = playScenario(clientSocket, &scenario, CLIENT_TIMEOUT_SECONDS, stdout);
    close(clientSocket);

    printf(clientFollowedScenario ? "Szenario vollständig durchlaufen\n" : "Szenario abgebrochen\n");
    return clientFollowedScenario ? EXIT_SUCCESS : EXIT_FAILURE;
}
