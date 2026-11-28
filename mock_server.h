#ifndef MOCK_SERVER_H
#define MOCK_SERVER_H

#include <stdbool.h>
#include <stdio.h>

#ifndef MOCK_PORT
#define MOCK_PORT 13570
#endif

#define MAX_SCENARIO_STEPS 64
#define MAX_LINE_LENGTH 256

typedef enum {
    STEP_SERVER_SENDS,
    STEP_CLIENT_SENDS_EXACTLY,
    STEP_CLIENT_SENDS_MATCHING,
    STEP_CLIENT_CLOSES
} StepKind;

typedef struct {
    StepKind kind;
    char text[MAX_LINE_LENGTH];
} ScenarioStep;

typedef struct {
    ScenarioStep steps[MAX_SCENARIO_STEPS];
    int stepCount;
    char clientArguments[MAX_LINE_LENGTH];
    bool clientShouldSucceed;
    int chunkSize;
} Scenario;

/* Gibt 0 zurück, bei einer fehlerhaften Szenario-Datei eine Meldung auf stderr und -1. */
int loadScenario(const char *scenarioPath, Scenario *scenario);

/* Gibt den Socket zurück, bei einem Fehler eine Meldung auf stderr und -1. */
int openListeningSocket(int port);

/* Gibt den Socket des Clients zurück, oder -1, wenn sich innerhalb der Zeit kein Client verbindet. */
int acceptClient(int listeningSocket, int timeoutSeconds);

/* Schreibt den Verlauf nach log; true, wenn sich der Client genau an das Szenario gehalten hat. */
bool playScenario(int clientSocket, const Scenario *scenario, int timeoutSeconds, FILE *log);

#endif
