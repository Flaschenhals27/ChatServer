#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "connectionhandler.h"
#include "util.h"
#include "broadcastagent.h"

#define DEFAULT_PORT 8111

volatile sig_atomic_t serverRunning = 1;

void handleSignal(int sig) {
    (void)sig;
    serverRunning = 0;
}

int main(const int argc, char **argv) {
    //- Signal abfangen (Strg+C) aktivieren -//
    struct sigaction sa = {0};
    sa.sa_handler = handleSignal;
    sigaction(SIGINT, &sa, NULL);

    utilInit(argv[0]);
    debugEnable();
    infoPrint("Chat server, group 27");

    in_port_t port = DEFAULT_PORT;
    if (argc == 2) {
        //--- Infos anfragen ---//
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            infoPrint("Usage: %s [PORT]", argv[0]);
            return EXIT_SUCCESS;
        }
        //Todo
        //--- Port überprüfen und setzen ---//
        if (atoi(argv[1]) >= 65536 || atoi(argv[1]) <= 1023) {
            port = 8111;
            fprintf(stderr, "Port must be <65536 and >1023, using standard port\n");
        } else port = (in_port_t) atoi(argv[1]);
        if (port == 0) {
            fprintf(stderr, "Invalid port number: %s\n", argv[1]);
            return EXIT_FAILURE; //Fehlercode 1
        }

    //--- Zu viele Argumente? ---//
    } else if (argc > 2) {
        fprintf(stderr, "Usage: %s [PORT]\n", argv[0]);
        return EXIT_FAILURE; //Fehlercode 1
    }

    //--- Startet den Broadcasts Agent ---//
    if (broadcastAgentInit() == -1) {
        fprintf(stderr, "broadcastAgentInit() failed\n");
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Starting server on port %u\n", port);
    const int result = connectionHandler(port);
    broadcastAgentCleanup();

    //Für Linux übersetzt:
    //Kein Fehler? != -1 ? -> gib 0 zurück
    //Doch ein Fehler -1?  -> gib 1 zurück (Fehler)
    return result != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
