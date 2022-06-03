#include <memory>
#include "dataset_bands.hpp"
#include "../gdal_common.hpp"
#include "../gdal_dataset.hpp"
#include "../gdal_rasterband.hpp"
#include "../utils/string_list.hpp"

namespace node_gdal {

Napi::FunctionReference DatasetBands::constructor;

void DatasetBands::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, DatasetBands::New);

  lcons->SetClassName(Napi::String::New(env, "DatasetBands"));

  InstanceMethod("toString", &toString),
  Nan__SetPrototypeAsyncableMethod(lcons, "count", count);
  Nan__SetPrototypeAsyncableMethod(lcons, "create", create);
  Nan__SetPrototypeAsyncableMethod(lcons, "get", get);

  ATTR_DONT_ENUM(lcons, "ds", dsGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "DatasetBands"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

DatasetBands::DatasetBands() : Napi::ObjectWrap<DatasetBands>(){
}

DatasetBands::~DatasetBands() {
}

/**
 * An encapsulation of a {@link Dataset}
 * raster bands.
 *
 * @example
 * var bands = dataset.bands;
 *
 * @class DatasetBands
 */
Napi::Value DatasetBands::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    DatasetBands *f = static_cast<DatasetBands *>(ptr);
    f->Wrap(info.This());
    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create DatasetBands directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value DatasetBands::New(Napi::Value ds_obj) {
  Napi::Env env = ds_obj.Env();
  Napi::EscapableHandleScope scope(env);

  DatasetBands *wrapped = new DatasetBands();

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, DatasetBands::constructor)), 1, &ext);
  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), ds_obj);

  return scope.Escape(obj);
}

Napi::Value DatasetBands::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "DatasetBands");
}

/**
 * Returns the band with the given ID.
 *
 * @method get
 * @instance
 * @memberof DatasetBands
 * @param {number} id
 * @throws {Error}
 * @return {RasterBand}
 */

/**
 * Returns the band with the given ID.
 * @async
 *
 * @method getAsync
 * @instance
 * @memberof DatasetBands
 *
 * @param {number} id
 * @param {callback<RasterBand>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<RasterBand>}
 */
GDAL_ASYNCABLE_DEFINE(DatasetBands::get) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Dataset *ds = parent.Unwrap<Dataset>();

  if (!ds->isAlive()) {
    Napi::Error::New(env, "Dataset object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  GDALDataset *raw = ds->get();
  int band_id;
  NODE_ARG_INT(0, "band id", band_id);

  GDALAsyncableJob<GDALRasterBand *> job(ds->uid);
  job.persist(parent);
  job.main = [raw, band_id](const GDALExecutionProgress &) {
    CPLErrorReset();
    GDALRasterBand *band = raw->GetRasterBand(band_id);
    if (band == nullptr) { throw CPLGetLastErrorMsg(); }
    return band;
  };
  job.rval = [raw](GDALRasterBand *band, const GetFromPersistentFunc &) { return RasterBand::New(band, raw); };
  job.run(info, async, 1);
}

/**
 * Adds a new band.
 *
 * @method create
 * @instance
 * @memberof DatasetBands
 * @throws {Error}
 * @param {string} dataType Type of band ({@link GDT|see GDT constants})
 * @param {object|string[]} [options] Creation options
 * @return {RasterBand}
 */

/**
 * Adds a new band.
 * @async
 *
 * @method createAsync
 * @instance
 * @memberof DatasetBands
 * @throws {Error}
 * @param {string} dataType Type of band ({@link GDT|see GDT constants})
 * @param {object|string[]} [options] Creation options
 * @param {callback<RasterBand>} [callback=undefined]
 * @return {Promise<RasterBand>}
 */

GDAL_ASYNCABLE_DEFINE(DatasetBands::create) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Dataset *ds = parent.Unwrap<Dataset>();

  if (!ds->isAlive()) {
    Napi::Error::New(env, "Dataset object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  GDALDataset *raw = ds->get();
  GDALDataType type;
  std::shared_ptr<StringList> options(new StringList);

  // NODE_ARG_ENUM(0, "data type", GDALDataType, type);
  if (info.Length() < 1) {
    Napi::Error::New(env, "data type argument needed").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsString()) {
    std::string type_name = info[0].As<Napi::String>().Utf8Value().c_str();
    type = GDALGetDataTypeByName(type_name.c_str());
  } else if (info[0].IsNull() || info[0].IsUndefined()) {
    type = GDT_Unknown;
  } else {
    Napi::Error::New(env, "data type must be string or undefined").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() > 1 && options->parse(info[1])) {
    return; // error parsing creation options, options->parse does the throwing
  }

  GDALAsyncableJob<GDALRasterBand *> job(ds->uid);
  job.persist(parent);
  job.main = [raw, type, options](const GDALExecutionProgress &) {
    CPLErrorReset();
    CPLErr err = raw->AddBand(type, options->get());
    if (err != CE_None) { throw CPLGetLastErrorMsg(); }
    return raw->GetRasterBand(raw->GetRasterCount());
  };
  job.rval = [raw](GDALRasterBand *r, const GetFromPersistentFunc &) { return RasterBand::New(r, raw); };
  job.run(info, async, 2);
}

/**
 * Returns the number of bands.
 *
 * @method count
 * @instance
 * @memberof DatasetBands
 * @return {number}
 */

/**
 *
 * Returns the number of bands.
 * @async
 *
 * @method countAsync
 * @instance
 * @memberof DatasetBands
 *
 * @param {callback<number>} [callback=undefined]
 * @return {Promise<number>}
 */
GDAL_ASYNCABLE_DEFINE(DatasetBands::count) {

  Napi::Object parent =
    Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_")).As<Napi::Object>();
  Dataset *ds = parent.Unwrap<Dataset>();

  if (!ds->isAlive()) {
    Napi::Error::New(env, "Dataset object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  GDALDataset *raw = ds->get();
  GDALAsyncableJob<int> job(ds->uid);
  job.persist(parent);
  job.main = [raw](const GDALExecutionProgress &) {
    int count = raw->GetRasterCount();
    return count;
  };
  job.rval = [](int count, const GetFromPersistentFunc &) { return Napi::Number::New(env, count); };
  job.run(info, async, 0);
}

/**
 * Returns the parent dataset.
 *
 * @readonly
 * @kind member
 * @name ds
 * @instance
 * @memberof DatasetBands
 * @type {Dataset}
 */
Napi::Value DatasetBands::dsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
}

} // namespace node_gdal
