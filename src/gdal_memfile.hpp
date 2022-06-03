#ifndef __NODE_GDAL_MEMFILE_H__
#define __NODE_GDAL_MEMFILE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "gdal_common.hpp"

using namespace Napi;
using namespace Napi;

// A vsimem file

namespace node_gdal {

class Memfile {
  void *data;
  Napi::ObjectReference *persistent;
  static void weakCallback(const Napi::WeakCallbackInfo<Memfile> &);

    public:
  std::string filename;
  Memfile(void *);
  Memfile(void *, const std::string &filename);
  ~Memfile();
  static Memfile *get(Napi::Object);
  static Memfile *get(Napi::Object, const std::string &filename);
  static bool copy(Napi::Object, const std::string &filename);
  static std::map<void *, Memfile *> memfile_collection;

  static void Initialize(Napi::Object target);
  static Napi::Value vsimemSet(const Napi::CallbackInfo &info);
  static Napi::Value vsimemAnonymous(const Napi::CallbackInfo &info);
  static Napi::Value vsimemRelease(const Napi::CallbackInfo &info);
  static Napi::Value vsimemCopy(const Napi::CallbackInfo &info);
};
} // namespace node_gdal
#endif
