#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#

cimport cython
from cpython.version cimport PY_VERSION_HEX
from libc.stdint cimport (
    int8_t as cint8_t,
    int16_t as cint16_t,
    int32_t as cint32_t,
    int64_t as cint64_t,
)
from libcpp.memory cimport shared_ptr, make_shared, unique_ptr, make_unique
from libcpp.string cimport string
from libcpp cimport bool as cbool
from cpython cimport bool as pbool
from libcpp.vector cimport vector
from libcpp.set cimport set as cset
from libcpp.map cimport map as cmap
from libcpp.utility cimport move as cmove
from cython.operator cimport dereference as deref
from cpython.ref cimport PyObject
from thrift.py3.exceptions cimport (
    cTApplicationException,
    ApplicationError as __ApplicationError,
    cTApplicationExceptionType__UNKNOWN)
from thrift.py3.server cimport ServiceInterface, RequestContext, Cpp2RequestContext
from thrift.py3.server import RequestContext, pass_context
from folly cimport (
  cFollyPromise,
  cFollyUnit,
  c_unit,
)
from thrift.py3.common cimport (
    cThriftServiceContext as __fbthrift_cThriftServiceContext,
    cThriftMetadata as __fbthrift_cThriftMetadata,
    ServiceMetadata,
    extractMetadataFromServiceContext,
    MetadataBox as __MetadataBox,
)

if PY_VERSION_HEX >= 0x030702F0:  # 3.7.2 Final
    from thrift.py3.server cimport THRIFT_REQUEST_CONTEXT as __THRIFT_REQUEST_CONTEXT

cimport folly.futures
from folly.executor cimport get_executor
cimport folly.iobuf as _fbthrift_iobuf
import folly.iobuf as _fbthrift_iobuf
from folly.iobuf cimport move as move_iobuf
from folly.memory cimport to_shared_ptr as __to_shared_ptr

cimport my.namespacing.extend.test.extend.types as _my_namespacing_extend_test_extend_types
import my.namespacing.extend.test.extend.types as _my_namespacing_extend_test_extend_types
cimport hsmodule.services as _hsmodule_services
import hsmodule.services as _hsmodule_services
import hsmodule.types as _hsmodule_types
cimport hsmodule.types as _hsmodule_types

cimport my.namespacing.extend.test.extend.services_reflection as _services_reflection

import asyncio
import functools
import sys
import traceback
import types as _py_types

from my.namespacing.extend.test.extend.services_wrapper cimport cExtendTestServiceInterface



@cython.auto_pickle(False)
cdef class Promise_cbool:
    cdef cFollyPromise[cbool] cPromise

    @staticmethod
    cdef create(cFollyPromise[cbool] cPromise):
        cdef Promise_cbool inst = Promise_cbool.__new__(Promise_cbool)
        inst.cPromise = cmove(cPromise)
        return inst

cdef object _ExtendTestService_annotations = _py_types.MappingProxyType({
})


@cython.auto_pickle(False)
cdef class ExtendTestServiceInterface(
    _hsmodule_services.HsTestServiceInterface
):
    annotations = _ExtendTestService_annotations

    def __cinit__(self):
        self._cpp_obj = cExtendTestServiceInterface(
            <PyObject *> self,
            get_executor()
        )

    @staticmethod
    def pass_context_check(fn):
        return pass_context(fn)

    async def check(
            self,
            struct1):
        raise NotImplementedError("async def check is not implemented")

    @classmethod
    def __get_reflection__(cls):
        return _services_reflection.get_reflection__ExtendTestService(for_clients=False)

    @staticmethod
    def __get_metadata__():
        cdef __fbthrift_cThriftMetadata meta
        cdef __fbthrift_cThriftServiceContext context
        ServiceMetadata[_services_reflection.cExtendTestServiceSvIf].gen(meta, context)
        extractMetadataFromServiceContext(meta, context)
        return __MetadataBox.box(cmove(meta))

    @staticmethod
    def __get_thrift_name__():
        return "extend.ExtendTestService"



cdef api void call_cy_ExtendTestService_check(
    object self,
    Cpp2RequestContext* ctx,
    cFollyPromise[cbool] cPromise,
    unique_ptr[_hsmodule_types.cHsFoo] struct1
):
    cdef Promise_cbool __promise = Promise_cbool.create(cmove(cPromise))
    arg_struct1 = _hsmodule_types.HsFoo.create(shared_ptr[_hsmodule_types.cHsFoo](struct1.release()))
    __context = RequestContext.create(ctx)
    if PY_VERSION_HEX >= 0x030702F0:  # 3.7.2 Final
        __context_token = __THRIFT_REQUEST_CONTEXT.set(__context)
        __context = None
    asyncio.get_event_loop().create_task(
        ExtendTestService_check_coro(
            self,
            __context,
            __promise,
            arg_struct1
        )
    )
    if PY_VERSION_HEX >= 0x030702F0:  # 3.7.2 Final
        __THRIFT_REQUEST_CONTEXT.reset(__context_token)

async def ExtendTestService_check_coro(
    object self,
    object ctx,
    Promise_cbool promise,
    struct1
):
    try:
        if ctx and getattr(self.check, "pass_context", False):
            result = await self.check(ctx,
                      struct1)
        else:
            result = await self.check(
                      struct1)
    except __ApplicationError as ex:
        # If the handler raised an ApplicationError convert it to a C++ one
        promise.cPromise.setException(cTApplicationException(
            ex.type.value, ex.message.encode('UTF-8')
        ))
    except Exception as ex:
        print(
            "Unexpected error in service handler check:",
            file=sys.stderr)
        traceback.print_exc()
        promise.cPromise.setException(cTApplicationException(
            cTApplicationExceptionType__UNKNOWN, repr(ex).encode('UTF-8')
        ))
    else:
        promise.cPromise.setValue(<cbool> result)
