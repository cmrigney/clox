#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <stdbool.h>
#include "../clox.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#define MAX_PENDING_CLIENTS 5
#define MAX_PACKETS 5
#define POLL_TIME_S 10
#define DEBUG_printf

typedef struct TCP_SERVER_SOCKET_PICO_ TCP_SERVER_SOCKET_PICO_;

typedef struct TCP_CLIENT_SOCKET_PICO_ {
  TCP_SERVER_SOCKET_PICO_ *server; // If NULL, then also designates owned by VM
  struct tcp_pcb *client_pcb;
  struct pbuf *recv_packet[MAX_PACKETS];
  bool is_closed;
} TCP_CLIENT_SOCKET_PICO;

typedef struct TCP_SERVER_SOCKET_PICO_ {
  struct tcp_pcb *server_pcb;
  TCP_CLIENT_SOCKET_PICO *clients[MAX_PENDING_CLIENTS];
  bool is_closed;
} TCP_SERVER_SOCKET_PICO;

static void dispose_server_socket(void *data) {
  FREE(TCP_SERVER_SOCKET_PICO, data);
}
static void dispose_client_socket(void *data) {
  FREE(TCP_CLIENT_SOCKET_PICO, data);
}

static int find_free_packet_slot(TCP_CLIENT_SOCKET_PICO *client) {
  cyw43_arch_lwip_check();
  for(int i = 0; i < MAX_PACKETS; i++) {
    if(client->recv_packet[i] == NULL) {
      return i;
    }
  }
  return -1;
}

static struct pbuf *get_packet(TCP_CLIENT_SOCKET_PICO *client) {
  cyw43_arch_lwip_check();
  struct pbuf *first = client->recv_packet[0];
  // shift left
  for(int i = 0; i < MAX_PACKETS - 1; i++) {
    client->recv_packet[i] = client->recv_packet[i+1];
  }
  client->recv_packet[MAX_PACKETS - 1] = NULL;
  return first;
}

static int find_first_free_client_slot(TCP_SERVER_SOCKET_PICO *server) {
  cyw43_arch_lwip_check();
  for(int i = 0; i < MAX_PENDING_CLIENTS; i++) {
    if(server->clients[i] == NULL) {
      return i;
    }
  }
  return -1;
}

static int find_first_client(TCP_SERVER_SOCKET_PICO *server) {
  cyw43_arch_lwip_check();
  for(int i = 0; i < MAX_PENDING_CLIENTS; i++) {
    if(server->clients[i] != NULL) {
      return i;
    }
  }
  return -1;
}

static void remove_client(TCP_SERVER_SOCKET_PICO *server, TCP_CLIENT_SOCKET_PICO *client) {
  cyw43_arch_lwip_check();
  for(int i = 0; i < MAX_PENDING_CLIENTS; i++) {
    if(server->clients[i] == client) {
      server->clients[i] = NULL;
      return;
    }
  }
}

static void maybe_remove_client(TCP_CLIENT_SOCKET_PICO *client) {
  if(client->server != NULL) {
    remove_client(client->server, client);
  }
}

static int close_client_socket(TCP_CLIENT_SOCKET_PICO *client) {
  cyw43_arch_lwip_check();
  int err = ERR_OK;
  DEBUG_printf("Closing connection with client\n");
  if(client->client_pcb != NULL) {
    tcp_arg(client->client_pcb, NULL);
    tcp_sent(client->client_pcb, NULL);
    tcp_recv(client->client_pcb, NULL);
    tcp_poll(client->client_pcb, NULL, 0);
    tcp_err(client->client_pcb, NULL);
    int err = tcp_close(client->client_pcb);
    if (err != ERR_OK) {
        DEBUG_printf("close failed %d, calling abort\n", err);
        tcp_abort(client->client_pcb);
    }
    client->client_pcb = NULL;
  }
  client->is_closed = true;
  return err;
}

static void maybe_free_client(TCP_CLIENT_SOCKET_PICO *client) {
  if(client->server != NULL) { // if not owned by VM, free here
    FREE(TCP_CLIENT_SOCKET_PICO, client);
  }
}

static Value connectToAPNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 2 && argCount != 3) {
    // runtimeError("connectToAPNative() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_STRING(args[0])) {
    // runtimeError("connectToAPNative() argument 1 must be a string.");
    return NIL_VAL;
  }
  if(!IS_STRING(args[1])) {
    // runtimeError("connectToAPNative() argument 2 must be a string.");
    return NIL_VAL;
  }

  char *ssid = AS_CSTRING(args[0]);
  char *password = AS_CSTRING(args[1]);

  cyw43_arch_enable_sta_mode();
  cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

  if(argCount == 3 && IS_NUMBER(args[2])) {
    int timeout = (int)AS_NUMBER(args[2]);
    if(cyw43_arch_wifi_connect_timeout_ms(ssid, password, CYW43_AUTH_WPA2_AES_PSK, timeout)) {
      DEBUG_printf("Connected to AP\n");
      return BOOL_VAL(false);
    }
    return BOOL_VAL(true);
  } else {
    if(cyw43_arch_wifi_connect_blocking(ssid, password, CYW43_AUTH_WPA2_AES_PSK)) {
      return BOOL_VAL(false);
    }
    return BOOL_VAL(true);
  }
}


static void tcp_server_err(void *arg, err_t err) {
  TCP_CLIENT_SOCKET_PICO *clientSocket = (TCP_CLIENT_SOCKET_PICO*)arg;
  clientSocket->client_pcb = NULL; // already freed according to docs
  DEBUG_printf("tcp_client_err_fn %d\n", err);
  if (err != ERR_ABRT) { // aborts assume connection is already closed at aborter's end
    close_client_socket(clientSocket);
    maybe_remove_client(clientSocket);
    maybe_free_client(clientSocket);
  }
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *tpcb) {
    DEBUG_printf("tcp_server_poll_fn\n");
    TCP_CLIENT_SOCKET_PICO *clientSocket = (TCP_CLIENT_SOCKET_PICO*)arg;
    err_t result = close_client_socket(clientSocket); // no response is an error, close connection
    maybe_remove_client(clientSocket);
    maybe_free_client(clientSocket);
    return result;
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
  DEBUG_printf("Sent data to client\n");
  return ERR_OK;
}

static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  TCP_CLIENT_SOCKET_PICO *clientSocket = (TCP_CLIENT_SOCKET_PICO*)arg;
  if(!p) {
    DEBUG_printf("connection is closed");
    err_t result = close_client_socket(clientSocket);
    maybe_remove_client(clientSocket);
    maybe_free_client(clientSocket);
    return result;
  }

  DEBUG_printf("Received data from client\n");

  int slot = find_free_packet_slot(clientSocket);
  if(slot == -1) {
    DEBUG_printf("No free slots for packet\n");
    return ERR_MEM;
  }
  clientSocket->recv_packet[slot] = p;
  return ERR_OK;
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
  TCP_SERVER_SOCKET_PICO *serverSocket = (TCP_SERVER_SOCKET_PICO*)arg;
  if (err != ERR_OK || client_pcb == NULL) {
      DEBUG_printf("Failure in accept\n");
      // tcp_server_result(arg, err);
      return ERR_VAL;
  }
  DEBUG_printf("Client connected\n");

  TCP_CLIENT_SOCKET_PICO *clientSocket = ALLOCATE(TCP_CLIENT_SOCKET_PICO, 1);
  memset(clientSocket, 0, sizeof(TCP_CLIENT_SOCKET_PICO));

  // Assumes single accept at a time
  int freeSlot = find_first_free_client_slot(serverSocket);
  if(freeSlot == -1) {
    DEBUG_printf("No free slots for client\n");
    FREE(TCP_CLIENT_SOCKET_PICO, clientSocket);
    return ERR_VAL;
  }
  serverSocket->clients[freeSlot] = clientSocket;

  clientSocket->client_pcb = client_pcb;
  clientSocket->server = serverSocket;

  tcp_arg(clientSocket->client_pcb, clientSocket);
  tcp_sent(clientSocket->client_pcb, tcp_server_sent);
  tcp_recv(clientSocket->client_pcb, tcp_server_recv);
  tcp_poll(clientSocket->client_pcb, tcp_server_poll, POLL_TIME_S * 2);
  tcp_err(clientSocket->client_pcb, tcp_server_err);

  return ERR_OK;
}

static Value tcpServerHasBacklogNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpServerAcceptNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpServerAcceptNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_SERVER_SOCKET_PICO *socket = (TCP_SERVER_SOCKET_PICO *)socketRef->data;
  if(socket->server_pcb == NULL) {
    // runtimeError("tcpServerAcceptNative() argument 1 is not a valid server socket.");
    return NIL_VAL;
  }
  cyw43_arch_lwip_begin();
  int acceptIdx = find_first_client(socket);
  cyw43_arch_lwip_end();
  return BOOL_VAL(acceptIdx >= 0);
}

static Value tcpServerAcceptNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpServerAcceptNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpServerAcceptNative() argument 1 must be a buffer.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_SERVER_SOCKET_PICO *socket = (TCP_SERVER_SOCKET_PICO *)socketRef->data;
  if(socket->server_pcb == NULL) {
    // runtimeError("tcpServerAcceptNative() argument 1 is not a valid server socket.");
    return NIL_VAL;
  }
  cyw43_arch_lwip_begin();
  int acceptIdx = find_first_client(socket);
  if(acceptIdx >= 0) {
    DEBUG_printf("Accepting client\n");
    TCP_CLIENT_SOCKET_PICO *clientSocket = socket->clients[acceptIdx];
    remove_client(socket, clientSocket);
    clientSocket->server = NULL; // take ownership

    ObjRef *clientSocketRef = newRef("client-socket", "client-socket", clientSocket, dispose_client_socket);
    cyw43_arch_lwip_end();
    return OBJ_VAL(clientSocketRef);
  }
  cyw43_arch_lwip_end();
  return NIL_VAL;
}

static Value tcpServerCreateNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpServerCreateNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_NUMBER(args[0])) {
    // runtimeError("tcpServerCreateNative() argument 1 must be a number.");
    return NIL_VAL;
  }
  int port = (int)AS_NUMBER(args[0]);
  ObjRef *socketRef = newRef("server-socket", "server-socket", ALLOCATE(TCP_SERVER_SOCKET_PICO, 1), dispose_server_socket);
  TCP_SERVER_SOCKET_PICO *socket = (TCP_SERVER_SOCKET_PICO *)socketRef->data;
  memset(socket, 0, sizeof(TCP_SERVER_SOCKET_PICO));

  cyw43_arch_lwip_begin();

  struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
  if (!pcb) {
    // runtimeError("failed to create pcb\n");
    cyw43_arch_lwip_end();
    return NIL_VAL;
  }

  err_t err = tcp_bind(pcb, NULL, port);
  if (err) {
    // runtimeError("failed to bind to port %d\n");
    cyw43_arch_lwip_end();
    return NIL_VAL;
  }

  socket->server_pcb = tcp_listen_with_backlog(pcb, MAX_PENDING_CLIENTS);
  if (!socket->server_pcb) {
    // runtimeError("failed to listen\n");
    if (pcb) {
      tcp_close(pcb);
    }
    cyw43_arch_lwip_end();
    return NIL_VAL;
  }

  tcp_arg(socket->server_pcb, socket);
  tcp_accept(socket->server_pcb, tcp_server_accept);

  cyw43_arch_lwip_end();

  DEBUG_printf("Listening\n");

  return OBJ_VAL(socketRef);
}

static Value tcpServerCloseNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpServerAcceptNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpServerAcceptNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_SERVER_SOCKET_PICO *socket = (TCP_SERVER_SOCKET_PICO *)socketRef->data;

  DEBUG_printf("Closing server socket\n");

  cyw43_arch_lwip_begin();
  for(int i = 0; i < MAX_PENDING_CLIENTS; i++) {
    if(socket->clients[i] != NULL) {
      close_client_socket(socket->clients[i]);
      // Owned by VM, so no need to remove from server
      maybe_free_client(socket->clients[i]);
      socket->clients[i] = NULL;
    }
  }
  if (socket->server_pcb) {
    tcp_arg(socket->server_pcb, NULL);
    tcp_close(socket->server_pcb);
    socket->server_pcb = NULL;
  }
  socket->is_closed = true;
  cyw43_arch_lwip_end();
  // Memory will be freed by GC
}

static Value tcpSocketHasDataNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpSocketReadNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpSocketReadNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_CLIENT_SOCKET_PICO *socket = (TCP_CLIENT_SOCKET_PICO *)socketRef->data;
  if(socket->client_pcb == NULL) {
    // runtimeError("tcpSocketReadNative() argument 1 is not a valid client socket.");
    return NIL_VAL;
  }
  return BOOL_VAL(find_free_packet_slot(socket) != 0);
}

static Value tcpSocketReadNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpSocketReadNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpSocketReadNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_CLIENT_SOCKET_PICO *socket = (TCP_CLIENT_SOCKET_PICO *)socketRef->data;
  if(socket->client_pcb == NULL) {
    // runtimeError("tcpSocketReadNative() argument 1 is not a valid client socket.");
    return NIL_VAL;
  }
  cyw43_arch_lwip_begin();
  DEBUG_printf("Attemping read\n");
  if(find_free_packet_slot(socket) != 0) { // has packet
    struct pbuf *packet;
    ObjBuffer *buffer = newBuffer(0);
    push(OBJ_VAL(buffer));
    while((packet = get_packet(socket)) != NULL) {
      if (packet->tot_len > 0) {
        ObjBuffer *addBuffer = newBuffer((int)packet->tot_len);
        DEBUG_printf("Reading data %hu\n", packet->tot_len);
      // Receive the buffer
        pbuf_copy_partial(packet, addBuffer->bytes, packet->tot_len, 0);
        tcp_recved(socket->client_pcb, packet->tot_len);

        push(OBJ_VAL(addBuffer));
        mutateConcatenate();
    } else {
        DEBUG_printf("Invalid tot_len: %hu\n", packet->tot_len);
    }
      pbuf_free(packet);
    }
    cyw43_arch_lwip_end();
    return pop();
  } else {
    DEBUG_printf("No receive packet\n");
  }
  cyw43_arch_lwip_end();
  return NIL_VAL;
}

static Value tcpSocketWriteNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 2) {
    // runtimeError("tcpSocketWriteNative() takes exactly 2 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpSocketWriteNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  if(!IS_BUFFER(args[1])) {
    // runtimeError("tcpSocketWriteNative() argument 2 must be a buffer.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_CLIENT_SOCKET_PICO *socket = (TCP_CLIENT_SOCKET_PICO *)socketRef->data;
  if(socket->client_pcb == NULL) {
    // runtimeError("tcpSocketWriteNative() argument 1 is not a valid client socket.");
    return NIL_VAL;
  }
  ObjBuffer *dataBuffer = AS_BUFFER(args[1]);
  if(dataBuffer->size == 0) {
    // runtimeError("tcpSocketWriteNative() argument 2 is empty.");
    return NUMBER_VAL(0);
  }
  cyw43_arch_lwip_begin();
  err_t err = tcp_write(socket->client_pcb, dataBuffer->bytes, dataBuffer->size, TCP_WRITE_FLAG_COPY);
  cyw43_arch_lwip_end();
  if (err != ERR_OK) {
    /*
      The tcp_write() function will fail and return ERR_MEM if the length of the data exceeds the current send buffer size or if the length of the queue of outgoing segment is larger than the upper limit defined in lwipopts.h (TCP_SND_QUEUELEN). If the function returns ERR_MEM, the application should wait until some of the currently enqueued data has been successfully received by the other host and try again.
    */
    DEBUG_printf("Failed to write data %d\n", err);
    return NUMBER_VAL(-1);
  }
  DEBUG_printf("Wrote data to client: %d\n", dataBuffer->size);
  return NUMBER_VAL(dataBuffer->size);
}

static Value tcpServerIsClosedNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpServerIsClosedNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpServerIsClosedNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_SERVER_SOCKET_PICO *socket = (TCP_SERVER_SOCKET_PICO *)socketRef->data;
  return BOOL_VAL(socket->is_closed);
}

static Value tcpSocketIsClosedNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpSocketIsClosedNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpSocketIsClosedNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_CLIENT_SOCKET_PICO *socket = (TCP_CLIENT_SOCKET_PICO *)socketRef->data;
  return BOOL_VAL(socket->is_closed);
}

static Value tcpSocketCloseNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 1) {
    // runtimeError("tcpSocketCloseNative() takes exactly 1 argument (%d given).", argCount);
    return NIL_VAL;
  }
  if(!IS_REF(args[0])) {
    // runtimeError("tcpSocketCloseNative() argument 1 must be a socket ref.");
    return NIL_VAL;
  }
  ObjRef *socketRef = AS_REF(args[0]);
  TCP_CLIENT_SOCKET_PICO *socket = (TCP_CLIENT_SOCKET_PICO *)socketRef->data;
  if(socket->client_pcb == NULL) {
    // runtimeError("tcpSocketCloseNative() argument 1 is not a valid client socket.");
    return NIL_VAL;
  }
  cyw43_arch_lwip_begin();
  close_client_socket(socket);
  // Don't free, we know it's owned by VM
  cyw43_arch_lwip_end();
  DEBUG_printf("Server socket closed\n");
  return NIL_VAL;
}

static Value pollNetworkNative(Value *receiver, int argCount, Value *args) {
  if(argCount != 0) {
    // runtimeError("pollNetworkNative() takes exactly 0 arguments (%d given).", argCount);
    return NIL_VAL;
  }
  cyw43_arch_poll();
  return NIL_VAL;
}

void registerPicoWFunctions() {
  registerNativeMethod("connectToAP", connectToAPNative);
  registerNativeMethod("__tcp_server_create", tcpServerCreateNative);
  registerNativeMethod("__tcp_server_has_backlog", tcpServerHasBacklogNative);
  registerNativeMethod("__tcp_server_accept", tcpServerAcceptNative);
  registerNativeMethod("__tcp_server_is_closed", tcpServerIsClosedNative);
  registerNativeMethod("__tcp_server_close", tcpServerCloseNative);
  registerNativeMethod("__tcp_socket_has_data", tcpSocketHasDataNative);
  registerNativeMethod("__tcp_socket_read", tcpSocketReadNative);
  registerNativeMethod("__tcp_socket_write", tcpSocketWriteNative);
  registerNativeMethod("__tcp_socket_is_closed", tcpSocketIsClosedNative);
  registerNativeMethod("__tcp_socket_close", tcpSocketCloseNative);
  registerNativeMethod("__poll_network", pollNetworkNative);
}