#include <pthread.h>
#include "user.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//--- Fügt einen neuen User hinzu ---//
User *user_add(const int client_fd) {
    User *newUser = calloc(1, sizeof(User)); //- Calloc reinigt den Speicher, verhindert somit Zombieuser
    if (newUser == NULL) {
        fprintf(stderr, "Memory allocation failed for new user\n");
        return NULL;
    }

    newUser->sock = client_fd;
    newUser->prev = NULL;
    newUser->next = NULL;

    pthread_mutex_lock(&userLock);

    if (userBack == NULL) {
        userFront = newUser;
        userBack = newUser;
    } else {
        userBack->next = newUser;
        newUser->prev = userBack;
        userBack = newUser;
    }

    pthread_mutex_unlock(&userLock);
    return newUser;
}

//--- Loescht einen User ---//
void user_remove(User *user) {
    pthread_mutex_lock(&userLock);

    if (user == NULL) {
        pthread_mutex_unlock(&userLock);
        return;
    }
    if (user->prev == NULL) {
        userFront = user->next;
    } else {
        user->prev->next = user->next;
    }
    if (user->next == NULL) {
        userBack = user->prev;
    } else {
        user->next->prev = user->prev;
    }

    pthread_mutex_unlock(&userLock);
    close(user->sock);
    free(user);
}

void user_iterate(void (*func)(User *)) {
    pthread_mutex_lock(&userLock);

    User *current = userFront;
    while (current != NULL) {
        func(current);
        current = current->next;
    }

    pthread_mutex_unlock(&userLock);
}

User *user_find(const char *name) {
    pthread_mutex_lock(&userLock);

    User *current = userFront;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            pthread_mutex_unlock(&userLock);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&userLock);
    return NULL;
}
