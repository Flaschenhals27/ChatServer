#ifndef USER_H
#define USER_H

#include <pthread.h>

typedef struct User {
    struct User *prev;
    struct User *next;
    pthread_t thread; //thread ID of the client thread
    int sock; //socket for client
    int closeReason;

    char name[32];
} User;

User *user_add(int client_fd);

void user_remove(User *user);

void user_iterate(void (*func)(User *));

User *user_find(const char *name);

#endif
