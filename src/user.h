#ifndef USER_H
#define USER_H
#define MAX_NAME_LENGTH 32

#include <pthread.h>

typedef struct User
{
	struct User *prev;
	struct User *next;
	pthread_t thread;	//thread ID of the client thread
	int sock;		//socket for client
	char name[MAX_NAME_LENGTH]; //Name des Nutzers
} User;

//TODO: Add prototypes for functions that fulfill the following tasks:
// * Add a new user to the list and start client thread
// * Iterate over the complete list (to send messages to all users)
// * Remove a user from the list
//CAUTION: You will need proper locking!
User * addUser(User *user);
User * removeUser(User *user);
void iterateUser(User *ignoreUser, void (*function)(User *user, void *arg), void *arg);

#endif
