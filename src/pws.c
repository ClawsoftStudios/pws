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

static const char *strnstr(const char *haystack, const char *needle, size_t length) {
  size_t needleLen = strlen(needle);
  if (!needleLen) return haystack;

  for (int i = 0; i <= (int)(length-needleLen); ++i) {
    if ((haystack[0] == needle[0]) && !strncmp(haystack, needle, needleLen)) return haystack;
    ++haystack;
  }

  return NULL;
}



Pws_Connection pws_connect(Pws *pws, void *userPtr, Pws_Connect_Info connectInfo) {
  assert(pws);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;

  _Pws_Dynamic_Buffer buffer = {0};
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "GET ");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, connectInfo.path);
  _pws_dynamic_buffer_append_cstr(pws, &buffer, " HTTP/1.1\r\nHost: ");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, connectInfo.host);
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\nUpgrade: websocket\r\nConnection: Upgrade");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\nSec-Websocket-Key: dGhlIHNhbXBsZSBub25jZQ==");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\nOrigin: ");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, connectInfo.origin);
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\nSec-Websocket-Protocol: chat, superchat");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\nSec-Websocket-Version: 13");
  _pws_dynamic_buffer_append_cstr(pws, &buffer, "\r\n\r\n");

  size_t sent = 0;
  while (sent < buffer.count) {
    int n = pws->send(userPtr, buffer.data, buffer.count);
    if (n <= 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    sent += n;
  }

  buffer.count = 0;
  buffer.capacity = 1<<(8-1);

  if (pws->leftovers.buffer) {
    _pws_dynamic_buffer_append_many(pws, &buffer, pws->leftovers.buffer, pws->leftovers.count);
    free(pws->leftovers.buffer);
    pws->leftovers.count = 0;
    pws->leftovers.buffer = NULL;
  }

  const char *ptr = NULL;
  while (buffer.count < 4 || !(ptr = strnstr(buffer.data, "\r\n\r\n", buffer.count))) {
    _pws_dynamic_buffer_resize(pws, &buffer, buffer.capacity << 1);
    int n = pws->recv(userPtr, &buffer.data[buffer.count], buffer.capacity-buffer.count);
    if (n < 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    buffer.count += n;
  }

  ptr += 4;
  if (buffer.count-(ptr-buffer.data)) {
    pws->leftovers.count = buffer.count-(ptr-buffer.data);
    pws->leftovers.buffer = malloc(pws->leftovers.count);
    memcpy(pws->leftovers.buffer, &buffer.data[ptr-buffer.data], pws->leftovers.count);
  }

  if (strncmp(buffer.data, "HTTP/1.1 101 Switching Protocols\r\n", 34) != 0) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
defer:
  return (pws->connection = deferValue);
}

Pws_Connection pws_recv_message(Pws *pws, void *userPtr, Pws_Message **message) {
  assert(pws && message);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  Pws_Connection deferValue = PWS_CONNECTION_OPEN;
  _Pws_Dynamic_Buffer messageBuffer = {0};

  Pws_Opcode opcode = PWS_OPCODE_CONTINUATION;

  _Pws_Frame *frame = NULL;
  while (!frame || !(frame->flags & _PWS_FRAME_FLAG_FIN)) {
    if (!_pws_recv_frame(pws, userPtr, &frame)) DEFER_RETURN(PWS_CONNECTION_CONNECTING);
    if (!opcode) {
      if (!frame->opcode) return _pws_error(pws, userPtr, 1002, "opcode of first frame in a message must not be set to continuation"); // Protocol error
      opcode = frame->opcode;
    } else if (frame->opcode) return _pws_error(pws, userPtr, 1002, "opcode of n-th frame in a message must be set to continuation"); // Protocol error

    _pws_dynamic_buffer_append_many(pws, &messageBuffer, frame->payload, frame->length);
  }

  Pws_Message *msg = pws_create_message(pws, opcode, messageBuffer.count);
  assert(msg);
  memcpy(msg->payload, messageBuffer.data, messageBuffer.count);

  *message = msg;
defer:
  _pws_dynamic_buffer_free(pws, &messageBuffer);

  return (pws->connection = deferValue);
}

Pws_Connection pws_send_message(Pws *pws, void *userPtr, const Pws_Message *message) {
  assert(pws && message);
  assert(pws->connection == PWS_CONNECTION_OPEN);

  _Pws_Frame *frame = _pws_create_frame(pws, _PWS_FRAME_FLAG_FIN, message->opcode, message->length);
  assert(frame);
  memcpy(frame->payload, message->payload, message->length);

  Pws_Connection connection = _pws_send_frame(pws, userPtr, frame);
  _pws_free_frame(pws, frame);
  if (message->opcode == PWS_OPCODE_CLOSE) connection = PWS_CONNECTION_CONNECTING;

  return (pws->connection = connection);
}



Pws_Message *pws_create_message(const Pws *pws, Pws_Opcode opcode, size_t length) {
  Pws_Message *message = _pws_alloc(pws, sizeof(*message) + length);
  if (!message) return NULL;
  memset(message, 0, sizeof(*message) + length);
  message->opcode = opcode;
  message->length = length;

  return message;
}

Pws_Message *pws_create_close_message(const Pws *pws, uint16_t status, size_t length, const char *payload) {
  Pws_Message *message = _pws_alloc(pws, sizeof(*message) + length);
  if (!message) return NULL;
  memset(message, 0, sizeof(*message));
  message->opcode = PWS_OPCODE_CLOSE;
  message->length = length+2;

  message->payload[0] = (status>>8) & 0xFF;
  message->payload[1] = (status>>0) & 0xFF;

  if (length) memcpy(&message->payload[2], payload, length);

  return message;
}

void pws_free_message(const Pws *pws, Pws_Message *message) {
  _pws_free(pws, message);
}



Pws_Connection _pws_error(Pws *pws, void *userPtr, uint16_t status, const char *payloadCstr) {
  assert(pws);

  Pws_Message *message = pws_create_close_message(pws, status, strlen(payloadCstr), payloadCstr);
  (void)pws_send_message(pws, userPtr, message);
  pws_free_message(pws, message);

  return (pws->connection = PWS_CONNECTION_CONNECTING);
}