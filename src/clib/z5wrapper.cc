//
// Created by Kevin Paul on 2019-04-24.
//
#include <string>
#include <vector>
#include <iostream>
#include "z5wrapper.h"


namespace fs = boost::filesystem;
namespace z5 {
    extern "C" {

    void z5CreateFile(char* path)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::File cFile(path_s);
        createFile(cFile, asZarr);
    }

    void z5CreateGroup(char* path) {
        std::string path_s(path);
        bool asZarr = true;
        handle::Group cGroup(path_s);
        createGroup(cGroup, asZarr);
    }
    void z5CreateFloatDataset(char *path, unsigned int ndim, size_t *shape, size_t *chunks, int cuseZlib, int level) {
        std::string path_s(path);
        std::vector<std::string> dtype({"float32"});
        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> chunks_v(chunks, chunks + ndim);
        bool asZarr = true;

        DatasetMetadata floatMeta(types::Datatype::float32,shape_v,chunks_v,asZarr);
        if (cuseZlib) {
            floatMeta.compressor = types::zlib;
            floatMeta.compressionOptions["useZlib"] = true;
            floatMeta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, floatMeta);

    }

    void z5CreateInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *chunks, int cuseZlib, int level) {
        std::string path_s(path);
        std::vector<std::string> dtype({"long long int"});

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> chunks_v(chunks, chunks + ndim);
        bool asZarr = true;

        DatasetMetadata int64Meta(types::Datatype::int64,shape_v,chunks_v,asZarr);
        if (cuseZlib) {
            int64Meta.compressor = types::zlib;
            int64Meta.compressionOptions["useZlib"] = true;
            int64Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, int64Meta);
        long long int idata1;
        int64_t data1;
        long int data2;
        //std::cout<<"int64 "<<typeid(data1).name()<<" "<<sizeof(int64_t)<<std::endl;
        //std::cout<<"long int "<<typeid(data2).name()<<" "<<sizeof(long int)<<std::endl;
        //std::cout<<"long long int "<<typeid(idata1).name()<<" "<<sizeof(long long int)<<std::endl;
    }

    void z5WriteFloatSubarray(char *path, float *array, unsigned int ndim, size_t *shape, size_t *offset) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        size_t size = 1;
        std::vector<std::size_t> shape_v(shape,shape + ndim);
        for (std::vector<size_t>::const_iterator i = shape_v.begin(); i != shape_v.end(); ++i)
            size=size*(*i);
        xt::xarray<float> adp_array=xt::adapt(array,size,xt::no_ownership(),shape_v);
        std::vector<size_t> offset_v(offset,offset + ndim);
        multiarray::writeSubarray<float>(ds,adp_array,offset_v.begin());
    }

    void z5WriteInt64Subarray(char *path, long long int *array, unsigned int ndim, size_t *shape, size_t *offset) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<long int>;
        size_t size = 1;
        std::vector<std::size_t> shape_v(shape,shape + ndim);
        for (std::vector<size_t>::const_iterator i = shape_v.begin(); i != shape_v.end(); ++i)
            size=size*(*i);
        using shape_type = std::vector<vec_type::size_type>;
        shape_type s(shape,shape+ndim);
        std::vector<size_t> offset_v(offset,offset + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<long int>(ds,adp_array,offset_v.begin());
    }

    void z5ReadFloatSubarray(char *path, float *array, unsigned int ndim, size_t *shape, size_t *offset) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<float>;
        size_t size = 1;
        std::vector<std::size_t> shape_v(shape,shape + ndim);
        for (std::vector<size_t>::const_iterator i = shape_v.begin(); i != shape_v.end(); ++i)
            size*=(*i);
        using shape_type = std::vector<vec_type::size_type>;
        shape_type s(shape,shape+ndim);
        std::vector<size_t> offset_v(offset,offset + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<float>(ds,adp_array,offset_v.begin());
    }
    void z5ReadInt64Subarray(char *path, long long int *array, unsigned int ndim, size_t *shape, size_t *offset) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<long int>;
        size_t size = 1;
        std::vector<std::size_t> shape_v(shape,shape + ndim);
        for (std::vector<size_t>::const_iterator i = shape_v.begin(); i != shape_v.end(); ++i)
            size*=(*i);
        using shape_type = std::vector<vec_type::size_type>;
        shape_type s(shape,shape+ndim);
        std::vector<size_t> offset_v(offset,offset + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<long int>(ds,adp_array,offset_v.begin());
    }
    size_t z5GetFileSize(char *path){
        std::string path_s(path);
        fs::ifstream file(path_s, std::ios::binary);
        file.seekg(0, std::ios::end);
        const std::size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        file.close();
        return fileSize;
    }
    void z5Delete(char *path ){
        std::string path_s(path);
        fs::path filename(path_s);
        fs::remove_all(filename);
    }

    }

}