#include "internal.h"

#include <string.h>
#include <assert.h>

void _pws_dynamic_buffer_free(const Pws *pws, _Pws_Dynamic_Buffer *buffer) {
  _pws_free(pws, buffer->data);
  buffer->capacity = buffer->count = 0;
  buffer->data = NULL;
}

void _pws_dynamic_buffer_resize(const Pws *pws, _Pws_Dynamic_Buffer *buffer, size_t newCapacity) {
  if (!newCapacity) {
    _pws_dynamic_buffer_free(pws, buffer);
    return;
  }

  buffer->data = _pws_realloc(pws, buffer->data, buffer->capacity = newCapacity);
  assert(buffer->data);

  if (buffer->count > buffer->capacity) buffer->count = buffer->capacity;
}

void _pws_dynamic_buffer_append(const Pws *pws, _Pws_Dynamic_Buffer *buffer, char byte) {
  if (buffer->count >= buffer->capacity) _pws_dynamic_buffer_resize(pws, buffer, buffer->capacity ? (buffer->capacity << 1) : 32);
  buffer->data[buffer->count++] = byte;
}

void _pws_dynamic_buffer_append_many(const Pws *pws, _Pws_Dynamic_Buffer *buffer, char *bytes, size_t length) {
  if (buffer->count+length > buffer->capacity) {
    do {
      _pws_dynamic_buffer_resize(pws, buffer, buffer->capacity ? (buffer->capacity << 1) : 32);
    } while (buffer->count+length > buffer->capacity);
  }

  memcpy(&buffer->data[buffer->count], bytes, length);
  buffer->count += length;
}