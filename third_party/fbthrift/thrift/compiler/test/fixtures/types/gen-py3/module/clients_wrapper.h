/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */

#pragma once
#include <src/gen-cpp2/SomeService.h>

#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include <folly/Unit.h>
#include <thrift/lib/py3/clientcallbacks.h>
#include <thrift/lib/py3/client_wrapper.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace apache {
namespace thrift {
namespace fixtures {
namespace types {

class SomeServiceClientWrapper : public ::thrift::py3::ClientWrapper {
  public:
    using ::thrift::py3::ClientWrapper::ClientWrapper;

    folly::Future<std::unordered_map<int32_t,std::string>> bounce_map(
      apache::thrift::RpcOptions& rpcOptions,
      std::unordered_map<int32_t,std::string> arg_m);
    folly::Future<std::map<std::string,int64_t>> binary_keyed_map(
      apache::thrift::RpcOptions& rpcOptions,
      std::vector<int64_t> arg_r);
};


} // namespace apache
} // namespace thrift
} // namespace fixtures
} // namespace types
