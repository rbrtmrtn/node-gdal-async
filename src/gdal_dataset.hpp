#ifndef __NODE_GDAL_DATASET_H__
#define __NODE_GDAL_DATASET_H__

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

using namespace Napi;
using namespace Napi;

// > GDAL 2.0 : a wrapper for GDALDataset
// < GDAL 2.0 : a wrapper for either a GDALDataset or OGRDataSource that behaves
// like a 2.0 Dataset

namespace node_gdal {

class Dataset : public Napi::ObjectWrap<Dataset> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(GDALDataset *ds, GDALDataset *parent = nullptr);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(flush);
  GDAL_ASYNCABLE_DECLARE(getMetadata);
  GDAL_ASYNCABLE_DECLARE(setMetadata);
  static Napi::Value getFileList(const Napi::CallbackInfo &info);
  static Napi::Value getGCPProjection(const Napi::CallbackInfo &info);
  static Napi::Value getGCPs(const Napi::CallbackInfo &info);
  static Napi::Value setGCPs(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(executeSQL);
  static Napi::Value testCapability(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(buildOverviews);
  static Napi::Value close(const Napi::CallbackInfo &info);

  Napi::Value bandsGetter(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_GETTER_DECLARE(rasterSizeGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(srsGetter);
  Napi::Value driverGetter(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_GETTER_DECLARE(geoTransformGetter);
  Napi::Value descriptionGetter(const Napi::CallbackInfo &info);
  Napi::Value layersGetter(const Napi::CallbackInfo &info);
  Napi::Value rootGetter(const Napi::CallbackInfo &info);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  void srsSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void geoTransformSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  Dataset(GDALDataset *ds);
  inline GDALDataset *get() {
    return this_dataset;
  }

  void dispose(bool manual);
  long uid;
  long parent_uid;

  inline bool isAlive() {
    return this_dataset && object_store.isAlive(uid);
  }

    private:
  ~Dataset();
  GDALDataset *this_dataset;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
