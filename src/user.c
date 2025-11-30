#include <pthread.h>
#include "user.h"

#include <stdio.h>
#include <stdlib.h>

#include "clientthread.h"

static pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;
static User *userFront = NULL;
static User *userBack = NULL;

//TODO: Implement the functions declared in user.h

User *addUser(User *user) {
    if (user == NULL) return NULL;
    pthread_mutex_lock(&userLock);
    user->next = userFront;
    user->prev = NULL;
    if (userFront != NULL) {
        userFront->prev = user;
    }
    userFront = user;
    pthread_mutex_unlock(&userLock);
    return user;
}

User *removeUser(User *user) {
    pthread_mutex_lock(&userLock);
    User *temp = userFront;
    if (userFront == NULL) {
        pthread_mutex_unlock(&userLock);
        return NULL;
    }
    while (temp != NULL) {
        if (temp->thread == user->thread && temp->sock == user->sock) {
            // Fall: User ist ganz vorne (oder der Einzige)
            if (temp->prev == NULL) {
                userFront = temp->next; // Der Nächste rückt nach

                if (userFront != NULL) {
                    // Wenn noch jemand da ist: Dessen Rückspiegel auf NULL setzen
                    userFront->prev = NULL;
                } else {
                    // WICHTIG: Wenn userFront jetzt NULL ist, ist die Liste leer!
                    // Dann muss auch der Hintereingang (userBack) zugemacht werden.
                    userBack = NULL;
                }

                free(temp);
                pthread_mutex_unlock(&userLock); // Nicht vergessen: Aufschließen!
                return user;
            }
            if (temp->next == NULL) {
                userBack = temp->prev;
                userBack->next = NULL;
                free(temp);
                pthread_mutex_unlock(&userLock);
                return user;
            }
            temp->prev->next = temp->next;
            temp->next->prev = temp->prev;
            pthread_mutex_unlock(&userLock);
            free(temp);
            return user;
        }
        temp = temp->next;
    }
    pthread_mutex_unlock(&userLock);
    return user;
}

/*void iterateUser(User *ignoreUser, void (*function)(User *user, void *arg), void *arg) {
    pthread_mutex_lock(&userLock);
    User *temp = userFront;
    while (temp != NULL) {
        User *nextBackup = temp->next;
        if (temp != ignoreUser) {
            function(temp, arg); //nur funktionen die nicht locken!
        }
        temp = nextBackup;
    }
    pthread_mutex_unlock(&userLock);
}*/

// In user.c

void iterateUser(User *ignoreUser, void (*function)(User *user, void *arg), void *arg) {
    pthread_mutex_lock(&userLock);

    User *temp = userFront;

    // DEBUG 1: Wo startet die Liste?
    if (temp == NULL) {
        printf("DEBUG: FEHLER! Die Liste ist komplett leer (userFront ist NULL)!\n");
    } else {
        printf("DEBUG: Starte Iteration. Ich bin User Socket %d.\n", ignoreUser->sock);
    }

    int count = 0;
    while (temp != NULL) {
        User *nextBackup = temp->next;
        count++;

        printf("DEBUG: Prüfe Listen-Eintrag %d (Socket %d)... ", count, temp->sock);

        if (temp != ignoreUser) {
            printf("Treffer! Rufe sendMessage auf.\n");
            function(temp, arg);
        } else {
            printf("Das bin ich selbst (überspringen).\n");
        }

        temp = nextBackup;
    }

    printf("DEBUG: Iteration fertig. %d User gefunden.\n", count);
    pthread_mutex_unlock(&userLock);
}