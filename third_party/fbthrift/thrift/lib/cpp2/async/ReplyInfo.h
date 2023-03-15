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

#pragma once

#include <thrift/lib/cpp2/async/Interaction.h>
#include <thrift/lib/cpp2/async/ResponseChannel.h>
#include <thrift/lib/cpp2/async/Sink.h>

namespace apache {
namespace thrift {

class AppOverloadExceptionInfo {
 public:
  AppOverloadExceptionInfo(
      ResponseChannelRequest::UniquePtr req,
      ManagedStringView message,
      Tile* interaction)
      : req_(std::move(req)),
        message_(std::move(message)),
        interaction_(interaction) {}

  void operator()(folly::EventBase& evb) noexcept {
    if (interaction_) {
      interaction_->__fbthrift_releaseRef(evb);
    }
    req_->sendErrorWrapped(
        folly::make_exception_wrapper<TApplicationException>(
            TApplicationException::LOADSHEDDING, message_.str()),
        kAppOverloadedErrorCode);
  }

  ResponseChannelRequest::UniquePtr req_;
  ManagedStringView message_;
  Tile* interaction_;
};

class QueueReplyInfo {
 public:
  QueueReplyInfo(
      ResponseChannelRequest::UniquePtr req,
      Tile* interaction,
      folly::IOBufQueue queue,
      folly::Optional<uint32_t> crc32c)
      : req_(std::move(req)),
        interaction_(interaction),
        queue_(std::move(queue)),
        crc32c_(crc32c) {}

  void operator()(folly::EventBase& evb) noexcept {
    if (interaction_) {
      interaction_->__fbthrift_releaseRef(evb);
    }
    req_->sendReply(queue_.move(), nullptr, crc32c_);
  }

  ResponseChannelRequest::UniquePtr req_;
  Tile* interaction_;
  folly::IOBufQueue queue_;
  folly::Optional<uint32_t> crc32c_;
};

class StreamReplyInfo {
 public:
  StreamReplyInfo(
      ResponseChannelRequest::UniquePtr req,
      apache::thrift::detail::ServerStreamFactory stream,
      folly::IOBufQueue queue,
      folly::Optional<uint32_t> crc32c)
      : req_(std::move(req)),
        stream_(std::move(stream)),
        queue_(std::move(queue)),
        crc32c_(crc32c) {}

  void operator()(folly::EventBase&) noexcept {
    req_->sendStreamReply(queue_.move(), std::move(stream_), crc32c_);
  }

  ResponseChannelRequest::UniquePtr req_;
  apache::thrift::detail::ServerStreamFactory stream_;
  folly::IOBufQueue queue_;
  folly::Optional<uint32_t> crc32c_;
};

class SinkConsumerReplyInfo {
 public:
  SinkConsumerReplyInfo(
      ResponseChannelRequest::UniquePtr req,
      apache::thrift::detail::SinkConsumerImpl sinkConsumer,
      folly::IOBufQueue queue,
      folly::Optional<uint32_t> crc32c)
      : req_(std::move(req)),
        sinkConsumer_(std::move(sinkConsumer)),
        queue_(std::move(queue)),
        crc32c_(crc32c) {}

  void operator()(folly::EventBase&) noexcept {
#if FOLLY_HAS_COROUTINES
    req_->sendSinkReply(queue_.move(), std::move(sinkConsumer_), crc32c_);
#endif
  }

  ResponseChannelRequest::UniquePtr req_;
  apache::thrift::detail::SinkConsumerImpl sinkConsumer_;
  folly::IOBufQueue queue_;
  folly::Optional<uint32_t> crc32c_;
};

using ReplyInfo = std::variant<
    AppOverloadExceptionInfo,
    QueueReplyInfo,
    StreamReplyInfo,
    SinkConsumerReplyInfo>;

/**
 * Used in EventBaseAtomicNotificationQueue to process each dequeued item
 */
class ReplyInfoConsumer {
 public:
  explicit ReplyInfoConsumer(folly::EventBase& evb) : evb_(evb) {}
  void operator()(ReplyInfo&& info) noexcept {
    std::visit([&evb = evb_](auto&& visitInfo) { visitInfo(evb); }, info);
  }

 private:
  folly::EventBase& evb_;
};

} // namespace thrift
} // namespace apache
