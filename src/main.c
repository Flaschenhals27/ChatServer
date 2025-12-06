#include <stdio.h>
#include <stdlib.h>

#include "broadcastagent.h"
#include "connectionhandler.h"
#include "util.h"

int main(int argc, char **argv)
{
	utilInit(argv[0]);
	infoPrint("Chat server, group 27");

	in_port_t port = 8111;

	if (argc > 1) {
		int p = atoi(argv[1]);

		if (p > 0 && p <= 65535) {
			port = (in_port_t)p;
			infoPrint("Server läuft unter Port %d", p);
		} else errorPrint("Ungültiger Port, nutze Port 8111");
	} else infoPrint("Server läuft auf Port 8111");

	if (broadcastAgentInit() == 1) {
		errorPrint("broadcastAgentInit() failed");
		return EXIT_FAILURE;
	}
	infoPrint("Chat server started");

	const int result = connectionHandler(port);

	//TODO: perform cleanup, if required by your implementation
	return result != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
