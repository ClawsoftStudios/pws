#ifndef   _PWS_H_
#define   _PWS_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

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

  Pws_Send send;
  Pws_Recv recv;

  struct {
    char *buffer;
    size_t count;
  } leftovers;

  Pws_Connection connection;
} Pws;

typedef struct Pws_Connect_Info {
  const char *origin;
  const char *host;
  const char *path;
} Pws_Connect_Info;

Pws_Connection pws_connect(Pws *pws, void *userPtr, Pws_Connect_Info connectInfo);
// Pws_Connection pws_accept(Pws *pws, void *userPtr);

Pws_Connection pws_recv_message(Pws *pws, void *userPtr, Pws_Message **message);
Pws_Connection pws_send_message(Pws *pws, void *userPtr, const Pws_Message *message);

Pws_Message *pws_create_message(const Pws *pws, Pws_Opcode opcode, size_t length);
Pws_Message *pws_create_close_message(const Pws *pws, uint16_t status, size_t length, const char *payload);
void pws_free_message(const Pws *pws, Pws_Message *message);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _PWS_H_