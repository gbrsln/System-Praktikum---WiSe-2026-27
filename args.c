#include "args.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool isValidGameId(const char *gameId) {
    if (strlen(gameId) != GAME_ID_LENGTH) {
        return false;
    }
    for (size_t position = 0; position < GAME_ID_LENGTH; position++) {
        if (!isalnum((unsigned char) gameId[position])) {
            return false;
        }
    }
    return true;
}

static int parsePlayerNumber(const char *text, int *playerNumber) {
    if (strcmp(text, "1") == 0) {
        *playerNumber = 1;
        return 0;
    }
    if (strcmp(text, "2") == 0) {
        *playerNumber = 2;
        return 0;
    }
    return -1;
}

void printUsage(const char *programName) {
    fprintf(stderr, "Aufruf: %s -g <GAME-ID> [-p <1|2>]\n", programName);
    fprintf(stderr, "  -g  Game-ID aus dem Webinterface (%d Buchstaben oder Ziffern)\n", GAME_ID_LENGTH);
    fprintf(stderr, "  -p  gewünschte Spielernummer (ohne -p teilt der Server einen freien Spieler zu)\n");
}

int parseCommandLineArguments(int argc, char *argv[], CommandLineArguments *arguments) {
    arguments->gameId[0] = '\0';
    arguments->requestedPlayerNumber = NO_PLAYER_REQUESTED;

    bool gameIdGiven = false;
    int option;

    opterr = 0;
    optind = 1;
    /* Der führende ':' lässt getopt einen fehlenden Wert als ':' statt als '?' melden. */
    while ((option = getopt(argc, argv, ":g:p:")) != -1) {
        switch (option) {
        case 'g':
            if (!isValidGameId(optarg)) {
                fprintf(stderr, "Fehler: Die Game-ID muss aus genau %d Buchstaben oder Ziffern bestehen, erhalten: \"%s\".\n",
                        GAME_ID_LENGTH, optarg);
                return -1;
            }
            memcpy(arguments->gameId, optarg, GAME_ID_LENGTH + 1);
            gameIdGiven = true;
            break;
        case 'p':
            if (parsePlayerNumber(optarg, &arguments->requestedPlayerNumber) != 0) {
                fprintf(stderr, "Fehler: Die Spielernummer muss 1 oder 2 sein, erhalten: \"%s\".\n", optarg);
                return -1;
            }
            break;
        case ':':
            fprintf(stderr, "Fehler: Die Option -%c braucht einen Wert.\n", optopt);
            return -1;
        default:
            fprintf(stderr, "Fehler: Unbekannte Option -%c.\n", optopt);
            return -1;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Fehler: Unerwartetes Argument \"%s\".\n", argv[optind]);
        return -1;
    }
    if (!gameIdGiven) {
        fprintf(stderr, "Fehler: Die Game-ID fehlt (-g <GAME-ID>).\n");
        return -1;
    }
    return 0;
}
