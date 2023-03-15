#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#

from libc.stdint cimport (
    int8_t as cint8_t,
    int16_t as cint16_t,
    int32_t as cint32_t,
    int64_t as cint64_t,
    uint32_t as cuint32_t,
)
from libcpp.string cimport string
from libcpp cimport bool as cbool, nullptr, nullptr_t
from cpython cimport bool as pbool
from libcpp.memory cimport shared_ptr, unique_ptr
from libcpp.utility cimport move as cmove
from libcpp.vector cimport vector
from libcpp.set cimport set as cset
from libcpp.map cimport map as cmap, pair as cpair
from thrift.py3.exceptions cimport cTException
cimport folly.iobuf as _fbthrift_iobuf
cimport thrift.py3.exceptions
cimport thrift.py3.types
from thrift.py3.types cimport (
    bstring,
    bytes_to_string,
    field_ref as __field_ref,
    optional_field_ref as __optional_field_ref,
    required_field_ref as __required_field_ref,
)
from thrift.py3.common cimport (
    RpcOptions as __RpcOptions,
    Protocol as __Protocol,
    cThriftMetadata as __fbthrift_cThriftMetadata,
    MetadataBox as __MetadataBox,
)
from folly.optional cimport cOptional as __cOptional

cimport module.types_fields as _fbthrift_types_fields

cdef extern from "src/gen-py3/module/types.h":
  pass





cdef extern from "src/gen-cpp2/module_metadata.h" namespace "apache::thrift::detail::md":
    cdef cppclass ExceptionMetadata[T]:
        @staticmethod
        void gen(__fbthrift_cThriftMetadata &metadata)
cdef extern from "src/gen-cpp2/module_metadata.h" namespace "apache::thrift::detail::md":
    cdef cppclass StructMetadata[T]:
        @staticmethod
        void gen(__fbthrift_cThriftMetadata &metadata)
cdef extern from "src/gen-cpp2/module_types_custom_protocol.h" namespace "::cpp2":

    cdef cppclass cFoo "::cpp2::Foo":
        cFoo() except +
        cFoo(const cFoo&) except +
        bint operator==(cFoo&)
        bint operator!=(cFoo&)
        bint operator<(cFoo&)
        bint operator>(cFoo&)
        bint operator<=(cFoo&)
        bint operator>=(cFoo&)
        __field_ref[cint32_t] intField_ref()
        __optional_field_ref[cint32_t] optionalIntField_ref()
        __field_ref[cint32_t] intFieldWithDefault_ref()
        __field_ref[cset[string]] setField_ref()
        __optional_field_ref[cset[string]] optionalSetField_ref()
        __field_ref[cmap[string,vector[string]]] mapField_ref()
        __optional_field_ref[cmap[string,vector[string]]] optionalMapField_ref()
        cint32_t intField
        cint32_t optionalIntField
        cint32_t intFieldWithDefault
        cset[string] setField
        cset[string] optionalSetField
        cmap[string,vector[string]] mapField
        cmap[string,vector[string]] optionalMapField


    cdef cppclass cBar "::cpp2::Bar":
        cBar() except +
        cBar(const cBar&) except +
        bint operator==(cBar&)
        bint operator!=(cBar&)
        bint operator<(cBar&)
        bint operator>(cBar&)
        bint operator<=(cBar&)
        bint operator>=(cBar&)
        __field_ref[cFoo] structField_ref()
        __optional_field_ref[cFoo] optionalStructField_ref()
        __field_ref[vector[cFoo]] structListField_ref()
        __optional_field_ref[vector[cFoo]] optionalStructListField_ref()
        cFoo structField
        cFoo optionalStructField
        vector[cFoo] structListField
        vector[cFoo] optionalStructListField




cdef class Foo(thrift.py3.types.Struct):
    cdef shared_ptr[cFoo] _cpp_obj
    cdef _fbthrift_types_fields.__Foo_FieldsSetter _fields_setter
    cdef Set__string __fbthrift_cached_setField
    cdef Set__string __fbthrift_cached_optionalSetField
    cdef Map__string_List__string __fbthrift_cached_mapField
    cdef Map__string_List__string __fbthrift_cached_optionalMapField

    @staticmethod
    cdef create(shared_ptr[cFoo])



cdef class Bar(thrift.py3.types.Struct):
    cdef shared_ptr[cBar] _cpp_obj
    cdef _fbthrift_types_fields.__Bar_FieldsSetter _fields_setter
    cdef Foo __fbthrift_cached_structField
    cdef Foo __fbthrift_cached_optionalStructField
    cdef List__Foo __fbthrift_cached_structListField
    cdef List__Foo __fbthrift_cached_optionalStructListField

    @staticmethod
    cdef create(shared_ptr[cBar])


cdef class Set__string(thrift.py3.types.Set):
    cdef shared_ptr[cset[string]] _cpp_obj
    @staticmethod
    cdef create(shared_ptr[cset[string]])
    @staticmethod
    cdef shared_ptr[cset[string]] _make_instance(object items) except *

cdef class List__string(thrift.py3.types.List):
    cdef shared_ptr[vector[string]] _cpp_obj
    @staticmethod
    cdef create(shared_ptr[vector[string]])
    @staticmethod
    cdef shared_ptr[vector[string]] _make_instance(object items) except *

cdef class Map__string_List__string(thrift.py3.types.Map):
    cdef shared_ptr[cmap[string,vector[string]]] _cpp_obj
    @staticmethod
    cdef create(shared_ptr[cmap[string,vector[string]]])
    @staticmethod
    cdef shared_ptr[cmap[string,vector[string]]] _make_instance(object items) except *

cdef class List__Foo(thrift.py3.types.List):
    cdef shared_ptr[vector[cFoo]] _cpp_obj
    @staticmethod
    cdef create(shared_ptr[vector[cFoo]])
    @staticmethod
    cdef shared_ptr[vector[cFoo]] _make_instance(object items) except *


