#ifndef   _PWS_H_
#define   _PWS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum Pws_Frame_Opcode {
  PWS_FRAME_OPCODE_CONTINUATION = 0x00,
  PWS_FRAME_OPCODE_TEXT = 0x01,
  PWS_FRAME_OPCODE_BINARY = 0x02,
  PWS_FRAME_OPCODE_CLOSE = 0x08,
  PWS_FRAME_OPCODE_PING = 0x09,
  PWS_FRAME_OPCODE_PONG = 0x0A,
} Pws_Frame_Opcode;

typedef enum Pws_Frame_Flag_Bits {
  PWS_FRAME_FLAG_RSV3 = (1<<0),
  PWS_FRAME_FLAG_RSV2 = (1<<1),
  PWS_FRAME_FLAG_RSV1 = (1<<2),
  PWS_FRAME_FLAG_FIN  = (1<<3)
} Pws_Frame_Flag_Bits;

typedef uint8_t Pws_Frame_Flags;

typedef struct Pws_Frame {
  Pws_Frame_Flags flags : 4;
  Pws_Frame_Opcode opcode : 4;
  size_t payloadLen;
  char payload[];
} Pws_Frame;



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

Pws_Connection pws_recv_frame(Pws *pws, Pws_Frame **frame);
Pws_Connection pws_send_frame(Pws *pws, Pws_Frame *frame);

Pws_Connection pws_close(Pws *pws, uint16_t statusCode);

Pws_Frame *pws_create_frame(const Pws *pws, size_t payloadLen);
void pws_free_frame(const Pws *pws, Pws_Frame *frame);

#endif // _PWS_H_