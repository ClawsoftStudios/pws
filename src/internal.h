#ifndef   __PWS_INTERNAL_H_
#define   __PWS_INTERNAL_H_

#include "pws/pws.h"

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
void _pws_dynamic_buffer_append_many(const Pws *pws, _Pws_Dynamic_Buffer *buffer, char *bytes, size_t length);

#endif // __PWS_INTERNAL_H_