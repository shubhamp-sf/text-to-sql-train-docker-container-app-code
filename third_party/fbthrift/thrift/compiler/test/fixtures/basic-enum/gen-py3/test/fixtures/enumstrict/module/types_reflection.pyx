#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#


import folly.iobuf as _fbthrift_iobuf

from thrift.py3.reflection cimport (
    NumberType as __NumberType,
    StructType as __StructType,
    Qualifier as __Qualifier,
)


cimport test.fixtures.enumstrict.module.types as _test_fixtures_enumstrict_module_types

from thrift.py3.types cimport (
    constant_shared_ptr,
    default_inst,
)


cdef __StructSpec get_reflection__MyStruct():
    cdef _test_fixtures_enumstrict_module_types.MyStruct defaults = _test_fixtures_enumstrict_module_types.MyStruct.create(
        constant_shared_ptr[_test_fixtures_enumstrict_module_types.cMyStruct](
            default_inst[_test_fixtures_enumstrict_module_types.cMyStruct]()
        )
    )
    cdef __StructSpec spec = __StructSpec.create(
        name="MyStruct",
        kind=__StructType.STRUCT,
        annotations={
        },
    )
    spec.add_field(
        __FieldSpec.create(
            id=1,
            name="myEnum",
            type=_test_fixtures_enumstrict_module_types.MyEnum,
            kind=__NumberType.NOT_A_NUMBER,
            qualifier=__Qualifier.UNQUALIFIED,
            default=None,
            annotations={
            },
        ),
    )
    spec.add_field(
        __FieldSpec.create(
            id=2,
            name="myBigEnum",
            type=_test_fixtures_enumstrict_module_types.MyBigEnum,
            kind=__NumberType.NOT_A_NUMBER,
            qualifier=__Qualifier.UNQUALIFIED,
            default=defaults.myBigEnum,
            annotations={
            },
        ),
    )
    return spec
