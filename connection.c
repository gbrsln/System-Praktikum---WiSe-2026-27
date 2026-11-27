#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <netdb.h>
#include <unistd.h>

#include "connection.h"
#include "constants.h"

int connectToGameserver(void) {
    // Datenstrukturen initialisieren:
    struct addrinfo hints = {0};
    struct addrinfo *result;
    struct addrinfo *rp;

    int sockfd = -1;
    int status;
    char port [6];

    snprintf(port, sizeof(port), "%d", PORTNUMBER);

    // Anforderungen an die Verbindung definieren:
    hints.ai_family   = AF_UNSPEC; // AF_UNSPEC laesst sowhol IPv4 als auch IPv6 zu
    hints.ai_socktype = SOCK_STREAM; // Dieses Protokoll gilt eine stromorientoierte Kommunikation
    hints.ai_protocol = IPPROTO_TCP; // TCP als Transportprotokoll verwenden

    //Hostname auflösen:
    status = getaddrinfo(HOSTNAME, port, &hints, &result);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));

        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        // Socket erzeugen:
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol );

        if (sockfd == -1) {
            perror("socket");
            continue;
        }

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }

        perror("connect"); // Fehlerbehandlung bei fehlgeschlagener Verbindung

        if (close(sockfd) == -1) {
            perror("close");
        }

        sockfd = -1;
    }

    //Reservierte Adressinformationen freigeben:
    freeaddrinfo(result);

    // Überprüfen, ob die Verbindung erfolgreich war:
    if (sockfd == -1) {
        fprintf(stderr, "Verbindung zum Gameserver ist fehlgeschlagen!\n");
        return -1;
    }

    printf("Verbindung zum Gameserver hergestellt.\n");
    printf("Hostname: %s\n", HOSTNAME);
    printf("Port: %d\n", PORTNUMBER);

    return sockfd; // Socket-File-Descriptor zurückgeben
}