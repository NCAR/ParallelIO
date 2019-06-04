//
// Created by Kevin Paul on 2019-04-24.
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
#include "z5/compression/zlib_compressor.hxx"
#include "z5/types/types.hxx"
#endif

#ifdef __cplusplus
namespace z5 {
    extern "C" {
#endif
    void z5CreateFile(char* path);

    void z5CreateGroup(char* path);

    void z5CreateFloatDataset(char *path, unsigned int ndim, size_t *shape, size_t *chunks, int cuseZlib, int level);

    void z5WriteFloatSubarray(char *path, float *array, unsigned int ndim, size_t *shape, size_t *offset);

    void z5ReadFloatSubarray(char *path, float *array, unsigned int ndim, size_t *shape, size_t *offset);

    void z5CreateInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *chunks, int cuseZlib, int level);

    void z5WriteInt64Subarray(char *path, long long int *array, unsigned int ndim, size_t *shape, size_t *offset);

    void z5ReadInt64Subarray(char *path, long long int *array, unsigned int ndim, size_t *shape, size_t *offset);

    size_t z5GetFileSize(char *path);

    void z5Delete(char *path );
#ifdef __cplusplus
    }
}
#endif
#endif //CZ5TEST_Z5WRAPPER_H