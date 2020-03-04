//
// Created by Kevin Paul on 2019-04-24.
// Modified by Weile Wei on 2019-06-01.
// Modified by Haiying Xu on 2019-11-12.
//

#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include "z5wrapper.h"
#include <typeinfo>
#include <cxxabi.h>

namespace fs = boost::filesystem;
namespace z5 {
    extern "C" {

    std::unique_ptr<Dataset> ds;
    void z5CreateFile(char* path)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::File cFile(path_s);
        createFile(cFile, asZarr);
    }

    int z5OpenFile(char* path, struct dsnametype **head_ds)
    {
        std::string path_s(path);
        handle::Dataset handle_(path_s);
        DatasetMetadata metadata;
        nlohmann::json jOut;
        readMetadata(handle_,metadata);
        readAttributes(handle_,jOut);
                
        int dtype = metadata.dtype;
        int size = metadata.shape.size();
      
        dsnametype *new_ds = (dsnametype*)malloc(sizeof(dsnametype));
        new_ds->next = (*head_ds);  
        new_ds->name = (char*)malloc((path_s.length()+1));
        strcpy(new_ds->name,path);
        new_ds->shape = (size_t*)malloc(sizeof(size_t)*size);
        new_ds->chunk = (size_t*)malloc(sizeof(size_t)*size);
        (*head_ds) = new_ds; 
	{
	    new_ds->ndims = jOut["ndims"];
	    if (new_ds->ndims){
		new_ds->dimnameid = (dim_name_id*)malloc(sizeof(dim_name_id) * new_ds->ndims);
	    }
	}
        if (new_ds->ndims > 0)
        {
	    for (int j = 0; j < jOut["_ARRAY_DIMENSIONS"].size(); ++j) 
	    {
		new_ds->dimnameid[j].name = (char*)malloc(sizeof(char) * (((std::string)jOut["_ARRAY_DIMENSIONS"][j]).length()+1));
		strcpy(new_ds->dimnameid[j].name, ((std::string)jOut["_ARRAY_DIMENSIONS"][j]).c_str());
		//ds->dimnameid[i].id=it.value();
	    }
	    for (int j = 0; j < jOut["dimid"].size(); ++j) 
	    {
		new_ds->dimnameid[j].id = jOut["dimid"][j];
	    }
        }
        for (int i = 0; i < size; ++i){
             new_ds->shape[i] = metadata.shape[i]; 
             new_ds->chunk[i] = metadata.chunkShape[i]; 
        }
        //std::cout<<"ds->name "<<new_ds->name<<std::endl;
        new_ds->dtype = dtype;
        return Z5_NOERR;
    }
    void z5CreateGroup(char* path) {
        std::string path_s(path);
        bool asZarr = true;
        handle::Group cGroup(path_s);
        createGroup(cGroup, asZarr);
    }
    int containGroup(const char* path){
        std::string paths_s(path);
        paths_s.append("/");
        paths_s.append(GROUP_META_KEY);
        fs::path p(paths_s);
        if (fs::exists(p))
           return Z5_NOERR;
        else
           return Z5_ERR;;
        
    }
    int containDataset(const char* path){
        std::string paths_s(path);
        paths_s.append("/");
        paths_s.append(DATASET_META_KEY);
        fs::path p(paths_s);
        if (fs::exists(p))
           return Z5_NOERR;
        else
           return Z5_ERR;
    } 
    int z5OpenGroup(char* path, struct dsnametype **head_ds) {
        std::string path_s(path);
        bool asZarr = true;
        for(fs::directory_iterator itr(path_s);itr!=fs::directory_iterator(); ++itr) {
            const auto filenameStr = itr->path().filename().string();
            //std::cout<<itr->path().parent_path()<<std::endl;
            //std::cout<<itr->path()<<std::endl;
            if (containGroup(itr->path().c_str())){
                z5OpenGroup((char*)itr->path().c_str(),head_ds);
		//std::cout <<"group: "<<filenameStr<<"\n";
            }
            else if (containDataset(itr->path().c_str())){
                int dtype = z5OpenFile((char*)itr->path().c_str(),head_ds);
		//std::cout <<"dataset: "<<filenameStr<<" "<<dtype<<"\n";
            }
        }
        return 0;
    }
    void z5CreateFloat32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);
        std::vector<std::string> dtype({"float32"});
        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;
        int shuffle = 1;

        DatasetMetadata floatMeta(types::Datatype::float32,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            floatMeta.compressor = types::zlib;
            floatMeta.compressionOptions["useZlib"] = true;
            floatMeta.compressionOptions["level"] = level;
        }
        if (cuseZlib == 2){
          std::string codec("lz4");
          std::cout<<"use blosc"<<std::endl;
          floatMeta.compressor = types::blosc;
          floatMeta.compressionOptions["codec"] = codec;
          floatMeta.compressionOptions["level"] = level;
          floatMeta.compressionOptions["shuffle"] = shuffle;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, floatMeta);

    }

    void z5WriteFloat32Subarray(char *path, float *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<int32_t>;
        std::vector<size_t> start_v;
        using count_type = std::vector<vec_type::size_type>;
        count_type s;
        int tmp;
        tmp = ndim == 0 ? 1 : ndim;
        size_t size = 1;
        for (int i = 0; i < tmp; i++){
            if (ndim == 0){
                start_v.push_back(0);
		s.push_back(1);  //set count to 1 instead of default 96
            }
            else{
                start_v.push_back(start[i]);
                s.push_back(count[i]);
                size = size*count[i];
            }
        }
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        //std::cout<<"Float array "<<path_s<<std::endl;
        multiarray::writeSubarray<float>(ds,adp_array,start_v.begin());
        //std::cout<<"Float array "<<path_s<<" done"<<std::endl;
    }

    void z5ReadFloat32Subarray(char *path, float *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<float>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<float>(ds,adp_array,start_v.begin());
    }

    void z5CreateFloat64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);
        bool asZarr = true;
        std::vector<size_t> shape_v,count_v;
        int tmp;
        tmp = ndim == 0 ? 1 : ndim;
           
        for (int i = 0; i < tmp; i++){
            shape_v.push_back(shape[i]);
            count_v.push_back(count[i]);
        }
         
        DatasetMetadata floatMeta(types::Datatype::float64,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            floatMeta.compressor = types::zlib;
            floatMeta.compressionOptions["useZlib"] = true;
            floatMeta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, floatMeta);

    }

    void z5WriteFloat64Subarray(char *path, double *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<int32_t>;
        std::vector<size_t> start_v;
        using count_type = std::vector<vec_type::size_type>;
        count_type s;
        int tmp;
        tmp = ndim == 0 ? 1 : ndim;
        size_t size = 1;
        for (int i = 0; i < tmp; i++){
            if (ndim == 0){
                start_v.push_back(0);
		s.push_back(1);  //set count to 1 instead of default 96
            }
            else{
                start_v.push_back(start[i]);
                s.push_back(count[i]);
                size = size*count[i];
            }
        }
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<double>(ds,adp_array,start_v.begin());
    }

    void z5ReadFloat64Subarray(char *path, double *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<float>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<double>(ds,adp_array,start_v.begin());
    }

    void z5CreateInt8Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata int8Meta(types::Datatype::int8,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            int8Meta.compressor = types::zlib;
            int8Meta.compressionOptions["useZlib"] = true;
            int8Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, int8Meta);
    }

    void z5WriteInt8Subarray(char *path, int8_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<int8_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<int8_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadInt8Subarray(char *path, int8_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<int8_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<int8_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateInt16Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata int16Meta(types::Datatype::int16,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            int16Meta.compressor = types::zlib;
            int16Meta.compressionOptions["useZlib"] = true;
            int16Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, int16Meta);
    }

    void z5WriteInt16Subarray(char *path, int16_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<int16_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<int16_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadInt16Subarray(char *path, int16_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<int16_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<int16_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateInt32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);
        bool asZarr = true;
        std::vector<size_t> shape_v,count_v;
        int tmp;
        tmp = ndim == 0 ? 1 : ndim;
           
        for (int i = 0; i < tmp; i++){
            shape_v.push_back(shape[i]);
            count_v.push_back(count[i]);
        }
         
        DatasetMetadata int32Meta(types::Datatype::int32,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            int32Meta.compressor = types::zlib;
            int32Meta.compressionOptions["useZlib"] = true;
            int32Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, int32Meta);
    }

    void z5WriteInt32Subarray(char *path, int32_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<int32_t>;
        std::vector<size_t> start_v;
        using count_type = std::vector<vec_type::size_type>;
        count_type s;
        int tmp;
        tmp = ndim == 0 ? 1 : ndim;
        size_t size = 1;
        //std::cout<<"writeInt "<<path_s<<" "<<*array<<std::endl; 

        for (int i = 0; i < tmp; i++){
            if (ndim == 0){
                start_v.push_back(0);
		s.push_back(1);  //set count to 1 instead of default 96
            }
            else{
                start_v.push_back(start[i]);
                s.push_back(count[i]);
                size = size*count[i];
            }
        }
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<int32_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadInt32Subarray(char *path, int32_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<int32_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<int32_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata int64Meta(types::Datatype::int64,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            int64Meta.compressor = types::zlib;
            int64Meta.compressionOptions["useZlib"] = true;
            int64Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, int64Meta);
    }

    void z5WriteInt64Subarray(char *path, long int *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<long int>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<long int>(ds,adp_array,start_v.begin());
    }

    void z5ReadInt64Subarray(char *path, long int *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<long int>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<long int>(ds,adp_array,start_v.begin());
    }

    void z5CreateUInt8Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata uint8Meta(types::Datatype::uint8,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            uint8Meta.compressor = types::zlib;
            uint8Meta.compressionOptions["useZlib"] = true;
            uint8Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, uint8Meta);
    }

    void z5WriteUInt8Subarray(char *path, uint8_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<uint8_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<uint8_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadUInt8Subarray(char *path, uint8_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<uint8_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<uint8_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateUInt16Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata uint16Meta(types::Datatype::uint16,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            uint16Meta.compressor = types::zlib;
            uint16Meta.compressionOptions["useZlib"] = true;
            uint16Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, uint16Meta);
    }

    void z5WriteUInt16Subarray(char *path, uint16_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<uint16_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<uint16_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadUInt16Subarray(char *path, uint16_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<uint16_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<uint16_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateUInt32Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata uint32Meta(types::Datatype::uint32,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            uint32Meta.compressor = types::zlib;
            uint32Meta.compressionOptions["useZlib"] = true;
            uint32Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, uint32Meta);
    }

    void z5WriteUInt32Subarray(char *path, uint32_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<uint32_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<uint32_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadUInt32Subarray(char *path, uint32_t *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<uint32_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<uint32_t>(ds,adp_array,start_v.begin());
    }

    void z5CreateUInt64Dataset(char *path, unsigned int ndim, size_t *shape, size_t *count, int cuseZlib, int level) {
        std::string path_s(path);

        std::vector<size_t> shape_v(shape, shape + ndim);
        std::vector<size_t> count_v(count, count + ndim);
        bool asZarr = true;

        DatasetMetadata uint64Meta(types::Datatype::uint64,shape_v,count_v,asZarr);
        if (cuseZlib == 1) {
            uint64Meta.compressor = types::zlib;
            uint64Meta.compressionOptions["useZlib"] = true;
            uint64Meta.compressionOptions["level"] = level;
        }
        handle::Dataset handle_(path_s);
        handle_.createDir();
        writeMetadata(handle_, uint64Meta);
    }

    void z5WriteUInt64Subarray(char *path, unsigned long *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds =openDataset(path_s);
        using vec_type = std::vector<uint64_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size=size*(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::writeSubarray<uint64_t>(ds,adp_array,start_v.begin());
    }

    void z5ReadUInt64Subarray(char *path, unsigned long *array, unsigned int ndim, size_t *count, size_t *start) {
        std::string path_s(path);
        auto ds = openDataset(path_s);
        using vec_type = std::vector<uint64_t>;
        size_t size = 1;
        std::vector<std::size_t> count_v(count,count + ndim);
        for (std::vector<size_t>::const_iterator i = count_v.begin(); i != count_v.end(); ++i)
            size*=(*i);
        using count_type = std::vector<vec_type::size_type>;
        count_type s(count,count+ndim);
        std::vector<size_t> start_v(start,start + ndim);
        auto adp_array=xt::adapt(array,size,xt::no_ownership(),s);
        multiarray::readSubarray<uint64_t>(ds,adp_array,start_v.begin());
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

    void z5writeAttributesString(char *path, const char *name, const char *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        std::string value_s(value);
        j[name_s] = value_s;
        writeAttributes(cHandle, j);
    }
    void z5writeAttributesStringArr(char *path, const char *name, int ndims,char** value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        std::vector<std::string> str_vector;
        for (int i = 0; i < ndims; ++i){
	    std::string value_s(value[i]);
            str_vector.push_back(value_s);
        } 
        j[name_s] = str_vector;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesshort(char *path, const char *name, const short *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        short tmp = *value;
        j[name_s] = (int64_t) tmp;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesint(char *path, const char *name, const int *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (int64_t) *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesIntArr(char *path, const char *name, int ndims,const int *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        std::vector<int64_t> int_vector;
        for (int i = 0; i < ndims; ++i){
            int_vector.push_back((int64_t)value[i]);
        } 
        j[name_s] = int_vector;
        writeAttributes(cHandle, j);
    }
    void z5writeAttributeslong(char *path, const char *name, const long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (int64_t) *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributeslonglong(char *path, const char *name, const long long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (int64_t) *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesfloat(char *path, const char *name, const float *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (double) *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesdouble(char *path, const char *name, const double *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesushort(char *path, const char *name, const unsigned short *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (uint64_t) *value;
        writeAttributes(cHandle, j);
    }


    void z5writeAttributesuint(char *path, const char *name, const unsigned int *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (uint64_t) *value;
        writeAttributes(cHandle, j);
    }

    void z5writeAttributesulonglong(char *path, const char *name, const unsigned long long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::string name_s(name);
        j[name_s] = (uint64_t) *value;
        writeAttributes(cHandle, j);
    }
    void z5inqAttributes(char *path, const char *name, enum z5Datatype *att_type, long long int *lenp)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        const auto val_type = jOut[name_s].type();
        switch (val_type)
        {
           case nlohmann::json::value_t::string:
                if (att_type)
                   *att_type = string;
                if (lenp)
                   *lenp = strlen(jOut[name_s].get<std::string>().c_str());
                break;
           case nlohmann::json::value_t::boolean:
                break;
           case nlohmann::json::value_t::number_integer:
                if (att_type)
                   *att_type = int32; 
                if (lenp)
                   *lenp = jOut[name_s].size();
                break;
           case nlohmann::json::value_t::number_unsigned:
                if (att_type)
                   *att_type = uint32; 
                if (lenp)
                   *lenp = jOut[name_s].size();
                break;
           case nlohmann::json::value_t::number_float:
                if (att_type)
                   *att_type = float32;
                if (lenp)
                   *lenp = jOut[name_s].size();
                break;
           default:
                break;
                //std::cout<<" "<<std::endl;
        } 
    } 

    void z5readAttributesString(char *path, const char *name, char *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        strcpy(value ,  jOut[name_s].get<std::string>().c_str());
    }

    void z5readAttributesshort(char *path, const char *name, short *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = (unsigned short)stoi(jOut[name_s].get<std::string>());
    }

    void z5readAttributesint(char *path, const char *name, int *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value =  jOut[name_s].get<int>();
    }

    void z5readAttributeslong(char *path, const char *name, long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = (long)stoll(jOut[name_s].get<std::string>());
    }

    void z5readAttributeslonglong(char *path, const char *name, long long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = stoll(jOut[name_s].get<std::string>());
    }

    void z5readAttributesfloat(char *path, const char *name, float *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = stof( jOut[name_s].get<std::string>());
    }

    void z5readAttributesdouble(char *path, const char *name, double *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = stod( jOut[name_s].get<std::string>());
    }

    void z5readAttributesushort(char *path, const char *name, unsigned short *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = (unsigned short)stoul(jOut[name_s].get<std::string>());
    }


    void z5readAttributesuint(char *path, const char *name, unsigned int *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = (unsigned int)stoul( jOut[name_s].get<std::string>());
    }

    void z5readAttributesulonglong(char *path, const char *name, unsigned long long *value)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json jOut;
        std::string name_s(name);
        std::vector<std::string> keys({name_s});
        readAttributes(cHandle,keys, jOut);
        *value = stoull(jOut[name_s].get<std::string>());
    }

    void z5Delete(char *path ){
        std::string path_s(path);
        fs::path filename(path_s);
        fs::remove_all(filename);
    }

    // read attributes //
    void z5readAttributesWithKeys(char *path, char *keys[], int keys_sz)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        std::vector<std::string> keys_s;
        for (size_t i = 0; i < keys_sz; i++)
        {
            std::string keys_tmp(keys[i]);
            keys_s.push_back(keys_tmp);
        }
        readAttributes(cHandle, keys_s, j);
#ifdef _JASON_OUTPUT_
        for (auto it = j.begin(); it != j.end(); ++it)
	{
    		std::cout << "key: " << it.key() << ", value:" << it.value() << '\n';
	}
#endif
    }

    void z5readAttributes(char *path)
    {
        std::string path_s(path);
        bool asZarr = true;
        handle::Handle cHandle(path_s);
        nlohmann::json j;
        readAttributes(cHandle, j);
#ifdef _JASON_OUTPUT_
        for (auto it = j.begin(); it != j.end(); ++it)
	{
    		std::cout << "key: " << it.key() << ", value:" << it.value() << '\n';
	}
#endif
    }
    }

}
