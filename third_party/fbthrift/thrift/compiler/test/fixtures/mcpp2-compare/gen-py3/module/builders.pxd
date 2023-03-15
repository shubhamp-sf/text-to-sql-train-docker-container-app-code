#
# Autogenerated by Thrift
#
# DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
#  @generated
#
from cpython cimport bool as pbool, int as pint, float as pfloat

cimport folly.iobuf as _fbthrift_iobuf

cimport thrift.py3.builder

cimport includes.types as _includes_types
cimport includes.builders as _includes_builders

cimport module.types as _module_types

cdef class Empty_Builder(thrift.py3.builder.StructBuilder):
    pass


cdef class ASimpleStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint boolField


cdef class ASimpleStructNoexcept_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint boolField


cdef class MyStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pbool MyBoolField
    cdef public pint MyIntField
    cdef public str MyStringField
    cdef public str MyStringField2
    cdef public bytes MyBinaryField
    cdef public bytes MyBinaryField2
    cdef public bytes MyBinaryField3
    cdef public list MyBinaryListField4
    cdef public dict MyMapEnumAndInt


cdef class SimpleUnion_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint intValue
    cdef public str stringValue


cdef class ComplexUnion_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint intValue
    cdef public pint opt_intValue
    cdef public str stringValue
    cdef public str opt_stringValue
    cdef public pint intValue2
    cdef public pint intValue3
    cdef public pfloat doubelValue
    cdef public pbool boolValue
    cdef public list union_list
    cdef public set union_set
    cdef public dict union_map
    cdef public dict opt_union_map
    cdef public _module_types.MyEnumA enum_field
    cdef public list enum_container
    cdef public object a_struct
    cdef public set a_set_struct
    cdef public object a_union
    cdef public object opt_a_union
    cdef public list a_union_list
    cdef public set a_union_typedef
    cdef public list a_union_typedef_list
    cdef public bytes MyBinaryField
    cdef public bytes MyBinaryField2
    cdef public list MyBinaryListField4
    cdef public object ref_field
    cdef public object ref_field2
    cdef public object excp_field


cdef class AnException_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint code
    cdef public pint req_code
    cdef public str message2
    cdef public str req_message
    cdef public list exception_list
    cdef public set exception_set
    cdef public dict exception_map
    cdef public dict req_exception_map
    cdef public _module_types.MyEnumA enum_field
    cdef public list enum_container
    cdef public object a_struct
    cdef public set a_set_struct
    cdef public list a_union_list
    cdef public set union_typedef
    cdef public list a_union_typedef_list


cdef class AnotherException_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint code
    cdef public pint req_code
    cdef public str message


cdef class containerStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pbool fieldA
    cdef public pbool req_fieldA
    cdef public pbool opt_fieldA
    cdef public dict fieldB
    cdef public dict req_fieldB
    cdef public dict opt_fieldB
    cdef public set fieldC
    cdef public set req_fieldC
    cdef public set opt_fieldC
    cdef public str fieldD
    cdef public str fieldE
    cdef public str req_fieldE
    cdef public str opt_fieldE
    cdef public list fieldF
    cdef public dict fieldG
    cdef public list fieldH
    cdef public pbool fieldI
    cdef public dict fieldJ
    cdef public list fieldK
    cdef public set fieldL
    cdef public dict fieldM
    cdef public pint fieldN
    cdef public list fieldO
    cdef public list fieldP
    cdef public _module_types.MyEnumA fieldQ
    cdef public _module_types.MyEnumA fieldR
    cdef public _module_types.MyEnumA req_fieldR
    cdef public _module_types.MyEnumA opt_fieldR
    cdef public _module_types.MyEnumA fieldS
    cdef public list fieldT
    cdef public list fieldU
    cdef public object fieldV
    cdef public object req_fieldV
    cdef public object opt_fieldV
    cdef public set fieldW
    cdef public object fieldX
    cdef public object req_fieldX
    cdef public object opt_fieldX
    cdef public list fieldY
    cdef public set fieldZ
    cdef public list fieldAA
    cdef public dict fieldAB
    cdef public _module_types.MyEnumB fieldAC
    cdef public _includes_types.AnEnum fieldAD
    cdef public dict fieldAE
    cdef public str fieldSD


cdef class MyIncludedStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint MyIncludedInt
    cdef public object MyIncludedStruct
    cdef public object ARefField
    cdef public object ARequiredField


cdef class AnnotatedStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public object no_annotation
    cdef public object cpp_unique_ref
    cdef public object cpp2_unique_ref
    cdef public dict container_with_ref
    cdef public object req_cpp_unique_ref
    cdef public object req_cpp2_unique_ref
    cdef public list req_container_with_ref
    cdef public object opt_cpp_unique_ref
    cdef public object opt_cpp2_unique_ref
    cdef public set opt_container_with_ref
    cdef public object ref_type_unique
    cdef public object ref_type_shared
    cdef public dict ref_type_const
    cdef public object req_ref_type_shared
    cdef public object req_ref_type_const
    cdef public list req_ref_type_unique
    cdef public object opt_ref_type_const
    cdef public object opt_ref_type_unique
    cdef public set opt_ref_type_shared
    cdef public pint base_type
    cdef public list list_type
    cdef public set set_type
    cdef public dict map_type
    cdef public dict map_struct_type
    cdef public _fbthrift_iobuf.IOBuf iobuf_type
    cdef public _fbthrift_iobuf.IOBuf iobuf_ptr
    cdef public list list_i32_template
    cdef public list list_string_template
    cdef public set set_template
    cdef public dict map_template
    cdef public list typedef_list_template
    cdef public list typedef_deque_template
    cdef public set typedef_set_template
    cdef public dict typedef_map_template
    cdef public pint indirection_a
    cdef public list indirection_b
    cdef public set indirection_c
    cdef public _fbthrift_iobuf.IOBuf iobuf_type_val
    cdef public _fbthrift_iobuf.IOBuf iobuf_ptr_val
    cdef public object struct_struct


cdef class ComplexContainerStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public dict map_of_iobufs
    cdef public dict map_of_iobuf_ptrs


cdef class FloatStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pfloat floatField
    cdef public pfloat doubleField


cdef class FloatUnion_Builder(thrift.py3.builder.StructBuilder):
    cdef public pfloat floatSide
    cdef public pfloat doubleSide


cdef class AllRequiredNoExceptMoveCtrStruct_Builder(thrift.py3.builder.StructBuilder):
    cdef public pint intField


