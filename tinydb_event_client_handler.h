/**
 * TinyDB Event-based Client Handler
 * Handles client connections using the event-based TCP server
 */
#ifndef __TINY_DB_EVENT_CLIENT_HANDLER_H
#define __TINY_DB_EVENT_CLIENT_HANDLER_H

#include "tinydb_event_tcp_server.h"

bool
EventClientHandler_Init();

void
EventClientHandler_Cleanup();

void
EventClientHandler_OnConnect(EventTcpClient* client);

void
EventClientHandler_OnData(EventTcpClient* client);

void
EventClientHandler_OnDisconnect(EventTcpClient* client);

#endif // __TINY_DB_EVENT_CLIENT_HANDLER_H