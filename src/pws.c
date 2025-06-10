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

Pws_Connection pws_recv_message(Pws *pws, Pws_Message **message) {
  assert(pws && message);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  _Pws_Dynamic_Buffer messageBuffer = {0};

  Pws_Opcode opcode = PWS_OPCODE_CONTINUATION;

  _Pws_Frame *frame = NULL;
  while (!frame || !(frame->flags & _PWS_FRAME_FLAG_FIN)) {
    if (!_pws_recv_frame(pws, &frame)) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    if (!opcode) {
      if (!frame->opcode) return pws_close(pws, 1002); // Protocol error
      opcode = frame->opcode;
    } else if (frame->opcode) return pws_close(pws, 1002); // Protocol error

    _pws_dynamic_buffer_append_many(pws, &messageBuffer, frame->payload, frame->length);
  }

  Pws_Message *msg = pws_create_message(pws, opcode, messageBuffer.count);
  assert(msg);
  memcpy(msg->payload, messageBuffer.data, messageBuffer.count);
defer:
  _pws_dynamic_buffer_free(pws, &messageBuffer);

  return (pws->connection = deferValue);
}

Pws_Connection pws_send_message(Pws *pws, Pws_Message *message) {
  assert(pws && message);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  _Pws_Frame *frame = _pws_create_frame(pws, _PWS_FRAME_FLAG_FIN, message->opcode, message->length);
  assert(frame);
  memcpy(frame->payload, message->payload, message->length);

  Pws_Connection connection = _pws_send_frame(pws, frame);
  _pws_free_frame(pws, frame);
  return (pws->connection = connection);
}

Pws_Connection pws_close(Pws *pws, uint16_t statusCode) {
  assert(pws);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  _Pws_Frame *frame = _pws_create_frame(pws, _PWS_FRAME_FLAG_FIN, PWS_OPCODE_CLOSE, statusCode?2:0);
  assert(frame);

  if (statusCode) {
    frame->payload[0] = (statusCode>>8) & 0xFF;
    frame->payload[1] = (statusCode>>0) & 0xFF;
  }

  if (!_pws_send_frame(pws, frame)) return pws->connection = PWS_CONNECTION_CONNECTING;

  return pws->connection = PWS_CONNECTION_CONNECTING;
}



Pws_Message *pws_create_message(const Pws *pws, Pws_Opcode opcode, size_t length) {
  Pws_Message *message = _pws_alloc(pws, sizeof(*message) + length);
  if (!message) return NULL;
  memset(message, 0, sizeof(*message) + length);
  message->opcode = opcode;
  message->length = length;

  return message;
}

void pws_free_message(const Pws *pws, Pws_Message *message) {
  _pws_free(pws, message);
}