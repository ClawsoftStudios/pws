#ifndef   __PWS_INTERNAL_H_
#define   __PWS_INTERNAL_H_

#include "pws/pws.h"

#define DEFER_RETURN(value) \
  do { \
    deferValue = value; \
    goto defer; \
  } while (0)

void *_pws_alloc(const Pws *pws, size_t size);
void _pws_free(const Pws *pws, void *ptr);
void *_pws_realloc(const Pws *pws, void *ptr, size_t size);

typedef struct _Pws_Dynamic_Buffer {
  char *data;
  size_t capacity;
  size_t count;
} _Pws_Dynamic_Buffer;

void _pws_dynamic_buffer_free(const Pws *pws, _Pws_Dynamic_Buffer *buffer);
void _pws_dynamic_buffer_resize(const Pws *pws, _Pws_Dynamic_Buffer *buffer, size_t newCapacity);
void _pws_dynamic_buffer_append(const Pws *pws, _Pws_Dynamic_Buffer *buffer, char byte);
void _pws_dynamic_buffer_append_many(const Pws *pws, _Pws_Dynamic_Buffer *buffer, const char *bytes, size_t length);
void _pws_dynamic_buffer_append_cstr(const Pws *pws, _Pws_Dynamic_Buffer *buffer, const char *cstr);

typedef enum _Pws_Frame_Flag_Bits {
  _PWS_FRAME_FLAG_RSV3 = (1<<0),
  _PWS_FRAME_FLAG_RSV2 = (1<<1),
  _PWS_FRAME_FLAG_RSV1 = (1<<2),
  _PWS_FRAME_FLAG_FIN  = (1<<3)
} _Pws_Frame_Flag_Bits;

typedef uint8_t _Pws_Frame_Flags;

typedef struct _Pws_Frame {
  _Pws_Frame_Flags flags : 4;
  Pws_Opcode opcode : 4;
  size_t length;
  char payload[];
} _Pws_Frame;

Pws_Connection _pws_recv_frame(Pws *pws, void *userPtr, _Pws_Frame **frame);
Pws_Connection _pws_send_frame(Pws *pws, void *userPtr, _Pws_Frame *frame);

_Pws_Frame *_pws_create_frame(const Pws *pws, _Pws_Frame_Flags flags, Pws_Opcode opcode, size_t length);
void _pws_free_frame(const Pws *pws, _Pws_Frame *frame);

#endif // __PWS_INTERNAL_H_