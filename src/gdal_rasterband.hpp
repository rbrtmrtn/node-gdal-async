#ifndef __NODE_GDAL_RASTERBAND_H__
#define __NODE_GDAL_RASTERBAND_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "gdal_dataset.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class RasterBand : public Napi::ObjectWrap<RasterBand> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);
  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(GDALRasterBand *band, GDALDataset *parent);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(flush);
  GDAL_ASYNCABLE_DECLARE(fill);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  static Napi::Value asMDArray(const Napi::CallbackInfo &info);
#endif
  static Napi::Value getStatistics(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(computeStatistics);
  static Napi::Value setStatistics(const Napi::CallbackInfo &info);
  static Napi::Value getMaskBand(const Napi::CallbackInfo &info);
  static Napi::Value getMaskFlags(const Napi::CallbackInfo &info);
  static Napi::Value createMaskBand(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(getMetadata);
  GDAL_ASYNCABLE_DECLARE(setMetadata);
  Napi::Value dsGetter(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_GETTER_DECLARE(sizeGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(idGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(descriptionGetter);
  Napi::Value overviewsGetter(const Napi::CallbackInfo &info);
  Napi::Value pixelsGetter(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_GETTER_DECLARE(blockSizeGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(minimumGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(maximumGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(readOnlyGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(dataTypeGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(hasArbitraryOverviewsGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(unitTypeGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(scaleGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(offsetGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(noDataValueGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(categoryNamesGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(colorInterpretationGetter);
  GDAL_ASYNCABLE_GETTER_DECLARE(colorTableGetter);
  Napi::Value uidGetter(const Napi::CallbackInfo &info);

  void unitTypeSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void scaleSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void offsetSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void noDataValueSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void categoryNamesSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void colorInterpretationSetter(const Napi::CallbackInfo &info, const Napi::Value &value);
  void colorTableSetter(const Napi::CallbackInfo &info, const Napi::Value &value);

  RasterBand();
  RasterBand(GDALRasterBand *band);
  inline bool isAlive() {
    return this_ && object_store.isAlive(uid);
  }
  inline GDALRasterBand *get() {
    return this_;
  }
  inline GDALDataset *getParent() {
    return parent_ds;
  }
  void dispose();
  long uid;
  // Dataset that will be locked
  long parent_uid;

    private:
  ~RasterBand();
  GDALRasterBand *this_;
  GDALDataset *parent_ds;
};

} // namespace node_gdal
#endif
