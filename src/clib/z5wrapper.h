//
// Created by Kevin Paul on 2019-04-24.
// Modified by Weile We on 2019-06-01.
// Modified by Haiying Xu on 2019-11-12.
//
#ifndef CZ5TEST_Z5WRAPPER_H
#define CZ5TEST_Z5WRAPPER_H

#include <stddef.h>
#ifdef __cplusplus
#include "z5/multiarray/xtensor_access.hxx"
#endif

#ifdef __cplusplus
#include "z5/dataset_factory.hxx"
#include "z5/file.hxx"
#include "z5/groups.hxx"
#include "z5/attributes.hxx"
#include "z5/compression/zlib_compressor.hxx"
#include "z5/types/types.hxx"
#endif

#include <stdint.h>
#ifdef __cplusplus
namespace z5 {
    extern "C" {
#endif
    #define GROUP_META_KEY ".zgroup"
    #define DATASET_META_KEY ".zarray"
    #define Z5_NOERR 1
    #define Z5_ERR 0

    //declare enum z5Datatype
    enum z5Datatype {
       int8, int16, int32, int64,
       uint8, uint16, uint32, uint64,
       float32, float64, string
    };

    //struct to map dim name with dim id
    struct dim_name_id{
       char* name;
       int id;
    };
  
    //struct to save dim info 
    struct dsnametype{
       char* name;
       int dtype;
       int xtypep;
       int ndims;
       int natts;
       size_t* shape;
       size_t* chunk;
       struct dim_name_id* dimnameid;
       struct dsnametype *next;
    };
    void z5CreateFile(char* path);
    int z5OpenFile(char* path,struct dsnametype **head_ds);

    void z5CreateGroup(char* path);
    int containGroup(const char* path);
    int containDataset(const char* path);
    int z5OpenGroup(char* path,struct dsnametype **head_ds);

    // float 32
    void z5CreateFloat32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteFloat32Subarray(char *path, float *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadFloat32Subarray(char *path, float *array, unsigned int ndim, size_t *count, size_t *start);

    // float 64 / double
    void z5CreateFloat64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteFloat64Subarray(char *path, double *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadFloat64Subarray(char *path, double *array, unsigned int ndim, size_t *count, size_t *start);

    // int8_t
    void z5CreateInt8Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteInt8Subarray(char *path, int8_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadInt8Subarray(char *path, int8_t *array, unsigned int ndim, size_t *count, size_t *start);

    // int16_t
    void z5CreateInt16Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteInt16Subarray(char *path, int16_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadInt16Subarray(char *path, int16_t *array, unsigned int ndim, size_t *count, size_t *start);

    // int32_t
    void z5CreateInt32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteInt32Subarray(char *path, int32_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadInt32Subarray(char *path, int32_t *array, unsigned int ndim, size_t *count, size_t *start);

    // int64
    void z5CreateInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteInt64Subarray(char *path, long int *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadInt64Subarray(char *path, long int *array, unsigned int ndim, size_t *count, size_t *start);

    // uint8_t
    void z5CreateUInt8Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteUInt8Subarray(char *path, uint8_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadUInt8Subarray(char *path, uint8_t *array, unsigned int ndim, size_t *count, size_t *start);

    // uint16_t
    void z5CreateUInt16Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteUInt16Subarray(char *path, uint16_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadUInt16Subarray(char *path, uint16_t *array, unsigned int ndim, size_t *count, size_t *start);

    // uint32_t
    void z5CreateUInt32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteUInt32Subarray(char *path, uint32_t *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadUInt32Subarray(char *path, uint32_t *array, unsigned int ndim, size_t *count, size_t *start);

    // uint64
    void z5CreateUInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level);
    void z5WriteUInt64Subarray(char *path, unsigned long *array, unsigned int ndim, size_t *count, size_t *start);
    void z5ReadUInt64Subarray(char *path, unsigned long *array, unsigned int ndim, size_t *count, size_t *start);

    size_t z5GetFileSize(char *path);

    void z5Delete(char *path );
    void z5inqAttributes(char *path, const char *name, enum z5Datatype *att_type, long long int *lenp);
    void z5readAttributesString(char *path, const char *name, char *value);

    void z5readAttributesshort(char *path, const char *name, short *value);

    void z5readAttributesint(char *path, const char *name, int *value);

    void z5readAttributeslong(char *path, const char *name, long *value);

    void z5readAttributeslonglong(char *path, const char *name, long long *value);

    void z5readAttributesfloat(char *path, const char *name, float *value);

    void z5readAttributesdouble(char *path, const char *name, double *value);

    void z5readAttributesushort(char *path, const char *name, unsigned short *value);

    void z5readAttributesusint(char *path, const char *name, unsigned short int *value);

    void z5readAttributesuint(char *path, const char *name, unsigned int *value);

    void z5readAttributesulonglong(char *path, const char *name, unsigned long long *value);


    void z5writeAttributesString(char *path, const char *name, const char *value);
    void z5writeAttributesStringArr(char *path, const char *name, int ndims, char **value);

    void z5writeAttributesshort(char *path, const char *name, const short *value);

    void z5writeAttributesint(char *path, const char *name, const int *value);
    void z5writeAttributesIntArr(char *path, const char *name, int ndims, const int *value);

    void z5writeAttributeslong(char *path, const char *name, const long *value);

    void z5writeAttributeslonglong(char *path, const char *name, const long long *value);

    void z5writeAttributesfloat(char *path, const char *name, const float *value);

    void z5writeAttributesdouble(char *path, const char *name, const double *value);

    void z5writeAttributesushort(char *path, const char *name, const unsigned short *value);

    void z5writeAttributesuint(char *path, const char *name, const unsigned int *value);

    void z5writeAttributesulonglong(char *path, const char *name, const unsigned long long *value);


    // read attributes //

    void z5readAttributesWithKeys(char *path, char *keys[], int keys_sz);

    void z5readAttributes(char *path);
#ifdef __cplusplus
}
}
#endif
#endif //CZ5TEST_Z5WRAPPER_H
