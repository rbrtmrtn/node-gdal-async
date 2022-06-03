#include "rasterband_overviews.hpp"
#include "../gdal_common.hpp"
#include "../gdal_rasterband.hpp"

namespace node_gdal {

Napi::FunctionReference RasterBandOverviews::constructor;

void RasterBandOverviews::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, RasterBandOverviews::New);

  lcons->SetClassName(Napi::String::New(env, "RasterBandOverviews"));

  InstanceMethod("toString", &toString),
  Nan__SetPrototypeAsyncableMethod(lcons, "count", count);
  Nan__SetPrototypeAsyncableMethod(lcons, "get", get);
  Nan__SetPrototypeAsyncableMethod(lcons, "getBySampleCount", getBySampleCount);

  (target).Set(Napi::String::New(env, "RasterBandOverviews"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

RasterBandOverviews::RasterBandOverviews() : Napi::ObjectWrap<RasterBandOverviews>(){
}

RasterBandOverviews::~RasterBandOverviews() {
}

/**
 * An encapsulation of a {@link RasterBand} overview functionality.
 *
 * @class RasterBandOverviews
 */
Napi::Value RasterBandOverviews::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    RasterBandOverviews *f = static_cast<RasterBandOverviews *>(ptr);
    f->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create RasterBandOverviews directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value RasterBandOverviews::New(Napi::Value band_obj) {
  Napi::Env env = band_obj.Env();
  Napi::EscapableHandleScope scope(env);

  RasterBandOverviews *wrapped = new RasterBandOverviews();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, RasterBandOverviews::constructor)), 1, &ext)
      ;
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), band_obj);

  return scope.Escape(obj);
}

Napi::Value RasterBandOverviews::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "RasterBandOverviews");
}

/**
 * Fetches the overview at the provided index.
 *
 * @method get
 * @instance
 * @memberof RasterBandOverviews
 * @throws {Error}
 * @param {number} index 0-based index
 * @return {RasterBand}
 */

/**
 * Fetches the overview at the provided index.
 * @async
 *
 * @method getAsync
 * @instance
 * @memberof RasterBandOverviews
 * @throws {Error}
 * @param {number} index 0-based index
 * @param {callback<RasterBand>} [callback=undefined]
 * @return {Promise<RasterBand>}
 */
GDAL_ASYNCABLE_DEFINE(RasterBandOverviews::get) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();

  NODE_UNWRAP_CHECK(RasterBand, parent, band);

  int id;
  NODE_ARG_INT(0, "id", id);

  GDALAsyncableJob<GDALRasterBand *> job(band->parent_uid);
  job.persist(parent);
  job.main = [band, id](const GDALExecutionProgress &) {
    CPLErrorReset();
    GDALRasterBand *result = band->get()->GetOverview(id);
    if (result == nullptr) { throw "Specified overview not found"; }
    return result;
  };
  job.rval = [band](GDALRasterBand *result, const GetFromPersistentFunc &) {
    return RasterBand::New(result, band->getParent());
  };
  job.run(info, async, 1);
}

/**
 * Fetch best sampling overview.
 *
 * Returns the most reduced overview of the given band that still satisfies the
 * desired number of samples. This function can be used with zero as the number
 * of desired samples to fetch the most reduced overview. The same band as was
 * passed in will be returned if it has not overviews, or if none of the
 * overviews have enough samples.
 *
 * @method getBySampleCount
 * @instance
 * @memberof RasterBandOverviews
 * @param {number} samples
 * @return {RasterBand}
 */

/**
 * Fetch best sampling overview.
 * @async
 *
 * Returns the most reduced overview of the given band that still satisfies the
 * desired number of samples. This function can be used with zero as the number
 * of desired samples to fetch the most reduced overview. The same band as was
 * passed in will be returned if it has not overviews, or if none of the
 * overviews have enough samples.
 *
 * @method getBySampleCountAsync
 * @instance
 * @memberof RasterBandOverviews
 * @param {number} samples
 * @param {callback<RasterBand>} [callback=undefined]
 * @return {Promise<RasterBand>}
 */
GDAL_ASYNCABLE_DEFINE(RasterBandOverviews::getBySampleCount) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  NODE_UNWRAP_CHECK(RasterBand, parent, band);

  int n_samples;
  NODE_ARG_INT(0, "minimum number of samples", n_samples);

  GDALAsyncableJob<GDALRasterBand *> job(band->parent_uid);
  job.persist(parent);
  job.main = [band, n_samples](const GDALExecutionProgress &) {
    CPLErrorReset();
    GDALRasterBand *result = band->get()->GetRasterSampleOverview(n_samples);
    if (result == nullptr) { throw "Specified overview not found"; }
    return result;
  };
  job.rval = [band](GDALRasterBand *result, const GetFromPersistentFunc &) {
    return RasterBand::New(result, band->getParent());
  };
  job.run(info, async, 1);
}

/**
 * Returns the number of overviews.
 *
 * @method count
 * @instance
 * @memberof RasterBandOverviews
 * @return {number}
 */

/**
 * Returns the number of overviews.
 * @async
 *
 * @method countAsync
 * @instance
 * @memberof RasterBandOverviews
 * @param {callback<number>} [callback=undefined]
 * @return {Promise<number>}
 */
GDAL_ASYNCABLE_DEFINE(RasterBandOverviews::count) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  NODE_UNWRAP_CHECK(RasterBand, parent, band);

  GDALAsyncableJob<int> job(band->parent_uid);
  job.persist(parent);
  job.main = [band](const GDALExecutionProgress &) {
    int count = band->get()->GetOverviewCount();
    return count;
  };
  job.rval = [](int count, const GetFromPersistentFunc &) { return Napi::Number::New(env, count); };
  job.run(info, async, 0);
}

} // namespace node_gdal
