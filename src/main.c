#include <stdio.h>
#include <stdlib.h>

#include "broadcastagent.h"
#include "connectionhandler.h"
#include "util.h"

int main(int argc, char **argv)
{
	utilInit(argv[0]);
	infoPrint("Chat server, group xy");	//TODO: Add your group number!

	//TODO: evaluate command line arguments
	//TODO: perform initialization
	if (broadcastAgentInit() == 1) {
		fprintf(stderr, "broadcastAgentInit() failed");
		return EXIT_FAILURE;
	}
	infoPrint("Chat server started");

	//TODO: use port specified via command line
	const int result = connectionHandler((in_port_t)8111);

	//TODO: perform cleanup, if required by your implementation
	return result != -1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
