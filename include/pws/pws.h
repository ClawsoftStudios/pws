#ifndef   _PWS_H_
#define   _PWS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum Pws_Opcode {
  PWS_OPCODE_CONTINUATION = 0x00,
  PWS_OPCODE_TEXT = 0x01,
  PWS_OPCODE_BINARY = 0x02,
  PWS_OPCODE_CLOSE = 0x08,
  PWS_OPCODE_PING = 0x09,
  PWS_OPCODE_PONG = 0x0A,
} Pws_Opcode;

typedef struct Pws_Message {
  Pws_Opcode opcode;
  size_t length;
  char payload[];
} Pws_Message;



typedef int (*Pws_Send)(void *, const void *, int);
typedef int (*Pws_Recv)(void *, void *, int);

typedef enum Pws_Type {
  PWS_SERVER = 0,
  PWS_CLIENT,

  COUNT_PWS_TYPES
} Pws_Type;

typedef enum Pws_Connection {
  PWS_CONNECTION_CONNECTING = 0,
  PWS_CONNECTION_OPEN,

  COUNT_PWS_CONNECTIONS
} Pws_Connection;

typedef void *(*Pws_Allocator_Alloc)(void *, size_t);
typedef void (*Pws_Allocator_Free)(void *, void *);
typedef void *(*Pws_Allocator_Realloc)(void *, void *, size_t);

typedef struct Pws_Allocator {
  void *data;
  Pws_Allocator_Alloc alloc;
  Pws_Allocator_Free free;
  Pws_Allocator_Realloc realloc;
} Pws_Allocator;

typedef struct Pws {
  Pws_Type type;
  Pws_Allocator *allocator;

  void *userPtr;
  Pws_Send send;
  Pws_Recv recv;

  Pws_Connection connection;
} Pws;

typedef struct Pws_Connect_Info {
  const char *origin;
  const char *host;
  const char *path;
} Pws_Connect_Info;

Pws_Connection pws_connect(Pws *pws, Pws_Connect_Info connectInfo);
// TODO: Add support for servers

Pws_Connection pws_recv_message(Pws *pws, Pws_Message **message);
Pws_Connection pws_send_message(Pws *pws, Pws_Message *message);

Pws_Connection pws_close(Pws *pws, uint16_t statusCode);

Pws_Message *pws_create_message(const Pws *pws, Pws_Opcode opcode, size_t length);
void pws_free_message(const Pws *pws, Pws_Message *message);

#endif // _PWS_H_