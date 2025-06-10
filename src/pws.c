/*

Server handshake:

HTTP/1.1 101 Switching Protocols
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
Sec-WebSocket-Protocol: chat

*/

#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define DEFER_RETURN(value) \
  do { \
    deferValue = value; \
    goto defer; \
  } while (0)



Pws_Connection pws_connect(Pws *pws, Pws_Connect_Info connectInfo) {
  assert(pws);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;

  const char *handshakeFormat = // TODO: Use better way of creating handshake request
    "GET %s HTTP/1.1\r\n"
    "Host: %s\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Origin: %s\r\n"
    "Sec-WebSocket-Protocol: chat, superchat\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n";

  enum { BUFFER_CAPACITY = 2048 };

  char buffer[BUFFER_CAPACITY];
  int n = sprintf(buffer, handshakeFormat, connectInfo.path, connectInfo.host, connectInfo.origin);
  if (n < 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  assert(pws->send(pws->userPtr, buffer, n) == n);

  n = pws->recv(pws->userPtr, buffer, BUFFER_CAPACITY);
  if (n < 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  if (strncmp(buffer, "HTTP/1.1 101 Switching Protocols\r\n", 34) != 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

defer:
  return (pws->connection = deferValue);
}

Pws_Connection pws_recv_frame(Pws *pws, Pws_Frame **frame) {
  assert(pws && frame);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  Pws_Frame *newFrame = NULL;

  _Pws_Dynamic_Buffer recvBuffer = {0};
  _pws_dynamic_buffer_resize(pws, &recvBuffer, 256);

  int receivedCount = pws->recv(pws->userPtr, recvBuffer.data, recvBuffer.capacity);
  if (receivedCount <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
  recvBuffer.count = receivedCount;

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

  newFrame = pws_create_frame(pws, payloadLength);
  if (!newFrame) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

  newFrame->flags  = (recvBuffer.data[0] >> 4) & 0x0F;
  newFrame->opcode = (recvBuffer.data[0] >> 0) & 0x0F;

  int j = 0;
  if (masking) {
    for (size_t i = offset; i < recvBuffer.count; ++i) recvBuffer.data[i] ^= maskingKey[(j++) % 4];
  }

  size_t payloadSize = recvBuffer.count-offset;
  memcpy(newFrame->payload, &recvBuffer.data[offset], payloadSize);

  _pws_dynamic_buffer_resize(pws, &recvBuffer, (payloadLength-payloadSize));
  recvBuffer.count = 0;

  while (recvBuffer.count < recvBuffer.capacity) {
    receivedCount = pws->recv(pws->userPtr, &recvBuffer.data[recvBuffer.count], recvBuffer.capacity-recvBuffer.count);
    if (receivedCount <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);

    for (size_t i = 0; i < (size_t)receivedCount; ++i) recvBuffer.data[recvBuffer.count+i] ^= maskingKey[(j++) % 4];
    recvBuffer.count += receivedCount;
  }

  memcpy(&newFrame->payload[payloadSize], recvBuffer.data, recvBuffer.count);

  _pws_dynamic_buffer_free(pws, &recvBuffer);

  *frame = newFrame;

defer:
  _pws_dynamic_buffer_free(pws, &recvBuffer);
  if (deferValue == PWS_CONNECTION_CONNECTING) pws_free_frame(pws, newFrame);
  return (pws->connection = deferValue);
}

Pws_Connection pws_send_frame(Pws *pws, Pws_Frame *frame) {
  assert(pws && frame);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  bool masking = (pws->type == PWS_CLIENT);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  _Pws_Dynamic_Buffer sendBuffer = {0};

  _pws_dynamic_buffer_append(pws, &sendBuffer, frame->opcode | (frame->flags<<4));

  int8_t lengthByte = 0;
  uint8_t lengthLength = 0;

  if (frame->payloadLen > UINT16_MAX) {
    lengthByte = 127;
    lengthLength = 8;
  } else if (frame->payloadLen > INT8_MAX) {
    lengthByte = 126;
    lengthLength = 2;
  } else {
    lengthByte = (int8_t)frame->payloadLen;
    lengthLength = 0;
  }

  _pws_dynamic_buffer_append(pws, &sendBuffer, lengthByte | (masking << 7));
  for (int i = 0; i < lengthLength; ++i) _pws_dynamic_buffer_append(pws, &sendBuffer, (frame->payloadLen>>((lengthLength-i-1)*8)) & 0xFF);

  char *payload = frame->payload;
  if (masking) {
    uint8_t maskingKey[4] = { 69, 12, 37, 42 };
    _pws_dynamic_buffer_append_many(pws, &sendBuffer, (char*)maskingKey, sizeof(maskingKey));
    payload = malloc(frame->payloadLen);
    if (!payload) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    for (size_t i = 0; i < frame->payloadLen; ++i) payload[i] = frame->payload[i] ^ maskingKey[i % 4];
  }

  _pws_dynamic_buffer_append_many(pws, &sendBuffer, payload, frame->payloadLen);

  size_t sent = 0;
  while (sent < sendBuffer.count) {
    int s = pws->send(pws->userPtr, &sendBuffer.data[sent], sendBuffer.count-sent);
    if (s <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    sent += s;
  }

defer:
  _pws_dynamic_buffer_free(pws, &sendBuffer);
  return (pws->connection = deferValue);
}

Pws_Connection pws_close(Pws *pws, uint16_t statusCode) {
  assert(pws);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  Pws_Frame *frame = pws_create_frame(pws, statusCode?2:0);
  assert(frame);
  frame->flags |= PWS_FRAME_FLAG_FIN;
  frame->opcode = PWS_FRAME_OPCODE_CLOSE;

  if (statusCode) {
    frame->payload[0] = (statusCode>>8) & 0xFF;
    frame->payload[1] = (statusCode>>0) & 0xFF;
  }

  if (!pws_send_frame(pws, frame)) return pws->connection = PWS_CONNECTION_CONNECTING;

  return pws->connection = PWS_CONNECTION_CONNECTING;
}



Pws_Frame *pws_create_frame(const Pws *pws, size_t payloadLen) {
  const size_t frameSize = sizeof(Pws_Frame) + payloadLen;

  Pws_Frame *frame = _pws_alloc(pws, frameSize);
  if (!frame) return NULL;
  memset(frame, 0, sizeof(frameSize));
  frame->payloadLen = payloadLen;

  return frame;
}

void pws_free_frame(const Pws *pws, Pws_Frame *frame) {
  _pws_free(pws, frame);
}