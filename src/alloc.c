#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void *_pws_alloc(const Pws *pws, size_t size) {
  if (!pws) return NULL;
  if (!pws->allocator || !pws->allocator->alloc) return malloc(size);
  return pws->allocator->alloc(pws->allocator->data, size);
}

void _pws_free(const Pws *pws, void *ptr) {
  if (!pws) return;
  if (!pws->allocator || !pws->allocator->free) free(ptr);
  else pws->allocator->free(pws->allocator->data, ptr);
}

void *_pws_realloc(const Pws *pws, void *ptr, size_t size) {
  if (!pws) return NULL;
  if (!pws->allocator || !pws->allocator->realloc) {
    if (pws->allocator) assert(!pws->allocator->alloc && !pws->allocator->free);
    return realloc(ptr, size);
  }

  return pws->allocator->realloc(pws->allocator->data, ptr, size);
}