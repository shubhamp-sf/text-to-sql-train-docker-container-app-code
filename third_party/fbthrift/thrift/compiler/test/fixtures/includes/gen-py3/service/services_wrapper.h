/**
 * Autogenerated by Thrift
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */

#pragma once
#include <src/gen-cpp2/MyService.h>
#include <folly/python/futures.h>
#include <Python.h>

#include <memory>

namespace cpp2 {

class MyServiceWrapper : virtual public MyServiceSvIf {
  protected:
    PyObject *if_object;
    folly::Executor *executor;
  public:
    explicit MyServiceWrapper(PyObject *if_object, folly::Executor *exc);
    void async_tm_query(std::unique_ptr<apache::thrift::HandlerCallback<void>> callback
        , std::unique_ptr<::cpp2::MyStruct> s
        , std::unique_ptr<::cpp2::Included> i
    ) override;
    void async_tm_has_arg_docs(std::unique_ptr<apache::thrift::HandlerCallback<void>> callback
        , std::unique_ptr<::cpp2::MyStruct> s
        , std::unique_ptr<::cpp2::Included> i
    ) override;
};

std::shared_ptr<apache::thrift::ServerInterface> MyServiceInterface(PyObject *if_object, folly::Executor *exc);
} // namespace cpp2