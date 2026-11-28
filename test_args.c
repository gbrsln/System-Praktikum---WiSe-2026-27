#include "../args.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define ARGUMENT_COUNT(argumentArray) ((int) (sizeof(argumentArray) / sizeof((argumentArray)[0])))

static int failedChecks = 0;
static int passedChecks = 0;

static void check(bool condition, const char *description) {
    if (condition) {
        passedChecks++;
    } else {
        failedChecks++;
        printf("FEHLGESCHLAGEN: %s\n", description);
    }
}

static void testValidGameIdOnly(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij123"};
    CommandLineArguments arguments;
    int result = parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments);
    check(result == 0, "nur -g wird akzeptiert");
    check(strcmp(arguments.gameId, "abcdefghij123") == 0, "Game-ID wird übernommen");
    check(arguments.requestedPlayerNumber == NO_PLAYER_REQUESTED, "ohne -p kein Spielerwunsch");
}

static void testGameIdAndPlayerNumber(void) {
    char *argv[] = {"sysprak-client", "-g", "ABCDEFGHIJ123", "-p", "2"};
    CommandLineArguments arguments;
    int result = parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments);
    check(result == 0, "-g und -p werden akzeptiert");
    check(strcmp(arguments.gameId, "ABCDEFGHIJ123") == 0, "Game-ID mit Großbuchstaben wird übernommen");
    check(arguments.requestedPlayerNumber == 2, "-p 2 wird zu Spielernummer 2");
}

static void testOptionsInOtherOrder(void) {
    char *argv[] = {"sysprak-client", "-p", "1", "-g", "0123456789abc"};
    CommandLineArguments arguments;
    int result = parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments);
    check(result == 0, "Reihenfolge der Optionen ist egal");
    check(arguments.requestedPlayerNumber == 1, "-p 1 wird zu Spielernummer 1");
}

static void testMissingGameId(void) {
    char *argv[] = {"sysprak-client", "-p", "1"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "fehlende Game-ID wird abgelehnt");
}

static void testGameIdTooShort(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij12"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "Game-ID mit 12 Zeichen wird abgelehnt");
}

static void testGameIdTooLong(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij1234"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "Game-ID mit 14 Zeichen wird abgelehnt");
}

static void testGameIdWithInvalidCharacter(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij-12"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "Game-ID mit Bindestrich wird abgelehnt");
}

static void testInvalidPlayerNumbers(void) {
    char *argvThree[] = {"sysprak-client", "-g", "abcdefghij123", "-p", "3"};
    char *argvZero[] = {"sysprak-client", "-g", "abcdefghij123", "-p", "0"};
    char *argvText[] = {"sysprak-client", "-g", "abcdefghij123", "-p", "eins"};
    char *argvWithSuffix[] = {"sysprak-client", "-g", "abcdefghij123", "-p", "1x"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvThree), argvThree, &arguments) == -1, "-p 3 wird abgelehnt");
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvZero), argvZero, &arguments) == -1, "-p 0 wird abgelehnt");
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvText), argvText, &arguments) == -1, "-p eins wird abgelehnt");
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvWithSuffix), argvWithSuffix, &arguments) == -1, "-p 1x wird abgelehnt");
}

static void testOptionsWithoutValue(void) {
    char *argvPlayerWithoutValue[] = {"sysprak-client", "-g", "abcdefghij123", "-p"};
    char *argvGameIdWithoutValue[] = {"sysprak-client", "-g"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvPlayerWithoutValue), argvPlayerWithoutValue, &arguments) == -1,
          "-p ohne Wert wird abgelehnt");
    check(parseCommandLineArguments(ARGUMENT_COUNT(argvGameIdWithoutValue), argvGameIdWithoutValue, &arguments) == -1,
          "-g ohne Wert wird abgelehnt");
}

static void testUnknownOption(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij123", "-x"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "unbekannte Option -x wird abgelehnt");
}

static void testUnexpectedArgument(void) {
    char *argv[] = {"sysprak-client", "-g", "abcdefghij123", "zusatz"};
    CommandLineArguments arguments;
    check(parseCommandLineArguments(ARGUMENT_COUNT(argv), argv, &arguments) == -1, "zusätzliches Argument wird abgelehnt");
}

int main(void) {
    /* Die erwarteten Fehlermeldungen des Parsers würden die Testausgabe überdecken. */
    if (freopen("/dev/null", "w", stderr) == NULL) {
        perror("freopen");
        return 1;
    }

    testValidGameIdOnly();
    testGameIdAndPlayerNumber();
    testOptionsInOtherOrder();
    testMissingGameId();
    testGameIdTooShort();
    testGameIdTooLong();
    testGameIdWithInvalidCharacter();
    testInvalidPlayerNumbers();
    testOptionsWithoutValue();
    testUnknownOption();
    testUnexpectedArgument();

    printf("Kommandozeile: %d Prüfungen bestanden, %d fehlgeschlagen\n", passedChecks, failedChecks);
    return failedChecks == 0 ? 0 : 1;
}
