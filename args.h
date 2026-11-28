#ifndef ARGS_H
#define ARGS_H

#define GAME_ID_LENGTH 13
#define NO_PLAYER_REQUESTED 0

typedef struct {
    char gameId[GAME_ID_LENGTH + 1];
    int requestedPlayerNumber;
} CommandLineArguments;

/* Gibt 0 zurück, bei ungültigen Argumenten eine Fehlermeldung auf stderr und -1. */
int parseCommandLineArguments(int argc, char *argv[], CommandLineArguments *arguments);

void printUsage(const char *programName);

#endif
