#ifndef __NODE_GDAL_MDARRAY_H__
#define __NODE_GDAL_MDARRAY_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

// ogr
#include <ogrsf_frmts.h>

#include "async.hpp"

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class MDArray : public Napi::ObjectWrap<MDArray> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(std::shared_ptr<GDALMDArray> group, GDALDataset *parent_ds);
  GDAL_ASYNCABLE_DECLARE(read);
  static Napi::Value getView(const Napi::CallbackInfo &info);
  static Napi::Value getMask(const Napi::CallbackInfo &info);
  static Napi::Value asDataset(const Napi::CallbackInfo &info);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  Napi::Value typeGetter(const Napi::CallbackInfo &info);
  Napi::Value lengthGetter(const Napi::CallbackInfo &info);
  Napi::Value srsGetter(const Napi::CallbackInfo &info);
  Napi::Value unitTypeGetter(const Napi::CallbackInfo &info);
  Napi::Value scaleGetter(const Napi::CallbackInfo &info);
  Napi::Value offsetGetter(const Napi::CallbackInfo &info);
  Napi::Value noDataValueGetter(const Napi::CallbackInfo &info);
  Napi::Value dimensionsGetter(const Napi::CallbackInfo &info);
  Napi::Value descriptionGetter(const Napi::CallbackInfo &info);
  Napi::Value attributesGetter(const Napi::CallbackInfo &info);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  MDArray(std::shared_ptr<GDALMDArray> ds);
  inline std::shared_ptr<GDALMDArray> get() {
    return this_;
  }

  void dispose();
  long uid;
  long parent_uid;
  size_t dimensions;

  inline bool isAlive() {
    return this_ && object_store.isAlive(uid);
  }

    private:
  ~MDArray();
  std::shared_ptr<GDALMDArray> this_;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
#endif
