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

#include <thrift/lib/cpp2/transport/rocket/server/ThriftRocketServerHandler.h>

#include <memory>
#include <utility>

#include <fmt/core.h>
#include <folly/ExceptionString.h>
#include <folly/ExceptionWrapper.h>
#include <folly/GLog.h>
#include <folly/io/Cursor.h>
#include <folly/io/IOBuf.h>

#include <thrift/lib/cpp/TApplicationException.h>
#include <thrift/lib/cpp2/Flags.h>
#include <thrift/lib/cpp2/protocol/CompactProtocol.h>
#include <thrift/lib/cpp2/server/Cpp2ConnContext.h>
#include <thrift/lib/cpp2/server/Cpp2Worker.h>
#include <thrift/lib/cpp2/server/LoggingEventHelper.h>
#include <thrift/lib/cpp2/server/MonitoringMethodNames.h>
#include <thrift/lib/cpp2/server/VisitorHelper.h>
#include <thrift/lib/cpp2/transport/core/ThriftRequest.h>
#include <thrift/lib/cpp2/transport/rocket/PayloadUtils.h>
#include <thrift/lib/cpp2/transport/rocket/RocketException.h>
#include <thrift/lib/cpp2/transport/rocket/framing/ErrorCode.h>
#include <thrift/lib/cpp2/transport/rocket/framing/Frames.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerConnection.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketServerFrameContext.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketSinkClientCallback.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketStreamClientCallback.h>
#include <thrift/lib/cpp2/transport/rocket/server/RocketThriftRequests.h>
#include <thrift/lib/cpp2/util/Checksum.h>
#include <thrift/lib/thrift/gen-cpp2/RpcMetadata_constants.h>

namespace {
const int64_t kRocketServerMaxVersion = 7;
}

THRIFT_FLAG_DEFINE_bool(rocket_server_legacy_protocol_key, true);
THRIFT_FLAG_DEFINE_int64(rocket_server_max_version, kRocketServerMaxVersion);

THRIFT_FLAG_DEFINE_int64(monitoring_over_user_logging_sample_rate, 0);

namespace apache {
namespace thrift {
namespace rocket {

thread_local uint32_t ThriftRocketServerHandler::sample_{0};

namespace {
bool isMetadataValid(const RequestRpcMetadata& metadata) {
  return metadata.protocol_ref() && metadata.name_ref() && metadata.kind_ref();
}
} // namespace

ThriftRocketServerHandler::ThriftRocketServerHandler(
    std::shared_ptr<Cpp2Worker> worker,
    const folly::SocketAddress& clientAddress,
    const folly::AsyncTransport* transport,
    const std::vector<std::unique_ptr<SetupFrameHandler>>& handlers)
    : worker_(std::move(worker)),
      connectionGuard_(worker_->getActiveRequestsGuard()),
      clientAddress_(clientAddress),
      connContext_(
          &clientAddress_,
          transport,
          nullptr, /* eventBaseManager */
          nullptr, /* duplexChannel */
          nullptr, /* x509PeerCert */
          worker_->getServer()->getClientIdentityHook(),
          worker_.get()),
      setupFrameHandlers_(handlers),
      version_(static_cast<int32_t>(std::min(
          kRocketServerMaxVersion, THRIFT_FLAG(rocket_server_max_version)))) {
  connContext_.setTransportType(Cpp2ConnContext::TransportType::ROCKET);
  if (auto* handler = worker_->getServer()->getEventHandlerUnsafe()) {
    handler->newConnection(&connContext_);
  }
}

ThriftRocketServerHandler::~ThriftRocketServerHandler() {
  if (auto* handler = worker_->getServer()->getEventHandlerUnsafe()) {
    handler->connectionDestroyed(&connContext_);
  }
  // Ensure each connAccepted() call has a matching connClosed()
  if (auto* observer = worker_->getServer()->getObserver()) {
    observer->connClosed();
  }
}

apache::thrift::server::TServerObserver::SamplingStatus
ThriftRocketServerHandler::shouldSample() {
  bool isServerSamplingEnabled =
      (sampleRate_ > 0) && ((sample_++ % sampleRate_) == 0);

  // TODO: determine isClientSamplingEnabled by "client_logging_enabled" header
  return apache::thrift::server::TServerObserver::SamplingStatus(
      isServerSamplingEnabled, false);
}

void ThriftRocketServerHandler::handleSetupFrame(
    SetupFrame&& frame, RocketServerConnection& connection) {
  if (!frame.payload().hasNonemptyMetadata()) {
    return connection.close(folly::make_exception_wrapper<RocketException>(
        ErrorCode::INVALID_SETUP, "Missing required metadata in SETUP frame"));
  }

  folly::io::Cursor cursor(frame.payload().buffer());

  // Validate Thrift protocol key
  uint32_t protocolKey;
  const bool success = cursor.tryReadBE<uint32_t>(protocolKey);
  constexpr uint32_t kLegacyRocketProtocolKey = 1;
  if (!success ||
      ((!THRIFT_FLAG(rocket_server_legacy_protocol_key) ||
        protocolKey != kLegacyRocketProtocolKey) &&
       protocolKey != RpcMetadata_constants::kRocketProtocolKey())) {
    return connection.close(folly::make_exception_wrapper<RocketException>(
        ErrorCode::INVALID_SETUP, "Incompatible Thrift version"));
  }

  try {
    CompactProtocolReader reader;
    reader.setInput(cursor);
    RequestSetupMetadata meta;
    // Throws on read error
    meta.read(&reader);
    if (reader.getCursorPosition() > frame.payload().metadataSize()) {
      return connection.close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID_SETUP,
          "Error deserializing SETUP payload: underflow"));
    }
    connContext_.readSetupMetadata(meta);

    auto minVersion = meta.minVersion_ref().value_or(0);
    auto maxVersion = meta.maxVersion_ref().value_or(0);

    THRIFT_CONNECTION_EVENT(rocket.setup).log(connContext_, [&] {
      return folly::dynamic::object("client_min_version", minVersion)(
          "client_max_version", maxVersion)("server_version", version_);
    });

    if (minVersion > version_) {
      return connection.close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID_SETUP, "Incompatible Rocket version"));
    }

    if (maxVersion < 0) {
      return connection.close(folly::make_exception_wrapper<RocketException>(
          ErrorCode::INVALID_SETUP, "Incompatible Rocket version"));
    }
    version_ = std::min(version_, maxVersion);
    eventBase_ = connContext_.getTransport()->getEventBase();
    for (const auto& h : setupFrameHandlers_) {
      auto processorInfo = h->tryHandle(meta);
      if (processorInfo) {
        bool valid = true;
        valid &= !!(cpp2Processor_ = std::move(processorInfo->cpp2Processor_));
        valid &= !!(threadManager_ = std::move(processorInfo->threadManager_));
        valid &= !!(serverConfigs_ = &processorInfo->serverConfigs_);
        valid &= !!(requestsRegistry_ = processorInfo->requestsRegistry_);
        if (!valid) {
          return connection.close(
              folly::make_exception_wrapper<RocketException>(
                  ErrorCode::INVALID_SETUP,
                  "Error in implementation of custom connection handler."));
        }
        return;
      }
    }
    // no custom frame handler was found, do the default
    cpp2Processor_ = worker_->getServer()->getCpp2Processor();
    threadManager_ = worker_->getServer()->getThreadManager();
    serverConfigs_ = worker_->getServer();
    requestsRegistry_ = worker_->getRequestsRegistry();
    // add sampleRate
    if (serverConfigs_) {
      if (auto* observer = serverConfigs_->getObserver()) {
        sampleRate_ = observer->getSampleRate();
      }
    }

    if (meta.dscpToReflect_ref() || meta.markToReflect_ref()) {
      connection.applyDscpAndMarkToSocket(meta);
    }

    if (version_ >= 5) {
      ServerPushMetadata serverMeta;
      serverMeta.set_setupResponse();
      serverMeta.setupResponse_ref()->version_ref() = version_;
      CompactProtocolWriter compactProtocolWriter;
      folly::IOBufQueue queue;
      compactProtocolWriter.setOutput(&queue);
      serverMeta.write(&compactProtocolWriter);
      connection.sendMetadataPush(std::move(queue).move());
    }

  } catch (const std::exception& e) {
    return connection.close(folly::make_exception_wrapper<RocketException>(
        ErrorCode::INVALID_SETUP,
        fmt::format(
            "Error deserializing SETUP payload: {}",
            folly::exceptionStr(e).toStdString())));
  }
}

void ThriftRocketServerHandler::handleRequestResponseFrame(
    RequestResponseFrame&& frame, RocketServerFrameContext&& context) {
  auto makeRequestResponse = [&](RequestRpcMetadata&& md,
                                 rocket::Payload&& debugPayload,
                                 std::shared_ptr<folly::RequestContext> ctx) {
    // Note, we're passing connContext by reference and rely on the next
    // chain of ownership to keep it alive: ThriftServerRequestResponse
    // stores RocketServerFrameContext, which keeps refcount on
    // RocketServerConnection, which in turn keeps ThriftRocketServerHandler
    // alive, which in turn keeps connContext_ alive.
    return RequestsRegistry::makeRequest<ThriftServerRequestResponse>(
        *eventBase_,
        *serverConfigs_,
        std::move(md),
        connContext_,
        std::move(ctx),
        *requestsRegistry_,
        std::move(debugPayload),
        std::move(context),
        version_);
  };

  handleRequestCommon(
      std::move(frame.payload()), std::move(makeRequestResponse));
}

void ThriftRocketServerHandler::handleRequestFnfFrame(
    RequestFnfFrame&& frame, RocketServerFrameContext&& context) {
  auto makeRequestFnf = [&](RequestRpcMetadata&& md,
                            rocket::Payload&& debugPayload,
                            std::shared_ptr<folly::RequestContext> ctx) {
    // Note, we're passing connContext by reference and rely on a complex
    // chain of ownership (see handleRequestResponseFrame for detailed
    // explanation).
    return RequestsRegistry::makeRequest<ThriftServerRequestFnf>(
        *eventBase_,
        *serverConfigs_,
        std::move(md),
        connContext_,
        std::move(ctx),
        *requestsRegistry_,
        std::move(debugPayload),
        std::move(context),
        [keepAlive = cpp2Processor_] {});
  };

  handleRequestCommon(std::move(frame.payload()), std::move(makeRequestFnf));
}

void ThriftRocketServerHandler::handleRequestStreamFrame(
    RequestStreamFrame&& frame,
    RocketServerFrameContext&& context,
    RocketStreamClientCallback* clientCallback) {
  auto makeRequestStream = [&](RequestRpcMetadata&& md,
                               rocket::Payload&& debugPayload,
                               std::shared_ptr<folly::RequestContext> ctx) {
    return RequestsRegistry::makeRequest<ThriftServerRequestStream>(
        *eventBase_,
        *serverConfigs_,
        std::move(md),
        connContext_,
        std::move(ctx),
        *requestsRegistry_,
        std::move(debugPayload),
        std::move(context),
        version_,
        clientCallback,
        cpp2Processor_);
  };

  handleRequestCommon(std::move(frame.payload()), std::move(makeRequestStream));
}

void ThriftRocketServerHandler::handleRequestChannelFrame(
    RequestChannelFrame&& frame,
    RocketServerFrameContext&& context,
    RocketSinkClientCallback* clientCallback) {
  auto makeRequestSink = [&](RequestRpcMetadata&& md,
                             rocket::Payload&& debugPayload,
                             std::shared_ptr<folly::RequestContext> ctx) {
    return RequestsRegistry::makeRequest<ThriftServerRequestSink>(
        *eventBase_,
        *serverConfigs_,
        std::move(md),
        connContext_,
        std::move(ctx),
        *requestsRegistry_,
        std::move(debugPayload),
        std::move(context),
        version_,
        clientCallback,
        cpp2Processor_);
  };

  handleRequestCommon(std::move(frame.payload()), std::move(makeRequestSink));
}

void ThriftRocketServerHandler::connectionClosing() {
  connContext_.connectionClosed();
  if (cpp2Processor_) {
    cpp2Processor_->destroyAllInteractions(connContext_, *eventBase_);
  }
}

template <class F>
void ThriftRocketServerHandler::handleRequestCommon(
    Payload&& payload, F&& makeRequest) {
  // setup request sampling for counters and stats
  auto samplingStatus = shouldSample();
  std::chrono::steady_clock::time_point readEnd;
  if (UNLIKELY(samplingStatus.isEnabled())) {
    readEnd = std::chrono::steady_clock::now();
  }

  auto baseReqCtx = cpp2Processor_->getBaseContextForRequest();
  auto rootid = requestsRegistry_->genRootId();
  auto reqCtx = baseReqCtx
      ? folly::RequestContext::copyAsRoot(*baseReqCtx, rootid)
      : std::make_shared<folly::RequestContext>(rootid);

  folly::RequestContextScopeGuard rctx(reqCtx);

  rocket::Payload debugPayload = payload.clone();
  auto requestPayloadTry =
      unpackAsCompressed<RequestPayload>(std::move(payload));

  auto makeActiveRequest = [&](auto&& md, auto&& payload, auto&& reqCtx) {
    serverConfigs_->incActiveRequests();
    return makeRequest(std::move(md), std::move(payload), std::move(reqCtx));
  };

  if (requestPayloadTry.hasException()) {
    handleDecompressionFailure(
        makeActiveRequest(
            RequestRpcMetadata(), rocket::Payload{}, std::move(reqCtx)),
        requestPayloadTry.exception().what().toStdString());
    return;
  }

  auto& data = requestPayloadTry->payload;
  auto& metadata = requestPayloadTry->metadata;

  if (!isMetadataValid(metadata)) {
    handleRequestWithBadMetadata(makeActiveRequest(
        std::move(metadata), std::move(debugPayload), std::move(reqCtx)));
    return;
  }

  if (worker_->isStopping()) {
    handleServerShutdown(makeActiveRequest(
        std::move(metadata), std::move(debugPayload), std::move(reqCtx)));
    return;
  }

  // check the checksum
  const bool badChecksum = metadata.crc32c_ref() &&
      (*metadata.crc32c_ref() != checksum::crc32c(*data));

  if (badChecksum) {
    handleRequestWithBadChecksum(makeActiveRequest(
        std::move(metadata), std::move(debugPayload), std::move(reqCtx)));
    return;
  }

  // A request should not be active until the overload checking is done.
  auto request = makeRequest(
      std::move(metadata), std::move(debugPayload), std::move(reqCtx));

  // check if server is overloaded
  const auto& headers = request->getTHeader().getHeaders();
  const auto& name = request->getMethodName();
  auto errorCode = serverConfigs_->checkOverload(&headers, &name);
  serverConfigs_->incActiveRequests();
  if (UNLIKELY(errorCode.has_value())) {
    handleRequestOverloadedServer(std::move(request), errorCode.value());
    return;
  }

  auto preprocessResult =
      serverConfigs_->preprocess({headers, name, connContext_, request.get()});
  if (UNLIKELY(preprocessResult.has_value())) {
    preprocessResult->apply_visitor(
        apache::thrift::detail::VisitorHelper()
            .with([&](AppClientException& ace) {
              handleAppError(
                  std::move(request), ace.name(), ace.getMessage(), true);
            })
            .with([&](const AppServerException& ase) {
              handleAppError(
                  std::move(request), ase.name(), ase.getMessage(), false);
            }));
    return;
  }

  logSetupConnectionEventsOnce(setupLoggingFlag_, connContext_);

  auto* cpp2ReqCtx = request->getRequestContext();
  auto& timestamps = cpp2ReqCtx->getTimestamps();
  timestamps.setStatus(samplingStatus);
  if (UNLIKELY(samplingStatus.isEnabled())) {
    timestamps.readEnd = readEnd;
    timestamps.processBegin = std::chrono::steady_clock::now();
  }

  // Log monitoring methods that are called over non-monitoring interface so
  // that they can be migrated.
  LoggingSampler monitoringLogSampler{
      THRIFT_FLAG(monitoring_over_user_logging_sample_rate)};
  if (UNLIKELY(monitoringLogSampler.isSampled())) {
    if (LIKELY(connContext_.getInterfaceKind() != InterfaceKind::MONITORING)) {
      auto& methodName = request->getMethodName();
      if (UNLIKELY(isMonitoringMethodName(methodName))) {
        THRIFT_CONNECTION_EVENT(monitoring_over_user)
            .logSampled(connContext_, monitoringLogSampler, [&] {
              return folly::dynamic::object("method_name", methodName);
            });
      }
    }
  }

  if (serverConfigs_) {
    if (auto* observer = serverConfigs_->getObserver()) {
      // Expensive operations; happens only when sampling is enabled
      if (samplingStatus.isEnabledByServer()) {
        observer->queuedRequests(threadManager_->pendingUpstreamTaskCount());
        observer->activeRequests(serverConfigs_->getActiveRequests());
      }
    }
  }
  const auto protocolId = request->getProtoId();
  if (auto interactionId = metadata.interactionId_ref()) {
    cpp2ReqCtx->setInteractionId(*interactionId);
  }
  if (auto interactionCreate = metadata.interactionCreate_ref()) {
    cpp2ReqCtx->setInteractionCreate(*interactionCreate);
    DCHECK_EQ(cpp2ReqCtx->getInteractionId(), 0);
    cpp2ReqCtx->setInteractionId(*interactionCreate->interactionId_ref());
  }
  try {
    cpp2Processor_->processSerializedCompressedRequest(
        std::move(request),
        SerializedCompressedRequest(
            std::move(data),
            metadata.compression_ref().value_or(CompressionAlgorithm::NONE)),
        protocolId,
        cpp2ReqCtx,
        eventBase_,
        threadManager_.get());
  } catch (...) {
    LOG(DFATAL) << "AsyncProcessor::process exception: "
                << folly::exceptionStr(std::current_exception());
  }
}

void ThriftRocketServerHandler::handleRequestWithBadMetadata(
    ThriftRequestCoreUniquePtr request) {
  if (auto* observer = serverConfigs_->getObserver()) {
    observer->taskKilled();
  }
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::UNSUPPORTED_CLIENT_TYPE,
          "Invalid metadata object"),
      kRequestParsingErrorCode);
}

void ThriftRocketServerHandler::handleRequestWithBadChecksum(
    ThriftRequestCoreUniquePtr request) {
  if (auto* observer = serverConfigs_->getObserver()) {
    observer->taskKilled();
  }
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::CHECKSUM_MISMATCH, "Checksum mismatch"),
      kChecksumMismatchErrorCode);
}

void ThriftRocketServerHandler::handleDecompressionFailure(
    ThriftRequestCoreUniquePtr request, std::string&& reason) {
  if (auto* observer = serverConfigs_->getObserver()) {
    observer->taskKilled();
  }
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::INVALID_TRANSFORM,
          fmt::format("decompression failure: {}", std::move(reason))),
      kRequestParsingErrorCode);
}

void ThriftRocketServerHandler::handleRequestOverloadedServer(
    ThriftRequestCoreUniquePtr request, const std::string& errorCode) {
  if (auto* observer = serverConfigs_->getObserver()) {
    observer->serverOverloaded();
  }
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::LOADSHEDDING, "loadshedding request"),
      errorCode);
}

void ThriftRocketServerHandler::handleAppError(
    ThriftRequestCoreUniquePtr request,
    const std::string& name,
    const std::string& message,
    bool isClientError) {
  static const std::string headerEx = "uex";
  static const std::string headerExWhat = "uexw";
  auto header = request->getRequestContext()->getHeader();
  header->setHeader(headerEx, name);
  header->setHeader(headerExWhat, message);
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::UNKNOWN, std::move(message)),
      isClientError ? kAppClientErrorCode : kAppServerErrorCode);
}

void ThriftRocketServerHandler::handleServerShutdown(
    ThriftRequestCoreUniquePtr request) {
  if (auto* observer = serverConfigs_->getObserver()) {
    observer->taskKilled();
  }
  request->sendErrorWrapped(
      folly::make_exception_wrapper<TApplicationException>(
          TApplicationException::LOADSHEDDING, "server shutting down"),
      kQueueOverloadedErrorCode);
}

void ThriftRocketServerHandler::requestComplete() {
  serverConfigs_->decActiveRequests();
}

void ThriftRocketServerHandler::terminateInteraction(int64_t id) {
  if (cpp2Processor_) {
    cpp2Processor_->terminateInteraction(id, connContext_, *eventBase_);
  }
}

void ThriftRocketServerHandler::onBeforeHandleFrame() {
  worker_->getServer()->touchRequestTimestamp();
}
} // namespace rocket
} // namespace thrift
} // namespace apache
