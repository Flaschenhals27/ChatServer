#include <errno.h>
#include "connectionhandler.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "clientthread.h"
#include "user.h"
#include "util.h"

static int createPassiveSocket(in_port_t port)
{
	int fd = -1;
	//TODO: socket()
	fd = socket(AF_INET, SOCK_STREAM, 0); //IPv4, TCP, Standard
	if (fd == -1) {
		return -1;
	}

	//Einstellung für den Socket
	const int on = 1; //1=an 0=aus
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
		perror("setsockopt");
		close(fd);
		return -1;
	}

	//TODO: bind() to port
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY; //Alle Interfaces abhören
	addr.sin_port = htons(port); //Schreibt den Port in Big Endian, PC würde Little Endian nutzen
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		return -1;
	}

	//TODO: listen()
	if (listen(fd, 10) == -1) { //Maximale Warteschlange für Verbindungen, also maximal 10 dürfen warten
		return -1;
	}

	return fd;
}

int connectionHandler(in_port_t port)
{
	const int fd = createPassiveSocket(port);
	if(fd == -1)
	{
		errnoPrint("Unable to create server socket");
		return -1;
	}

	while(1)
	{
		//TODO: accept() incoming connection
		struct sockaddr_in clientAddr;
		socklen_t clientAddrLen = sizeof(clientAddr);
		int client_fd = accept(fd, (struct sockaddr *)&clientAddr, &clientAddrLen);
		if (client_fd == -1) {
			continue;
		}
		//TODO: add connection to user list and start client thread
		User *newUser = malloc(sizeof(User));
		if (newUser == NULL) {
			errnoPrint("Unable to allocate memory for new user");
			close(fd);
			continue;
		}

		newUser->sock = client_fd;
		newUser->next = NULL;
		newUser->prev = NULL;
		addUser(newUser);

		if (pthread_create(&newUser->thread, NULL, clientthread, newUser) != 0) {
			errnoPrint("pthread_create failed");
			removeUser(newUser);
			close(client_fd);
			continue;
		}
		pthread_detach(newUser->thread); //BS räumt fertigen Prozess weg ohne zu warten
	}
	return 0;
}