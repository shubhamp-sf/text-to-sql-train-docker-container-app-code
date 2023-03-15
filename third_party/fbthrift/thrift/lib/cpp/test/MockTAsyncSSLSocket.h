/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THRIFT_TEST_MOCKTASYNCSSLSOCKET_H_
#define THRIFT_TEST_MOCKTASYNCSSLSOCKET_H_ 1

#include <folly/portability/GMock.h>

#include <thrift/lib/cpp/async/TAsyncSSLSocket.h>

namespace apache {
namespace thrift {
namespace test {

class MockTAsyncSSLSocket : public apache::thrift::async::TAsyncSSLSocket {
 public:
  using UniquePtr = std::unique_ptr<MockTAsyncSSLSocket, Destructor>;

  MockTAsyncSSLSocket(
      const std::shared_ptr<folly::SSLContext> ctx, folly::EventBase* base)
      : TAsyncSSLSocket(ctx, base) {}

  static MockTAsyncSSLSocket::UniquePtr newSocket(
      const std::shared_ptr<folly::SSLContext> ctx, folly::EventBase* base) {
    return MockTAsyncSSLSocket::UniquePtr(new MockTAsyncSSLSocket(ctx, base));
  }

  GMOCK_METHOD6_(
      ,
      noexcept,
      ,
      connectInternal,
      void(
          AsyncSocket::ConnectCallback*,
          const folly::SocketAddress&,
          int,
          const folly::SocketOptionMap&,
          const folly::SocketAddress&,
          const std::string&));

  GMOCK_METHOD7_(
      ,
      noexcept,
      ,
      connectInternalWithTimeouts,
      void(
          AsyncSocket::ConnectCallback*,
          const folly::SocketAddress&,
          std::chrono::milliseconds,
          std::chrono::milliseconds,
          const folly::SocketOptionMap&,
          const folly::SocketAddress&,
          const std::string&));

  void connect(
      AsyncSocket::ConnectCallback* callback,
      const folly::SocketAddress& addr,
      int timeout = 0,
      const folly::SocketOptionMap& options = folly::emptySocketOptionMap,
      const folly::SocketAddress& bindAddr = anyAddress(),
      const std::string& ifName = "") noexcept override {
    connectInternal(callback, addr, timeout, options, bindAddr, ifName);
  }

  void connect(
      AsyncSocket::ConnectCallback* callback,
      const folly::SocketAddress& addr,
      std::chrono::milliseconds connectTimeout,
      std::chrono::milliseconds totalConnectTimeout,
      const folly::SocketOptionMap& options = folly::emptySocketOptionMap,
      const folly::SocketAddress& bindAddr = anyAddress(),
      const std::string& ifName = "") noexcept override {
    connectInternalWithTimeouts(
        callback,
        addr,
        connectTimeout,
        totalConnectTimeout,
        options,
        bindAddr,
        ifName);
  }

  MOCK_CONST_METHOD1(getLocalAddress, void(folly::SocketAddress*));
  MOCK_CONST_METHOD1(getPeerAddress, void(folly::SocketAddress*));
  MOCK_METHOD0(closeNow, void());
  MOCK_CONST_METHOD0(good, bool());
  MOCK_CONST_METHOD0(readable, bool());
  MOCK_CONST_METHOD0(hangup, bool());
  MOCK_CONST_METHOD2(
      getSelectedNextProtocol, void(const unsigned char**, unsigned*));
  MOCK_CONST_METHOD2(
      getSelectedNextProtocolNoThrow, bool(const unsigned char**, unsigned*));
};
} // namespace test
} // namespace thrift
} // namespace apache

#endif
