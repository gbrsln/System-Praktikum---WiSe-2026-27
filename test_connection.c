#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "connection.h"

int main(void) {
    int sockfd = connectToGameserver();

    if(sockfd == -1) {
        fprintf(stderr, "Verbindungsaufbau ist fehlgeschlagen.\n");
        return EXIT_FAILURE;
    }

    printf("Socket-File-Descriptor: %d\n", sockfd);

    if(close(sockfd) == -1) {
        perror("close");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}