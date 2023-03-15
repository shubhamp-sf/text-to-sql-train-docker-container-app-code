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

#include <type_traits>
#include <utility>

#include <folly/Portability.h>

#include <fmt/core.h>
#include <folly/Traits.h>
#include <folly/Utility.h>
#include <folly/futures/Future.h>
#include <folly/io/Cursor.h>
#include <thrift/lib/cpp/protocol/TBase64Utils.h>
#include <thrift/lib/cpp2/SerializationSwitch.h>
#include <thrift/lib/cpp2/Thrift.h>
#include <thrift/lib/cpp2/async/AsyncProcessor.h>
#include <thrift/lib/cpp2/async/ClientBufferedStream.h>
#include <thrift/lib/cpp2/async/ClientSinkBridge.h>
#include <thrift/lib/cpp2/async/RequestChannel.h>
#include <thrift/lib/cpp2/async/Sink.h>
#include <thrift/lib/cpp2/async/StreamCallbacks.h>
#include <thrift/lib/cpp2/detail/meta.h>
#include <thrift/lib/cpp2/frozen/Frozen.h>
#include <thrift/lib/cpp2/protocol/Cpp2Ops.h>
#include <thrift/lib/cpp2/protocol/Traits.h>
#include <thrift/lib/cpp2/transport/core/RpcMetadataUtil.h>
#include <thrift/lib/cpp2/util/Frozen2ViewHelpers.h>

#if FOLLY_HAS_COROUTINES
#include <folly/experimental/coro/FutureUtil.h>
#endif

namespace apache {
namespace thrift {

class BinaryProtocolReader;
class CompactProtocolReader;

namespace detail {

template <int N, int Size, class F, class Tuple>
struct ForEachImpl {
  static uint32_t forEach(Tuple&& tuple, F&& f) {
    uint32_t res = f(std::get<N>(tuple), N);
    res += ForEachImpl<N + 1, Size, F, Tuple>::forEach(
        std::forward<Tuple>(tuple), std::forward<F>(f));
    return res;
  }
};
template <int Size, class F, class Tuple>
struct ForEachImpl<Size, Size, F, Tuple> {
  static uint32_t forEach(Tuple&& /*tuple*/, F&& /*f*/) { return 0; }
};

template <int N = 0, class F, class Tuple>
uint32_t forEach(Tuple&& tuple, F&& f) {
  return ForEachImpl<
      N,
      std::tuple_size<typename std::remove_reference<Tuple>::type>::value,
      F,
      Tuple>::forEach(std::forward<Tuple>(tuple), std::forward<F>(f));
}

template <int N, int Size, class F, class Tuple>
struct ForEachVoidImpl {
  static void forEach(Tuple&& tuple, F&& f) {
    f(std::get<N>(tuple), N);
    ForEachVoidImpl<N + 1, Size, F, Tuple>::forEach(
        std::forward<Tuple>(tuple), std::forward<F>(f));
  }
};
template <int Size, class F, class Tuple>
struct ForEachVoidImpl<Size, Size, F, Tuple> {
  static void forEach(Tuple&& /*tuple*/, F&& /*f*/) {}
};

template <int N = 0, class F, class Tuple>
void forEachVoid(Tuple&& tuple, F&& f) {
  ForEachVoidImpl<
      N,
      std::tuple_size<typename std::remove_reference<Tuple>::type>::value,
      F,
      Tuple>::forEach(std::forward<Tuple>(tuple), std::forward<F>(f));
}

template <typename Protocol, typename IsSet>
struct Writer {
  Writer(Protocol* prot, const IsSet& isset) : prot_(prot), isset_(isset) {}
  template <typename FieldData>
  uint32_t operator()(const FieldData& fieldData, int index) {
    using Ops = Cpp2Ops<typename FieldData::ref_type>;

    if (!isset_.getIsSet(index)) {
      return 0;
    }

    int16_t fid = FieldData::fid;
    const auto& ex = fieldData.ref();

    uint32_t xfer = 0;
    xfer += prot_->writeFieldBegin("", Ops::thriftType(), fid);
    xfer += Ops::write(prot_, &ex);
    xfer += prot_->writeFieldEnd();
    return xfer;
  }

 private:
  Protocol* prot_;
  const IsSet& isset_;
};

template <typename Protocol, typename IsSet>
struct Sizer {
  Sizer(Protocol* prot, const IsSet& isset) : prot_(prot), isset_(isset) {}
  template <typename FieldData>
  uint32_t operator()(const FieldData& fieldData, int index) {
    using Ops = Cpp2Ops<typename FieldData::ref_type>;

    if (!isset_.getIsSet(index)) {
      return 0;
    }

    int16_t fid = FieldData::fid;
    const auto& ex = fieldData.ref();

    uint32_t xfer = 0;
    xfer += prot_->serializedFieldSize("", Ops::thriftType(), fid);
    xfer += Ops::serializedSize(prot_, &ex);
    return xfer;
  }

 private:
  Protocol* prot_;
  const IsSet& isset_;
};

template <typename Protocol, typename IsSet>
struct SizerZC {
  SizerZC(Protocol* prot, const IsSet& isset) : prot_(prot), isset_(isset) {}
  template <typename FieldData>
  uint32_t operator()(const FieldData& fieldData, int index) {
    using Ops = Cpp2Ops<typename FieldData::ref_type>;

    if (!isset_.getIsSet(index)) {
      return 0;
    }

    int16_t fid = FieldData::fid;
    const auto& ex = fieldData.ref();

    uint32_t xfer = 0;
    xfer += prot_->serializedFieldSize("", Ops::thriftType(), fid);
    xfer += Ops::serializedSizeZC(prot_, &ex);
    return xfer;
  }

 private:
  Protocol* prot_;
  const IsSet& isset_;
};

template <typename Protocol, typename IsSet>
struct Reader {
  Reader(
      Protocol* prot,
      IsSet& isset,
      int16_t fid,
      protocol::TType ftype,
      bool& success)
      : prot_(prot),
        isset_(isset),
        fid_(fid),
        ftype_(ftype),
        success_(success) {}
  template <typename FieldData>
  void operator()(FieldData& fieldData, int index) {
    using Ops = Cpp2Ops<typename FieldData::ref_type>;

    if (ftype_ != Ops::thriftType()) {
      return;
    }

    int16_t myfid = FieldData::fid;
    auto& ex = fieldData.ref();
    if (myfid != fid_) {
      return;
    }

    success_ = true;
    isset_.setIsSet(index);
    Ops::read(prot_, &ex);
  }

 private:
  Protocol* prot_;
  IsSet& isset_;
  int16_t fid_;
  protocol::TType ftype_;
  bool& success_;
};

template <typename T>
T& maybe_remove_pointer(T& x) {
  return x;
}

template <typename T>
T& maybe_remove_pointer(T* x) {
  return *x;
}

template <bool hasIsSet, size_t count>
struct IsSetHelper {
  void setIsSet(size_t /*index*/, bool /*value*/ = true) {}
  bool getIsSet(size_t /*index*/) const { return true; }
};

template <size_t count>
struct IsSetHelper<true, count> {
  void setIsSet(size_t index, bool value = true) { isset_[index] = value; }
  bool getIsSet(size_t index) const { return isset_[index]; }

 private:
  std::array<bool, count> isset_ = {};
};

} // namespace detail

template <int16_t Fid, typename TC, typename T>
struct FieldData {
  static const constexpr int16_t fid = Fid;
  static const constexpr protocol::TType ttype = protocol_type_v<TC, T>;
  typedef TC type_class;
  typedef T type;
  typedef typename std::remove_pointer<T>::type ref_type;
  T value;
  ref_type& ref() {
    return apache::thrift::detail::maybe_remove_pointer(value);
  }
  const ref_type& ref() const {
    return apache::thrift::detail::maybe_remove_pointer(value);
  }
};

template <bool hasIsSet, typename... Field>
class ThriftPresult
    : private std::tuple<Field...>,
      public apache::thrift::detail::IsSetHelper<hasIsSet, sizeof...(Field)> {
  // The fields tuple and IsSetHelper are base classes (rather than members)
  // to employ the empty base class optimization when they are empty
  typedef std::tuple<Field...> Fields;
  typedef apache::thrift::detail::IsSetHelper<hasIsSet, sizeof...(Field)>
      CurIsSetHelper;

 public:
  using size = std::tuple_size<Fields>;

  CurIsSetHelper& isSet() { return *this; }
  const CurIsSetHelper& isSet() const { return *this; }
  Fields& fields() { return *this; }
  const Fields& fields() const { return *this; }

  // returns lvalue ref to the appropriate FieldData
  template <size_t index>
  auto get() -> decltype(std::get<index>(this->fields())) {
    return std::get<index>(this->fields());
  }

  template <size_t index>
  auto get() const -> decltype(std::get<index>(this->fields())) {
    return std::get<index>(this->fields());
  }

  template <class Protocol>
  uint32_t read(Protocol* prot) {
    auto xfer = prot->getCursorPosition();
    std::string fname;
    apache::thrift::protocol::TType ftype;
    int16_t fid;

    prot->readStructBegin(fname);

    while (true) {
      prot->readFieldBegin(fname, ftype, fid);
      if (ftype == apache::thrift::protocol::T_STOP) {
        break;
      }
      bool readSomething = false;
      apache::thrift::detail::forEachVoid(
          fields(),
          apache::thrift::detail::Reader<Protocol, CurIsSetHelper>(
              prot, isSet(), fid, ftype, readSomething));
      if (!readSomething) {
        prot->skip(ftype);
      }
      prot->readFieldEnd();
    }
    prot->readStructEnd();

    return folly::to_narrow(prot->getCursorPosition() - xfer);
  }

  template <class Protocol>
  uint32_t serializedSize(Protocol* prot) const {
    uint32_t xfer = 0;
    xfer += prot->serializedStructSize("");
    xfer += apache::thrift::detail::forEach(
        fields(),
        apache::thrift::detail::Sizer<Protocol, CurIsSetHelper>(prot, isSet()));
    xfer += prot->serializedSizeStop();
    return xfer;
  }

  template <class Protocol>
  uint32_t serializedSizeZC(Protocol* prot) const {
    uint32_t xfer = 0;
    xfer += prot->serializedStructSize("");
    xfer += apache::thrift::detail::forEach(
        fields(),
        apache::thrift::detail::SizerZC<Protocol, CurIsSetHelper>(
            prot, isSet()));
    xfer += prot->serializedSizeStop();
    return xfer;
  }

  template <class Protocol>
  uint32_t write(Protocol* prot) const {
    uint32_t xfer = 0;
    xfer += prot->writeStructBegin("");
    xfer += apache::thrift::detail::forEach(
        fields(),
        apache::thrift::detail::Writer<Protocol, CurIsSetHelper>(
            prot, isSet()));
    xfer += prot->writeFieldStop();
    xfer += prot->writeStructEnd();
    return xfer;
  }
};

template <typename PResults, typename StreamPresult>
struct ThriftPResultStream {
  using StreamPResultType = StreamPresult;
  using FieldsType = PResults;

  PResults fields;
  StreamPresult stream;
};

template <
    typename PResults,
    typename SinkPresult,
    typename FinalResponsePresult>
struct ThriftPResultSink {
  using SinkPResultType = SinkPresult;
  using FieldsType = PResults;
  using FinalResponsePResultType = FinalResponsePresult;

  PResults fields;
  SinkPresult stream;
  FinalResponsePresult finalResponse;
};

template <bool hasIsSet, class... Args>
class Cpp2Ops<ThriftPresult<hasIsSet, Args...>> {
 public:
  typedef ThriftPresult<hasIsSet, Args...> Presult;
  static constexpr protocol::TType thriftType() { return protocol::T_STRUCT; }
  template <class Protocol>
  static uint32_t write(Protocol* prot, const Presult* value) {
    return value->write(prot);
  }
  template <class Protocol>
  static uint32_t read(Protocol* prot, Presult* value) {
    return value->read(prot);
  }
  template <class Protocol>
  static uint32_t serializedSize(Protocol* prot, const Presult* value) {
    return value->serializedSize(prot);
  }
  template <class Protocol>
  static uint32_t serializedSizeZC(Protocol* prot, const Presult* value) {
    return value->serializedSizeZC(prot);
  }
};

// Forward declaration
namespace detail {
namespace ap {

template <typename Protocol, typename PResult, typename T>
folly::Try<T> decode_stream_element(
    folly::Try<apache::thrift::StreamPayload>&& payload);

template <typename Protocol, typename PResult, typename T>
apache::thrift::ClientBufferedStream<T> decode_client_buffered_stream(
    apache::thrift::detail::ClientStreamBridge::ClientPtr streamBridge,
    const BufferOptions& bufferOptions);

template <typename Protocol, typename PResult, typename T>
std::unique_ptr<folly::IOBuf> encode_stream_payload(T&& _item);

template <typename Protocol, typename PResult>
std::unique_ptr<folly::IOBuf> encode_stream_payload(folly::IOBuf&& _item);

template <typename Protocol, typename PResult, typename ErrorMapFunc>
std::unique_ptr<folly::IOBuf> encode_stream_exception(
    folly::exception_wrapper ew);

template <typename Protocol, typename PResult, typename T>
T decode_stream_payload_impl(folly::IOBuf& payload, folly::tag_t<T>);

template <typename Protocol, typename PResult, typename T>
folly::IOBuf decode_stream_payload_impl(
    folly::IOBuf& payload, folly::tag_t<folly::IOBuf>);

template <typename Protocol, typename PResult, typename T>
T decode_stream_payload(folly::IOBuf& payload);

template <typename Protocol, typename PResult, typename T>
folly::exception_wrapper decode_stream_exception(folly::exception_wrapper ew);

struct EmptyExMapType {
  template <typename PResult>
  bool operator()(PResult&, folly::exception_wrapper) {
    return false;
  }
};

} // namespace ap
} // namespace detail

//  AsyncClient helpers
namespace detail {
namespace ac {

template <bool HasReturnType, typename PResult>
folly::exception_wrapper extract_exn(PResult& result) {
  using base = std::integral_constant<std::size_t, HasReturnType ? 1 : 0>;
  auto ew = folly::exception_wrapper();
  if (HasReturnType && result.getIsSet(0)) {
    return ew;
  }
  foreach_index<PResult::size::value - base::value>([&](auto index) {
    if (!ew && result.getIsSet(index.value + base::value)) {
      auto& fdata = result.template get<index.value + base::value>();
      ew = folly::exception_wrapper(std::move(fdata.ref()));
    }
  });
  if (!ew && HasReturnType) {
    ew = folly::make_exception_wrapper<TApplicationException>(
        TApplicationException::TApplicationExceptionType::MISSING_RESULT,
        "failed: unknown result");
  }
  return ew;
}

template <typename Protocol, typename PResult>
folly::exception_wrapper recv_wrapped_helper(
    const char* method,
    Protocol* prot,
    ClientReceiveState& state,
    PResult& result) {
  ContextStack* ctx = state.ctx();
  std::string fname;
  int32_t protoSeqId = 0;
  MessageType mtype;
  ctx->preRead();
  try {
    if (state.header() && state.header()->getCrc32c().has_value() &&
        checksum::crc32c(*state.buf()) != *state.header()->getCrc32c()) {
      return folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::TApplicationExceptionType::CHECKSUM_MISMATCH,
          "corrupted response");
    }
    prot->readMessageBegin(fname, mtype, protoSeqId);
    if (mtype == T_EXCEPTION) {
      TApplicationException x;
      apache::thrift::detail::deserializeExceptionBody(prot, &x);
      prot->readMessageEnd();
      return folly::exception_wrapper(std::move(x));
    }
    if (mtype != T_REPLY) {
      prot->skip(protocol::T_STRUCT);
      prot->readMessageEnd();
      return folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::TApplicationExceptionType::
              INVALID_MESSAGE_TYPE);
    }
    if (fname.compare(method) != 0) {
      prot->skip(protocol::T_STRUCT);
      prot->readMessageEnd();
      return folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::TApplicationExceptionType::WRONG_METHOD_NAME,
          folly::to<std::string>(
              "expected method: ", method, ", actual method: ", fname));
    }
    SerializedMessage smsg;
    smsg.protocolType = prot->protocolType();
    smsg.buffer = state.buf();
    ctx->onReadData(smsg);
    apache::thrift::detail::deserializeRequestBody(prot, &result);
    prot->readMessageEnd();
    ctx->postRead(
        state.header(),
        folly::to_narrow(state.buf()->computeChainDataLength()));
    return folly::exception_wrapper();
  } catch (std::exception const& e) {
    return folly::exception_wrapper(std::current_exception(), e);
  } catch (...) {
    return folly::exception_wrapper(std::current_exception());
  }
}

template <typename PResult, typename Protocol, typename... ReturnTs>
folly::exception_wrapper recv_wrapped(
    const char* method,
    Protocol* prot,
    ClientReceiveState& state,
    ReturnTs&... _returns) {
  prot->setInput(state.buf());
  auto guard = folly::makeGuard([&] { prot->setInput(nullptr); });
  apache::thrift::ContextStack* ctx = state.ctx();
  PResult result;
  foreach(
      [&](auto index, auto& obj) {
        result.template get<index.value>().value = &obj;
      },
      _returns...);
  auto ew = recv_wrapped_helper(method, prot, state, result);
  if (!ew) {
    constexpr auto const kHasReturnType = sizeof...(_returns) != 0;
    ew = apache::thrift::detail::ac::extract_exn<kHasReturnType>(result);
  }
  if (ew) {
    ctx->handlerErrorWrapped(ew);
  }
  return ew;
}

template <typename PResult, typename Protocol, typename Response, typename Item>
folly::exception_wrapper recv_wrapped(
    const char* method,
    Protocol* prot,
    ClientReceiveState& state,
    apache::thrift::ResponseAndClientBufferedStream<Response, Item>& _return) {
  prot->setInput(state.buf());
  auto guard = folly::makeGuard([&] { prot->setInput(nullptr); });
  apache::thrift::ContextStack* ctx = state.ctx();

  typename PResult::FieldsType result;
  result.template get<0>().value = &_return.response;

  auto ew = recv_wrapped_helper(method, prot, state, result);
  if (!ew) {
    ew = apache::thrift::detail::ac::extract_exn<true>(result);
  }
  if (ew) {
    ctx->handlerErrorWrapped(ew);
  }

  if (!ew) {
    _return.stream = apache::thrift::detail::ap::decode_client_buffered_stream<
        Protocol,
        typename PResult::StreamPResultType,
        Item>(state.extractStreamBridge(), state.bufferOptions());
  }
  return ew;
}

template <typename PResult, typename Protocol, typename Item>
folly::exception_wrapper recv_wrapped(
    const char* method,
    Protocol* prot,
    ClientReceiveState& state,
    apache::thrift::ClientBufferedStream<Item>& _return) {
  prot->setInput(state.buf());
  auto guard = folly::makeGuard([&] { prot->setInput(nullptr); });
  apache::thrift::ContextStack* ctx = state.ctx();

  typename PResult::FieldsType result;

  auto ew = recv_wrapped_helper(method, prot, state, result);
  if (!ew) {
    ew = apache::thrift::detail::ac::extract_exn<false>(result);
  }
  if (ew) {
    ctx->handlerErrorWrapped(ew);
  }

  if (!ew) {
    _return = apache::thrift::detail::ap::decode_client_buffered_stream<
        Protocol,
        typename PResult::StreamPResultType,
        Item>(state.extractStreamBridge(), state.bufferOptions());
  }
  return ew;
}

#if FOLLY_HAS_COROUTINES

template <
    typename ProtocolReader,
    typename ProtocolWriter,
    typename SinkPResult,
    typename SinkType,
    typename FinalResponsePResult,
    typename FinalResponseType,
    typename ErrorMapFunc>
ClientSink<SinkType, FinalResponseType> createSink(
    apache::thrift::detail::ClientSinkBridge::Ptr impl) {
  return ClientSink<SinkType, FinalResponseType>(
      std::move(impl),
      [](folly::Try<SinkType>&& item) mutable {
        if (item.hasValue()) {
          return ap::encode_stream_payload<ProtocolWriter, SinkPResult>(
              std::move(item).value());
        } else {
          return ap::encode_stream_exception<
              ProtocolWriter,
              SinkPResult,
              std::decay_t<ErrorMapFunc>>(std::move(item).exception());
        }
      },
      apache::thrift::detail::ap::decode_stream_element<
          ProtocolReader,
          FinalResponsePResult,
          FinalResponseType>);
}
#endif

template <
    typename PResult,
    typename ErrorMapFunc,
    typename ProtocolWriter,
    typename ProtocolReader,
    typename Response,
    typename Item,
    typename FinalResponse>
folly::exception_wrapper recv_wrapped(
    const char* method,
    ProtocolReader* prot,
    ClientReceiveState& state,
    apache::thrift::detail::ClientSinkBridge::Ptr impl,
    apache::thrift::ResponseAndClientSink<Response, Item, FinalResponse>&
        _return) {
#if FOLLY_HAS_COROUTINES
  prot->setInput(state.buf());
  auto guard = folly::makeGuard([&] { prot->setInput(nullptr); });
  apache::thrift::ContextStack* ctx = state.ctx();

  typename PResult::FieldsType result;
  result.template get<0>().value = &_return.response;

  auto ew = recv_wrapped_helper(method, prot, state, result);
  if (!ew) {
    ew = apache::thrift::detail::ac::extract_exn<true>(result);
  }
  if (ew) {
    ctx->handlerErrorWrapped(ew);
  }

  if (!ew) {
    _return.sink = createSink<
        ProtocolReader,
        ProtocolWriter,
        typename PResult::SinkPResultType,
        Item,
        typename PResult::FinalResponsePResultType,
        FinalResponse,
        std::decay_t<ErrorMapFunc>>(std::move(impl));
  }
  return ew;
#else
  (void)method;
  (void)prot;
  (void)state;
  (void)impl;
  (void)_return;
  std::terminate();
#endif
}

template <
    typename PResult,
    typename ErrorMapFunc,
    typename ProtocolWriter,
    typename ProtocolReader,
    typename Item,
    typename FinalResponse>
folly::exception_wrapper recv_wrapped(
    const char* method,
    ProtocolReader* prot,
    ClientReceiveState& state,
    apache::thrift::detail::ClientSinkBridge::Ptr impl,
    apache::thrift::ClientSink<Item, FinalResponse>& _return) {
#if FOLLY_HAS_COROUTINES
  prot->setInput(state.buf());
  auto guard = folly::makeGuard([&] { prot->setInput(nullptr); });
  apache::thrift::ContextStack* ctx = state.ctx();

  typename PResult::FieldsType result;

  auto ew = recv_wrapped_helper(method, prot, state, result);
  if (!ew) {
    ew = apache::thrift::detail::ac::extract_exn<false>(result);
  }
  if (ew) {
    ctx->handlerErrorWrapped(ew);
  }

  if (!ew) {
    _return = createSink<
        ProtocolReader,
        ProtocolWriter,
        typename PResult::SinkPResultType,
        Item,
        typename PResult::FinalResponsePResultType,
        FinalResponse,
        std::decay_t<ErrorMapFunc>>(std::move(impl));
  }
  return ew;
#else
  (void)method;
  (void)prot;
  (void)state;
  (void)impl;
  (void)_return;
  std::terminate();
#endif
}

[[noreturn]] void throw_app_exn(char const* msg);
} // namespace ac
} // namespace detail

//  AsyncProcessor helpers
namespace detail {
namespace ap {

//  Everything templated on only protocol goes here. The corresponding .cpp file
//  explicitly instantiates this struct for each supported protocol.
template <typename ProtocolReader, typename ProtocolWriter>
struct helper {
  static std::unique_ptr<folly::IOBuf> write_exn(
      const char* method,
      ProtocolWriter* prot,
      int32_t protoSeqId,
      ContextStack* ctx,
      const TApplicationException& x);

  static void process_exn(
      const char* func,
      const TApplicationException::TApplicationExceptionType type,
      const std::string& msg,
      ResponseChannelRequest::UniquePtr req,
      Cpp2RequestContext* ctx,
      folly::EventBase* eb,
      int32_t protoSeqId);
};

template <typename ProtocolReader>
using writer_of = typename ProtocolReader::ProtocolWriter;
template <typename ProtocolWriter>
using reader_of = typename ProtocolWriter::ProtocolReader;

template <typename ProtocolReader>
using helper_r = helper<ProtocolReader, writer_of<ProtocolReader>>;
template <typename ProtocolWriter>
using helper_w = helper<reader_of<ProtocolWriter>, ProtocolWriter>;

template <typename T>
using is_root_async_processor = std::is_void<typename T::BaseAsyncProcessor>;

template <class ProtocolReader, class Processor>
typename std::enable_if<is_root_async_processor<Processor>::value>::type
process_missing(
    Processor*,
    const std::string& fname,
    ResponseChannelRequest::UniquePtr req,
    apache::thrift::SerializedCompressedRequest&&,
    Cpp2RequestContext*,
    folly::EventBase* eb,
    concurrency::ThreadManager*) {
  if (req) {
    eb->runInEventBaseThread([request = move(req),
                              msg = fmt::format(
                                  "Method name {} not found", fname)]() {
      request->sendErrorWrapped(
          folly::make_exception_wrapper<TApplicationException>(
              TApplicationException::TApplicationExceptionType::UNKNOWN_METHOD,
              msg),
          kMethodUnknownErrorCode);
    });
  }
}

template <class ProtocolReader, class Processor>
typename std::enable_if<!is_root_async_processor<Processor>::value>::type
process_missing(
    Processor* processor,
    const std::string& /*fname*/,
    ResponseChannelRequest::UniquePtr req,
    apache::thrift::SerializedCompressedRequest&& serializedRequest,
    Cpp2RequestContext* ctx,
    folly::EventBase* eb,
    concurrency::ThreadManager* tm) {
  auto protType = ProtocolReader::protocolType();
  processor->Processor::BaseAsyncProcessor::processSerializedCompressedRequest(
      std::move(req), std::move(serializedRequest), protType, ctx, eb, tm);
}

struct MessageBegin {
  MessageBegin() {}
  MessageBegin(const MessageBegin&) = delete;
  MessageBegin& operator=(const MessageBegin&) = delete;
  MessageBegin(MessageBegin&&) = default;
  MessageBegin& operator=(MessageBegin&&) = default;
  bool isValid{true};
  size_t size{0};
  std::string methodName;
  MessageType msgType;
  int32_t seqId{0};
  std::string errMessage;
};

bool setupRequestContextWithMessageBegin(
    const MessageBegin& msgBegin,
    protocol::PROTOCOL_TYPES protType,
    ResponseChannelRequest::UniquePtr& req,
    Cpp2RequestContext* ctx,
    folly::EventBase* eb);

MessageBegin deserializeMessageBegin(
    const folly::IOBuf& buf, protocol::PROTOCOL_TYPES protType);

template <class ProtocolReader, class Processor>
void process_pmap(
    Processor* proc,
    const typename GeneratedAsyncProcessor::ProcessMap<
        GeneratedAsyncProcessor::ProcessFunc<Processor>>& pmap,
    ResponseChannelRequest::UniquePtr req,
    apache::thrift::SerializedCompressedRequest&& serializedRequest,
    Cpp2RequestContext* ctx,
    folly::EventBase* eb,
    concurrency::ThreadManager* tm) {
  const auto& fname = ctx->getMethodName();
  auto pfn = pmap.find(fname);
  if (pfn == pmap.end()) {
    process_missing<ProtocolReader>(
        proc, fname, std::move(req), std::move(serializedRequest), ctx, eb, tm);
    return;
  }

  (proc->*(pfn->second))(
      std::move(req), std::move(serializedRequest), ctx, eb, tm);
}

//  Generated AsyncProcessor::process just calls this.
template <class Processor>
void process(
    Processor* processor,
    ResponseChannelRequest::UniquePtr req,
    apache::thrift::SerializedCompressedRequest&& serializedRequest,
    protocol::PROTOCOL_TYPES protType,
    Cpp2RequestContext* ctx,
    folly::EventBase* eb,
    concurrency::ThreadManager* tm) {
  switch (protType) {
    case protocol::T_BINARY_PROTOCOL: {
      const auto& pmap = processor->getBinaryProtocolProcessMap();
      return process_pmap<BinaryProtocolReader>(
          processor,
          pmap,
          std::move(req),
          std::move(serializedRequest),
          ctx,
          eb,
          tm);
    }
    case protocol::T_COMPACT_PROTOCOL: {
      const auto& pmap = processor->getCompactProtocolProcessMap();
      return process_pmap<CompactProtocolReader>(
          processor,
          pmap,
          std::move(req),
          std::move(serializedRequest),
          ctx,
          eb,
          tm);
    }
    default:
      LOG(ERROR) << "invalid protType: " << folly::to_underlying(protType);
      return;
  }
}

template <typename Protocol, typename PResult, typename T>
std::unique_ptr<folly::IOBuf> encode_stream_payload(T&& _item) {
  PResult res;
  res.template get<0>().value = const_cast<T*>(&_item);
  res.setIsSet(0);

  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  Protocol prot;
  prot.setOutput(&queue, res.serializedSizeZC(&prot));

  res.write(&prot);
  return std::move(queue).move();
}

template <typename Protocol, typename PResult>
std::unique_ptr<folly::IOBuf> encode_stream_payload(folly::IOBuf&& _item) {
  return std::make_unique<folly::IOBuf>(std::move(_item));
}

template <typename Protocol, typename PResult, typename ErrorMapFunc>
std::unique_ptr<folly::IOBuf> encode_stream_exception(
    folly::exception_wrapper ew) {
  ErrorMapFunc mapException;
  Protocol prot;
  folly::IOBufQueue queue(folly::IOBufQueue::cacheChainLength());
  PResult res;
  if (mapException(res, ew)) {
    prot.setOutput(&queue, res.serializedSizeZC(&prot));
    res.write(&prot);
  } else {
    constexpr size_t kQueueAppenderGrowth = 4096;
    prot.setOutput(&queue, kQueueAppenderGrowth);
    TApplicationException ex(ew.what().toStdString());
    apache::thrift::detail::serializeExceptionBody(&prot, &ex);
  }

  return std::move(queue).move();
}

template <
    typename Protocol,
    typename PResult,
    typename T,
    typename ErrorMapFunc>
folly::Try<StreamPayload> encode_server_stream_payload(folly::Try<T>&& val) {
  if (val.hasValue()) {
    return folly::Try<StreamPayload>(
        {encode_stream_payload<Protocol, PResult>(std::move(*val)), {}});
  } else if (val.hasException()) {
    return folly::Try<StreamPayload>(folly::exception_wrapper(
        EncodedError(encode_stream_exception<Protocol, PResult, ErrorMapFunc>(
            val.exception()))));
  } else {
    return folly::Try<StreamPayload>();
  }
}

template <typename Protocol, typename PResult, typename T>
T decode_stream_payload_impl(folly::IOBuf& payload, folly::tag_t<T>) {
  PResult args;
  T res{};
  args.template get<0>().value = &res;

  Protocol prot;
  prot.setInput(&payload);
  args.read(&prot);
  return res;
}

template <typename Protocol, typename PResult, typename T>
folly::IOBuf decode_stream_payload_impl(
    folly::IOBuf& payload, folly::tag_t<folly::IOBuf>) {
  return std::move(payload);
}

template <typename Protocol, typename PResult, typename T>
T decode_stream_payload(folly::IOBuf& payload) {
  return decode_stream_payload_impl<Protocol, PResult, T>(
      payload, folly::tag_t<T>{});
}

template <typename Protocol, typename PResult, typename T>
folly::exception_wrapper decode_stream_exception(folly::exception_wrapper ew) {
  Protocol prot;
  folly::exception_wrapper hijacked;
  ew.with_exception(
      [&hijacked, &prot](apache::thrift::detail::EncodedError& err) {
        PResult result;
        T res{};
        result.template get<0>().value = &res;

        prot.setInput(err.encoded.get());
        result.read(&prot);

        CHECK(!result.getIsSet(0));

        foreach_index<PResult::size::value - 1>([&](auto index) {
          if (!hijacked && result.getIsSet(index.value + 1)) {
            auto& fdata = result.template get<index.value + 1>();
            hijacked = folly::exception_wrapper(std::move(fdata.ref()));
          }
        });

        if (!hijacked) {
          // Could not decode the error. It may be a TApplicationException
          TApplicationException x;
          prot.setInput(err.encoded.get());
          apache::thrift::detail::deserializeExceptionBody(&prot, &x);
          hijacked = folly::exception_wrapper(std::move(x));
        }
      });

  if (hijacked) {
    return hijacked;
  }

  return ew;
}

template <
    typename Protocol,
    typename PResult,
    typename ErrorMapFunc,
    typename T>
ServerStreamFactory encode_server_stream(
    apache::thrift::ServerStream<T>&& stream,
    folly::Executor::KeepAlive<> serverExecutor) {
  return stream(
      std::move(serverExecutor),
      encode_server_stream_payload<Protocol, PResult, T, ErrorMapFunc>);
}

template <typename Protocol, typename PResult, typename T>
folly::Try<T> decode_stream_element(
    folly::Try<apache::thrift::StreamPayload>&& payload) {
  if (payload.hasValue()) {
    return folly::Try<T>(
        decode_stream_payload<Protocol, PResult, T>(*payload->payload));
  } else if (payload.hasException()) {
    return folly::Try<T>(decode_stream_exception<Protocol, PResult, T>(
        std::move(payload).exception()));
  } else {
    return folly::Try<T>();
  }
}

template <typename Protocol, typename PResult, typename T>
apache::thrift::ClientBufferedStream<T> decode_client_buffered_stream(
    apache::thrift::detail::ClientStreamBridge::ClientPtr streamBridge,
    const BufferOptions& bufferOptions) {
  return apache::thrift::ClientBufferedStream<T>(
      std::move(streamBridge),
      decode_stream_element<Protocol, PResult, T>,
      bufferOptions);
}

template <
    typename ProtocolReader,
    typename ProtocolWriter,
    typename SinkPResult,
    typename FinalResponsePResult,
    typename ErrorMapFunc,
    typename SinkType,
    typename FinalResponseType>
apache::thrift::detail::SinkConsumerImpl toSinkConsumerImpl(
    FOLLY_MAYBE_UNUSED SinkConsumer<SinkType, FinalResponseType>&& sinkConsumer,
    FOLLY_MAYBE_UNUSED folly::Executor::KeepAlive<> executor) {
#if FOLLY_HAS_COROUTINES
  auto consumer =
      [innerConsumer = std::move(sinkConsumer.consumer)](
          folly::coro::AsyncGenerator<folly::Try<StreamPayload>&&> gen) mutable
      -> folly::coro::Task<folly::Try<StreamPayload>> {
    folly::exception_wrapper ew;
    try {
      FinalResponseType finalResponse = co_await innerConsumer(
          [](folly::coro::AsyncGenerator<folly::Try<StreamPayload>&&> gen_)
              -> folly::coro::AsyncGenerator<SinkType&&> {
            while (auto item = co_await gen_.next()) {
              auto payload = std::move(*item);
              if (payload.hasValue()) {
                // if exception is thrown there, it will propagate to inner
                // consumer which is intended
                co_yield ap::decode_stream_payload<
                    ProtocolReader,
                    SinkPResult,
                    SinkType>(*payload->payload);
              } else {
                ap::decode_stream_exception<
                    ProtocolReader,
                    SinkPResult,
                    SinkType>(payload.exception())
                    .throw_exception();
              }
            }
          }(std::move(gen)));
      co_return folly::Try<StreamPayload>(StreamPayload(
          ap::encode_stream_payload<ProtocolWriter, FinalResponsePResult>(
              std::move(finalResponse)),
          {}));
    } catch (std::exception& e) {
      ew = folly::exception_wrapper(std::current_exception(), e);
    } catch (...) {
      ew = folly::exception_wrapper(std::current_exception());
    }
    co_return folly::Try<StreamPayload>(rocket::RocketException(
        rocket::ErrorCode::APPLICATION_ERROR,
        ap::encode_stream_exception<
            ProtocolWriter,
            FinalResponsePResult,
            ErrorMapFunc>(std::move(ew))));
  };
  return apache::thrift::detail::SinkConsumerImpl{
      std::move(consumer),
      sinkConsumer.bufferSize,
      sinkConsumer.sinkOptions.chunkTimeout,
      std::move(executor)};
#else
  std::terminate();
#endif
}

} // namespace ap
} // namespace detail

//  ServerInterface helpers
namespace detail {
namespace si {

template <typename F>
using ret = typename folly::invoke_result_t<F>;
template <typename F>
using ret_lift = typename folly::lift_unit<ret<F>>::type;
template <typename F>
using fut_ret = typename ret<F>::value_type;
template <typename F>
using fut_ret_drop = typename folly::drop_unit<fut_ret<F>>::type;
template <typename T>
struct action_traits_impl;
template <typename C, typename A>
struct action_traits_impl<void (C::*)(A&) const> {
  using arg_type = A;
};
template <typename C, typename A>
struct action_traits_impl<void (C::*)(A&)> {
  using arg_type = A;
};
template <typename F>
using action_traits = action_traits_impl<decltype(&F::operator())>;
template <typename F>
using arg = typename action_traits<F>::arg_type;

template <typename T>
folly::Future<T> future(
    folly::SemiFuture<T>&& future, folly::Executor::KeepAlive<> keepAlive) {
  if (future.isReady()) {
    return std::move(future).toUnsafeFuture();
  }
  return std::move(future).via(keepAlive);
}

template <class F>
folly::SemiFuture<ret_lift<F>> semifuture(F&& f) {
  return folly::makeSemiFutureWith(std::forward<F>(f));
}

template <class F>
arg<F> returning(F&& f) {
  arg<F> ret;
  f(ret);
  return ret;
}

template <class F>
folly::SemiFuture<arg<F>> semifuture_returning(F&& f) {
  return semifuture([&]() { return returning(std::forward<F>(f)); });
}

template <class F>
std::unique_ptr<arg<F>> returning_uptr(F&& f) {
  auto ret = std::make_unique<arg<F>>();
  f(*ret);
  return ret;
}

template <class F>
folly::SemiFuture<std::unique_ptr<arg<F>>> semifuture_returning_uptr(F&& f) {
  return semifuture([&]() { return returning_uptr(std::forward<F>(f)); });
}

using CallbackBase = HandlerCallbackBase;
using CallbackBasePtr = std::unique_ptr<CallbackBase>;
template <class R>
using Callback = HandlerCallback<fut_ret_drop<R>>;
template <class R>
using CallbackPtr = std::unique_ptr<Callback<R>>;

inline void async_tm_prep(ServerInterface* si, CallbackBase* callback) {
  si->setEventBase(callback->getEventBase());
  si->setThreadManager(callback->getThreadManager());
  si->setRequestContext(callback->getRequestContext());
}

template <class F>
auto makeFutureWithAndMaybeReschedule(CallbackBase& callback, F&& f) {
  auto fut = folly::makeFutureWith(std::forward<F>(f));
  if (fut.isReady()) {
    return fut;
  }
  auto tm = callback.getThreadManager();
  auto ka = tm->getKeepAlive(
      callback.getRequestContext()->getRequestExecutionScope(),
      apache::thrift::concurrency::ThreadManager::Source::INTERNAL);
  return std::move(fut).via(std::move(ka));
}

template <class F>
void async_tm_future_oneway(CallbackBasePtr callback, F&& f) {
  makeFutureWithAndMaybeReschedule(*callback.get(), std::forward<F>(f))
      .thenValueInline([cb = std::move(callback)](auto&&) {});
}

template <class F>
void async_tm_future(CallbackPtr<F> callback, F&& f) {
  makeFutureWithAndMaybeReschedule(*callback.get(), std::forward<F>(f))
      .thenTryInline([cb = std::move(callback)](folly::Try<fut_ret<F>>&& _ret) {
        cb->complete(std::move(_ret));
      });
}

template <class F>
void async_tm_semifuture_oneway(CallbackBasePtr callback, F&& f) {
  auto scope = callback->getRequestContext()->getRequestExecutionScope();
  auto ka = callback->getThreadManager()->getKeepAlive(
      std::move(scope),
      apache::thrift::concurrency::ThreadManager::Source::INTERNAL);
  apache::thrift::detail::si::future(
      folly::makeSemiFutureWith(std::forward<F>(f)), std::move(ka))
      .thenValueInline([cb = std::move(callback)](auto&&) {});
}

template <class F>
void async_tm_semifuture(CallbackPtr<F> callback, F&& f) {
  auto scope = callback->getRequestContext()->getRequestExecutionScope();
  auto ka = callback->getThreadManager()->getKeepAlive(
      std::move(scope),
      apache::thrift::concurrency::ThreadManager::Source::INTERNAL);
  apache::thrift::detail::si::future(
      folly::makeSemiFutureWith(std::forward<F>(f)), std::move(ka))
      .thenTryInline([cb = std::move(callback)](folly::Try<fut_ret<F>>&& _ret) {
        cb->complete(std::move(_ret));
      });
}

#if FOLLY_HAS_COROUTINES
template <typename T>
void async_tm_coro_oneway(
    folly::coro::Task<T>&& task, CallbackBasePtr&& callback) {
  auto scope = callback->getRequestContext()->getRequestExecutionScope();
  auto executor = callback->getThreadManager()->getKeepAlive(
      std::move(scope),
      apache::thrift::concurrency::ThreadManager::Source::INTERNAL);
  std::move(task)
      .scheduleOn(std::move(executor))
      .startInlineUnsafe([callback = std::move(callback)](
                             folly::Try<folly::lift_unit_t<T>>&&) mutable {});
}

template <typename T>
void async_tm_coro(
    folly::coro::Task<T>&& task,
    std::unique_ptr<apache::thrift::HandlerCallback<T>>&& callback) {
  auto scope = callback->getRequestContext()->getRequestExecutionScope();
  auto executor = callback->getThreadManager()->getKeepAlive(
      std::move(scope),
      apache::thrift::concurrency::ThreadManager::Source::INTERNAL);
  std::move(task)
      .scheduleOn(std::move(executor))
      .startInlineUnsafe([callback = std::move(callback)](
                             folly::Try<folly::lift_unit_t<T>>&& tryResult) {
        callback->complete(std::move(tryResult));
      });
}
#endif

[[noreturn]] void throw_app_exn_unimplemented(char const* name);
} // namespace si
} // namespace detail

namespace util {

namespace detail {

constexpr ErrorKind fromExceptionKind(ExceptionKind kind) {
  switch (kind) {
    case ExceptionKind::TRANSIENT:
      return ErrorKind::TRANSIENT;

    case ExceptionKind::STATEFUL:
      return ErrorKind::STATEFUL;

    case ExceptionKind::PERMANENT:
      return ErrorKind::PERMANENT;

    default:
      return ErrorKind::UNSPECIFIED;
  }
}

constexpr ErrorBlame fromExceptionBlame(ExceptionBlame blame) {
  switch (blame) {
    case ExceptionBlame::SERVER:
      return ErrorBlame::SERVER;

    case ExceptionBlame::CLIENT:
      return ErrorBlame::CLIENT;

    default:
      return ErrorBlame::UNSPECIFIED;
  }
}

constexpr ErrorSafety fromExceptionSafety(ExceptionSafety safety) {
  switch (safety) {
    case ExceptionSafety::SAFE:
      return ErrorSafety::SAFE;

    default:
      return ErrorSafety::UNSPECIFIED;
  }
}

template <typename T>
std::string serializeExceptionMeta() {
  ErrorClassification errorClassification;

  constexpr auto errorKind = apache::thrift::detail::st::struct_private_access::
      __fbthrift_cpp2_gen_exception_kind<T>();
  errorClassification.kind_ref() = fromExceptionKind(errorKind);
  constexpr auto errorBlame = apache::thrift::detail::st::
      struct_private_access::__fbthrift_cpp2_gen_exception_blame<T>();
  errorClassification.blame_ref() = fromExceptionBlame(errorBlame);
  constexpr auto errorSafety = apache::thrift::detail::st::
      struct_private_access::__fbthrift_cpp2_gen_exception_safety<T>();
  errorClassification.safety_ref() = fromExceptionSafety(errorSafety);

  return apache::thrift::detail::serializeErrorClassification(
      errorClassification);
}

} // namespace detail

void appendExceptionToHeader(
    const folly::exception_wrapper& ew, Cpp2RequestContext& ctx);

template <typename T>
void appendErrorClassificationToHeader(Cpp2RequestContext& ctx) {
  auto header = ctx.getHeader();
  if (!header) {
    return;
  }
  auto exMeta = detail::serializeExceptionMeta<T>();
  header->setHeader(
      std::string(apache::thrift::detail::kHeaderExMeta), std::move(exMeta));
}

TApplicationException toTApplicationException(
    const folly::exception_wrapper& ew);

} // namespace util

} // namespace thrift
} // namespace apache
