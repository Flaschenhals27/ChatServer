#ifndef BROADCASTAGENT_H
#define BROADCASTAGENT_H
#include "network.h" // Daher kommt InternalMessage

int broadcastAgentInit(void);

void broadcastAgentCleanup(void);
// Es wird jetzt InternalMessage übergeben
int broadcastQueueSend(const InternalMessage *msg);

int broadcastStop(void);

int broadcastResume(void);

#endif
