#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "connectionhandler.h"

#include <signal.h>
#include <stdbool.h>

#include "clientthread.h"
#include "user.h"
#include "util.h"

//- Variable von main.c -//
extern volatile sig_atomic_t serverRunning;

//--- Erstellt den Socket und Filedesriptor ---//
static int createPassiveSocket(const in_port_t port) {
    //- Parameter: IPv4, TCP, Standard -//
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        errnoPrint("socket");
        return -1;
    }

    //- Parameter: File Descriptor, Level = Socket, Reuse Address: Adresse sofort wiederverwenden, Option = anschalten
    const int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        errnoPrint("setsockopt");
        close(fd);
        return -1;
    }

    //- Socket Adresse initialisieren und binden --/
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == -1) {
        errnoPrint("bind");
        close(fd);
        return -1;
    }

    //- Maximal 10 User duerfen warten um reingelassen zu werden -//
    if (listen(fd, 10) == -1) {
        errnoPrint("listen");
        close(fd);
        return -1;
    }
    //- File Descriptor zurueckgeben fuer den ConnectionHandler -//
    return fd;
}

int connectionHandler(const in_port_t port) {
    const int fd = createPassiveSocket(port);
    if (fd == -1) {
        errnoPrint("Unable to create server socket");
        return -1;
    }
    while (serverRunning) {
        //- mit accept koennte die IP des Nutzers abgefragt werden, aber kein Interrese -//
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno == EINTR) { //- Nur Signal? -//
                continue;
            }
            errnoPrint("accept");
            continue;;
        }
        fprintf(stderr, "Accepted new connection (fd=%d)\n", client_fd);

        //- User erstellen und den Filedescriptor abspeichern darin -//
        User *newUser = user_add(client_fd);
        if (newUser == NULL) {
            fprintf(stderr, "Unable to add new user\n");
            close(client_fd);
            continue;
        }

        //- Thread erstellen und an clientthread die Arbeit abgeben -//
        pthread_t thread;
        if (pthread_create(&thread, NULL, clientthread, newUser) != 0) {
            fprintf(stderr, "Failed to create client thread\n");
            user_remove(newUser); //Clean up
            close(client_fd);
            continue;
        }
        pthread_detach(thread);
    }
    return 0; //- never reached -//
}
