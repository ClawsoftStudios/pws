#ifndef   _PWS_HPP_
#define   _PWS_HPP_

#include "pws.h"

#include <utility>

namespace pws {

using Allocator = Pws_Allocator;

enum class Opcode {
  Continuation = PWS_OPCODE_CONTINUATION,
  Text = PWS_OPCODE_TEXT,
  Binary = PWS_OPCODE_BINARY,
  Close = PWS_OPCODE_CLOSE,
  Ping = PWS_OPCODE_PING,
  Pong = PWS_OPCODE_PONG,
};

class Message {
  friend class Context;
private:
  inline Message(Pws *context, Pws_Message *message)
    : mContext(context), mMessage(message)
  {}
public:
  Message() = default;

  inline ~Message() {
    free();
  }

  inline Message(const Message &other)
  {
    *this = other;
  }

  inline Message &operator =(const Message &other) {
    if (this == &other) return *this;

    free();
    mContext = other.mContext;
    mMessage = other.mMessage;

    return *this;
  }

  inline Message(Message &&other)
    : mContext(std::move(other.mContext)), mMessage(std::move(other.mMessage))
  {
    other.mContext = nullptr;
    other.mMessage = nullptr;
  }

  inline Message &operator =(Message &&other) {
    mContext = std::move(other.mContext);
    mMessage = std::move(other.mMessage);

    other.mContext = nullptr;
    other.mMessage = nullptr;

    return *this;
  }

  inline void free() {
    pws_free_message(mContext, mMessage);
    mMessage = nullptr;
    mContext = nullptr;
  }

  inline operator bool() const { return mMessage; }

  inline Opcode opcode() const { return (Opcode)mMessage->opcode; }
  inline char *payload() const { return mMessage->payload; }
  inline size_t length() const { return mMessage->length; }
private:
  Pws *mContext = nullptr;
  Pws_Message *mMessage = nullptr;
};

class Connection {
public:
  enum Value {
    Connecting = PWS_CONNECTION_CONNECTING,
    Open = PWS_CONNECTION_OPEN
  };
public:
  Connection() = default;

  inline Connection(const Value &value)
    : mValue(value)
  {}

  inline Connection(const Pws_Connection &value)
    : mValue((Value)value)
  {}

  ~Connection() = default;

  inline bool operator !() const { return !mValue; }

  inline bool operator ==(const Connection &other) const { return mValue == other.mValue; }
  inline bool operator ==(const Value &other) const { return mValue == other; }

  inline bool operator !=(const Connection &other) const { return mValue != other.mValue; }
  inline bool operator !=(const Value &other) const { return mValue != other; }
public:
  Value mValue = Connecting;
};

struct ConnectInfo {
  const char *origin;
  const char *host;
  const char *path;
};

class Context {
public:
  enum class Type {
    Server = PWS_SERVER,
    Client = PWS_CLIENT
  };

  using Send = int (*)(void *, const void *, int);
  using Recv = int (*)(void *, void *, int);
public:
  inline Context(Type type, Send send, Recv recv, Allocator *allocator = nullptr)
    : mPws(Pws{ (Pws_Type)type, allocator, send, recv, {}, PWS_CONNECTION_CONNECTING })
  {}

  ~Context() = default;

  Context(const Context &) = delete;
  Context &operator =(const Context &) = delete;

  Context(Context &&) = delete;
  Context &operator =(Context &&) = delete;

  inline Message createMessage(Opcode opcode, size_t length) {
    Message msg = Message(&mPws, pws_create_message(&mPws, (Pws_Opcode)opcode, length));
    return std::move(msg);
  }

  inline Message createMessage(uint16_t status, size_t length = 0, const char *payload = nullptr) {
    Message msg = Message(&mPws, pws_create_close_message(&mPws, status, length, payload));
    return std::move(msg);
  }

  inline Connection connect(void *userPtr, const ConnectInfo &connectInfo) { return (Connection)pws_connect(&mPws, userPtr, Pws_Connect_Info{ connectInfo.origin, connectInfo.host, connectInfo.path }); }

  inline Connection recvMessage(void *userPtr, Message &message) {
    message.mContext = &mPws;
    return (Connection)pws_recv_message(&mPws, userPtr, &message.mMessage);
  }

  inline Connection sendMessage(void *userPtr, const Message &message) {
    return (Connection)pws_send_message(&mPws, userPtr, message.mMessage);
  }
private:
  Pws mPws;
};

}

#endif // _PWS_HPP_