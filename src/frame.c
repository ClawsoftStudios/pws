#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef min
#define min(a, b) ((a)<(b))?(a):(b)
#endif

_Pws_Frame *_pws_create_frame(const Pws *pws, _Pws_Frame_Flags flags, Pws_Opcode opcode, size_t length) {
  _Pws_Frame *frame = _pws_alloc(pws, sizeof(*frame) + length);
  if (!frame) return NULL;
  memset(frame, 0, sizeof(*frame) + length);
  frame->flags = flags;
  frame->opcode = opcode;
  frame->length = length;

  return frame;
}

void _pws_free_frame(const Pws *pws, _Pws_Frame *frame) {
  _pws_free(pws, frame);
}



Pws_Connection _pws_recv_frame(Pws *pws, void *userPtr, _Pws_Frame **frame) {
  assert(pws && frame);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  _Pws_Frame *newFrame = NULL;

  _Pws_Dynamic_Buffer recvBuffer = {0};
  _pws_dynamic_buffer_resize(pws, &recvBuffer, 256);

  if (pws->leftovers.count) {
    _pws_dynamic_buffer_append_many(pws, &recvBuffer, pws->leftovers.buffer, pws->leftovers.count);
    free(pws->leftovers.buffer);
    pws->leftovers.count = 0;
    pws->leftovers.buffer = NULL;
  }

  int receivedCount = 0;
  if (recvBuffer.count < 2) {
    if ((receivedCount = pws->recv(userPtr, recvBuffer.data, recvBuffer.capacity)) <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    recvBuffer.count = receivedCount;
  }

  if (recvBuffer.count < 2) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  size_t payloadLength = recvBuffer.data[1] & 0x7F;
  size_t offset = 2;
  if (payloadLength == 126) {
    offset += 2;
    if (recvBuffer.count < offset) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

    payloadLength = (recvBuffer.data[2] << 8) | recvBuffer.data[3];
  } else if (payloadLength == 127) {
    offset += 8;
    if (recvBuffer.count < offset) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

    payloadLength = 0;
    for (int i = 0; i < 8; ++i) payloadLength = (payloadLength<<8) | (recvBuffer.data[2+i] & 0xFF);
  }

  bool masking = (recvBuffer.data[1]>>7)&1;
  if (pws->type == masking) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  uint8_t maskingKey[4] = {0};
  if (masking) {
    memcpy(maskingKey, &recvBuffer.data[offset], 4);
    offset += 4;
    if (recvBuffer.count < offset) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
  }

  newFrame = _pws_create_frame(pws, (recvBuffer.data[0] >> 4) & 0x0F, (recvBuffer.data[0] >> 0) & 0x0F, payloadLength);
  if (!newFrame) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  int j = 0;
  if (masking) {
    for (size_t i = offset; i < recvBuffer.count; ++i) recvBuffer.data[i] ^= maskingKey[(j++) % 4];
  }

  size_t payloadSize = min(recvBuffer.count-offset, payloadLength);
  memcpy(newFrame->payload, &recvBuffer.data[offset], payloadSize);

  if (recvBuffer.count-offset > payloadLength) {
    pws->leftovers.count = recvBuffer.count-offset-payloadLength;
    pws->leftovers.buffer = malloc(pws->leftovers.count);
    memcpy(pws->leftovers.buffer, &recvBuffer.data[offset+payloadLength], pws->leftovers.count);
  } else {
    _pws_dynamic_buffer_resize(pws, &recvBuffer, (payloadLength-payloadSize));
    recvBuffer.count = 0;

    while (recvBuffer.count < recvBuffer.capacity) {
      receivedCount = pws->recv(userPtr, &recvBuffer.data[recvBuffer.count], recvBuffer.capacity-recvBuffer.count);
      if (receivedCount <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

      for (size_t i = 0; i < (size_t)receivedCount; ++i) recvBuffer.data[recvBuffer.count+i] ^= maskingKey[(j++) % 4];
      recvBuffer.count += receivedCount;
    }

    memcpy(&newFrame->payload[payloadSize], recvBuffer.data, recvBuffer.count);
  }

  *frame = newFrame;
defer:
  _pws_dynamic_buffer_free(pws, &recvBuffer);
  if (deferValue == PWS_CONNECTION_CONNECTING) _pws_free_frame(pws, newFrame);
  return (pws->connection = deferValue);
}

Pws_Connection _pws_send_frame(Pws *pws, void *userPtr, _Pws_Frame *frame) {
  assert(pws && frame);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  bool masking = (pws->type == PWS_CLIENT);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  _Pws_Dynamic_Buffer sendBuffer = {0};

  _pws_dynamic_buffer_append(pws, &sendBuffer, frame->opcode | (frame->flags<<4));

  int8_t lengthByte = 0;
  uint8_t lengthLength = 0;

  if (frame->length > UINT16_MAX) {
    lengthByte = 127;
    lengthLength = 8;
  } else if (frame->length > INT8_MAX) {
    lengthByte = 126;
    lengthLength = 2;
  } else {
    lengthByte = (int8_t)frame->length;
    lengthLength = 0;
  }

  _pws_dynamic_buffer_append(pws, &sendBuffer, lengthByte | (masking << 7));
  for (int i = 0; i < lengthLength; ++i) _pws_dynamic_buffer_append(pws, &sendBuffer, (frame->length>>((lengthLength-i-1)*8)) & 0xFF);

  char *payload = frame->payload;
  if (masking) {
    uint8_t maskingKey[4] = { 69, 12, 37, 42 };
    _pws_dynamic_buffer_append_many(pws, &sendBuffer, (char*)maskingKey, sizeof(maskingKey));
    payload = malloc(frame->length);
    if (!payload) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    for (size_t i = 0; i < frame->length; ++i) payload[i] = frame->payload[i] ^ maskingKey[i % 4];
  }

  _pws_dynamic_buffer_append_many(pws, &sendBuffer, payload, frame->length);

  size_t sent = 0;
  while (sent < sendBuffer.count) {
    int s = pws->send(userPtr, &sendBuffer.data[sent], sendBuffer.count-sent);
    if (s <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    sent += s;
  }

defer:
  _pws_dynamic_buffer_free(pws, &sendBuffer);
  return (pws->connection = deferValue);
}