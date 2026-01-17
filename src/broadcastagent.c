#include <pthread.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>

#include "broadcastagent.h"

#include <string.h>

#include "util.h"
#include "user.h"
#include "network.h"

#define QUEUE_NAME "/chat-group27-proto-v2"

static mqd_t messageQueue;
static pthread_t threadId; // Hier Nachricht speichern, die gerade an alle verteilt wird
static InternalMessage g_current_msg;
static sem_t pauseSem;

//--- Verschiedene Nachrichtentypen an den Client uebermitteln ---//
static void send_to_user(User *user) {

    //- User ohne Namen (Zb Netcat-lauscher) erhalten keine Nachricht -//
    if (strlen(user->name) == 0) {
        return;
    }

    //- Wenn User gekickt wird, darf er Nachricht nicht selber erhalten! -//
    if (g_current_msg.type == MT_USER_REMOVED) {
        if (strncmp(g_current_msg.data.urm.name, user->name, 32) == 0) {
            return;
        }
    }

    switch (g_current_msg.type) {
        case MT_SERVER_TO_CLIENT:
            //- Parameter: File Descriptor, Sender, Textnachricht, Timestamp -//
            sendServer2Client(user->sock,
                              g_current_msg.data.s2c.original_sender,
                              g_current_msg.data.s2c.text,
                              g_current_msg.data.s2c.timestamp);
            break;

        case MT_USER_ADDED:
            //- Parameter: File Descriptor, Name des Benutzers, Timestamp -//
            sendUserAdded(user->sock,
                          g_current_msg.data.uad.name,
                          g_current_msg.data.uad.timestamp);
            break;

        case MT_USER_REMOVED:
            //- Parameter: File Descriptor, Name des Benutzers, Grund des Entfernens, Timestamp -//
            sendUserRemoved(user->sock,
                            g_current_msg.data.urm.name,
                            g_current_msg.data.urm.code,
                            g_current_msg.data.urm.timestamp);
        default: ;
    }
}

//--- Wartet auf neue Nachrichten und verteilt diese anschliessend ---//
//- void* name(void *arg) wird von POSIX so vorgegeben, koennte ein Ergebnis nach Beendigung zurueckliefern -//
static void *broadcastAgent(void *arg) {
    (void) arg; //- Argumente werden nicht benoetigt, in void casten um Compiler zufrieden zustellen -//
    debugPrint("BroadcastAgent thread started\n");

    while (1) {
        InternalMessage msg;
        ssize_t bytes_read = mq_receive(messageQueue, (char *) &msg, sizeof(InternalMessage), NULL);

        if (bytes_read < 0) {
            if (errno == EINTR) continue; //- System hat Thread kurz angestupst durch ein Signal zb, kein echter Fehler -//
            errnoPrint("mq_receive failed");
            break; //- Thread beenden -//
        }

        //- Wenn ein nicht vollstaendiges Paket empfangen wird, Paket verwerfen und weitermachen -//
        if (bytes_read != sizeof(InternalMessage)) {
            errnoPrint("Received message with unexpected size from queue");
            continue;
        }

        //- Falls Systemnachricht -> muss direkt gesendet werden -//
        int is_system_msg = 0;
        if (msg.type == MT_SERVER_TO_CLIENT) {
            if (msg.data.s2c.original_sender[0] == '\0') { //- Systemnachrichten haben keinen Absender, also nix drin -//
                is_system_msg = 1;
            }
        }

        else if (msg.type == MT_USER_REMOVED || msg.type == MT_USER_ADDED) { //- Auch Abmeldungen und neue User sollen sofort gemeldet werden -//
            is_system_msg = 1;
        }

        if (is_system_msg) {
            //- Semaphor nicht pruefen ob Server pausiert ist -//
        } else {
            //- Versuchen den Semaphor zu sperren. Falls Server pausiert ist, klemmen hier die Nachrichten fest. -//
            sem_wait(&pauseSem);
            sem_post(&pauseSem);
        }

        //- Nachrichten Verteilung durchfuehren -//
        g_current_msg = msg;
        user_iterate(send_to_user);
    }
    return NULL;
}

//--- Bereitet die Queue, Semaphore und den Thread vor ---//
int broadcastAgentInit(void) {
    struct mq_attr attr;
    attr.mq_flags = 0; //- 0, damit blockierend bei leerer oder voller Queue -//
    attr.mq_maxmsg = 10; //- Maximal 10 Nachrichten in der Queue halten -//
    attr.mq_msgsize = sizeof(InternalMessage);
    attr.mq_curmsgs = 0; //- 0 Nachrichen bei Start in der Queue, mq_open ignoriert das -//

    //- Sauberes starten der Queue -//
    mq_unlink(QUEUE_NAME);
    messageQueue = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);

    if (messageQueue == -1) {
        errnoPrint("Failed to create message queue");
        return -1;
    }

    //- Parameter: PauseSem wird nicht geteilt mit anderen Prozessen (=0) und ist standardmaessig aktiv -//
    if (sem_init(&pauseSem, 0, 1) == -1) {
        errnoPrint("Failed to init semaphore");
        return -1;
    }

    //- Parameter: ThreadId erhaelt keine besondere Prioritaet, Thread fuehrt broadcastAgent aus und uebergibt keine Parameter -//
    if (pthread_create(&threadId, NULL, broadcastAgent, NULL) != 0) {
        errnoPrint("Failed to start broadcast agent thread");
        mq_close(messageQueue);
        mq_unlink(QUEUE_NAME);
        return -1;
    }
    return 0;
}

//--- Thread, Queue und Semaphor schliessen ---//
void broadcastAgentCleanup(void) {
    pthread_cancel(threadId);
    pthread_join(threadId, NULL); //- join laesst den aktuellen Prozess immer auf den darin angegebenen warten, hier also warten bis er wirklich tot ist -//
    mq_close(messageQueue);
    mq_unlink(QUEUE_NAME);
    sem_destroy(&pauseSem);
}

int broadcastStop(void) {
    //- Versuche den Semaphor auf 0 zu setzen -//
    if (sem_trywait(&pauseSem) == 0) {
        infoPrint("Server is now paused.");
        return 0;
    }
    //- Wenn Semaphor vorher schon 0 war, ist errno gesetzt worden. Bei EAGAIN war er einfach schon 0, sonst Fehler! -//
    if (errno == EAGAIN) {
        infoPrint("Server was already paused.");
        return -1;
    }
    errnoPrint("Semaphor error");
    return -1;
}

int broadcastResume(void) {
    int val;
    sem_getvalue(&pauseSem, &val);

    //- Pruefen ob Semaphor auf 0 steht, sonst nix machen -//

    if (val > 0) {
        return -1;
    }

    if (val == 0) {
        if (sem_post(&pauseSem) != 0) {
            errorPrint("Resuming chat caused error");
            return -1;
        }
    }
    infoPrint("Resuming chat was successful.");
    return 0;
}

//--- Verteilt die Nachrichten an alle Clients ---//
int broadcastQueueSend(const InternalMessage *msg) {
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    //- Eine Sekunde bei einer vollen Queue warten. Wenn immer noch voll -> verwerfen
    tm.tv_sec += 1;

    if (mq_timedsend(messageQueue, (const char *) msg, sizeof(InternalMessage), 0, &tm) == -1) {
        if (errno == ETIMEDOUT) {
            errorPrint("Broadcast queue full, message dropped.");
            return -1;
        }
        errnoPrint("mq_send failed");
        return -1;
    }
    return 0;
}