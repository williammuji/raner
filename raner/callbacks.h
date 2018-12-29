// Copyright (c) 2018 Williammuji Wong. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RANER_NET_CALLBACKS_H_
#define RANER_NET_CALLBACKS_H_

#include <functional>
#include <memory>

namespace raner {
using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// All client visible callbacks go here.

class ByteBuffer;
class TCPConnection;
typedef std::shared_ptr<TCPConnection> TCPConnectionPtr;
typedef std::function<void()> TimerCallback;
typedef std::function<void(const TCPConnectionPtr&)> ConnectionCallback;
typedef std::function<void(const TCPConnectionPtr&)> CloseCallback;
typedef std::function<void(const TCPConnectionPtr&)> WriteCompleteCallback;
typedef std::function<void(const TCPConnectionPtr&, size_t)>
    HighWaterMarkCallback;

// the data has been read to (buf, len)
typedef std::function<void(const TCPConnectionPtr&, ByteBuffer*)>
    MessageCallback;

void defaultConnectionCallback(const TCPConnectionPtr& conn);
void defaultMessageCallback(const TCPConnectionPtr& conn, ByteBuffer* buffer);

}  // namespace raner

#endif  // RANER_NET_CALLBACKS_H_
