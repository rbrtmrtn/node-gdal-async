#include <memory>
#include "gdal_driver.hpp"
#include "gdal_common.hpp"
#include "gdal_dataset.hpp"
#include "gdal_majorobject.hpp"
#include "async.hpp"
#include "utils/string_list.hpp"

namespace node_gdal {

Napi::FunctionReference Driver::constructor;

void Driver::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Driver::New);

  lcons->SetClassName(Napi::String::New(env, "Driver"));

  InstanceMethod("toString", &toString),
  Nan__SetPrototypeAsyncableMethod(lcons, "open", open);
  Nan__SetPrototypeAsyncableMethod(lcons, "create", create);
  Nan__SetPrototypeAsyncableMethod(lcons, "createCopy", createCopy);
  InstanceMethod("deleteDataset", &deleteDataset),
  InstanceMethod("rename", &rename),
  InstanceMethod("copyFiles", &copyFiles),
  InstanceMethod("getMetadata", &getMetadata),

  ATTR(lcons, "description", descriptionGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Driver"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Driver::Driver(GDALDriver *driver) : Napi::ObjectWrap<Driver>(), this_gdaldriver(driver) {
  LOG("Created GDAL Driver [%p]", driver);
}
Driver::Driver() : Napi::ObjectWrap<Driver>(), this_gdaldriver(0) {
}

Driver::~Driver() {
  dispose();
}

void Driver::dispose() {
  if (this_gdaldriver) {
    LOG("Disposing GDAL Driver [%p]", this_gdaldriver);
    object_store.dispose(uid);
    LOG("Disposed GDAL Driver [%p]", this_gdaldriver);
    this_gdaldriver = NULL;
  }
}

/**
 * Format specific driver.
 *
 * An instance of this class is created for each supported format, and
 * manages information about the format.
 *
 * This roughly corresponds to a file format, though some drivers may
 * be gateways to many formats through a secondary multi-library.
 *
 * @class Driver
 */
Napi::Value Driver::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    Driver *f = static_cast<Driver *>(ptr);
    f->Wrap(info.This());

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create Driver directly").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value Driver::New(GDALDriver *driver) {
  Napi::EscapableHandleScope scope(env);

  if (!driver) { return scope.Escape(env.Null()); }
  if (object_store.has(driver)) { return scope.Escape(object_store.get(driver)); }

  Driver *wrapped = new Driver(driver);
  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Driver::constructor)), 1, &ext);

  // LOG("ADDING DRIVER TO CACHE [%p]", driver);
  wrapped->uid = object_store.add(driver, wrapped->persistent(), 0);
  // LOG("DONE ADDING DRIVER TO CACHE [%p]", driver);

  return scope.Escape(obj);
}

Napi::Value Driver::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Driver");
}

/**
 * @readonly
 * @kind member
 * @name description
 * @instance
 * @memberof Driver
 * @type {string}
 */
Napi::Value Driver::descriptionGetter(const Napi::CallbackInfo& info) {
  Driver *driver = info.This().Unwrap<Driver>();

  return SafeString::New(driver->getGDALDriver()->GetDescription());
}

/**
 * @throws {Error}
 * @method deleteDataset
 * @instance
 * @memberof Driver
 * @param {string} filename
 */
Napi::Value Driver::deleteDataset(const Napi::CallbackInfo& info) {

  std::string name("");
  NODE_ARG_STR(0, "dataset name", name);

  Driver *driver = info.This().Unwrap<Driver>();

  CPLErr err = driver->getGDALDriver()->Delete(name.c_str());
  if (err) {
    NODE_THROW_LAST_CPLERR;
    return;
  }
  return;
}

// This is shared across all Driver functions
auto DatasetRval = [](GDALDataset *ds, const GetFromPersistentFunc &) { return Dataset::New(ds); };

/**
 * Create a new dataset with this driver.
 *
 * @throws {Error}
 * @method create
 * @instance
 * @memberof Driver
 * @param {string} filename
 * @param {number} [x_size=0] raster width in pixels (ignored for vector
 * datasets)
 * @param {number} [y_size=0] raster height in pixels (ignored for vector
 * datasets)
 * @param {number} [band_count=0]
 * @param {number} [data_type=GDT_Byte] pixel data type (ignored for
 * vector datasets) (see {@link GDT|data types}
 * @param {StringOptions} [creation_options] An array or object containing
 * driver-specific dataset creation options
 * @throws {Error}
 * @return {Dataset}
 */

/**
 * Asynchronously create a new dataset with this driver.
 *
 * @throws {Error}
 * @method createAsync
 * @instance
 * @memberof Driver
 * @param {string} filename
 * @param {number} [x_size=0] raster width in pixels (ignored for vector
 * datasets)
 * @param {number} [y_size=0] raster height in pixels (ignored for vector
 * datasets)
 * @param {number} [band_count=0]
 * @param {number} [data_type=GDT_Byte] pixel data type (ignored for
 * vector datasets) (see {@link GDT|data types}
 * @param {StringOptions} [creation_options] An array or object containing
 * driver-specific dataset creation options
 * @param {callback<Dataset>} [callback=undefined]
 * @throws {Error}
 * @return {Promise<Dataset>}
 */
GDAL_ASYNCABLE_DEFINE(Driver::create) {
  Driver *driver = info.This().Unwrap<Driver>();

  std::string filename;
  unsigned int x_size = 0, y_size = 0, n_bands = 0;
  GDALDataType type = GDT_Byte;
  std::string type_name = "";
  StringList *options = new StringList;

  NODE_ARG_STR(0, "filename", filename);

  if (info.Length() < 3) {
    if (info.Length() > 1 && options->parse(info[1])) {
      Napi::Error::New(env, "Failed parsing options").ThrowAsJavaScriptException();
      return env.Null(); // error parsing string list
    }
  } else {
    NODE_ARG_INT(1, "x size", x_size);
    NODE_ARG_INT(2, "y size", y_size);
    NODE_ARG_INT_OPT(3, "number of bands", n_bands);
    NODE_ARG_OPT_STR(4, "data type", type_name);
    if (info.Length() > 5 && options->parse(info[5])) {
      Napi::Error::New(env, "Failed parsing options").ThrowAsJavaScriptException();
      return env.Null(); // error parsing string list
    }
    if (!type_name.empty()) { type = GDALGetDataTypeByName(type_name.c_str()); }
  }

  GDALDriver *raw = driver->getGDALDriver();

  // Very careful here
  // we can't reference automatic variables, thus the *options object
  GDALAsyncableJob<GDALDataset *> job(0);
  job.persist(driver->handle());
  job.main = [raw, filename, x_size, y_size, n_bands, type, options](const GDALExecutionProgress &) {
    std::unique_ptr<StringList> options_ptr(options);
    CPLErrorReset();
    GDALDataset *ds = raw->Create(filename.c_str(), x_size, y_size, n_bands, type, options->get());
    if (!ds) throw CPLGetLastErrorMsg();
    return ds;
  };
  job.rval = DatasetRval;

  job.run(info, async, 6);
}

/**
 * @typedef {object} CreateOptions
 * @property {ProgressCb} [progress_cb]
 */

/**
 * Create a copy of a dataset.
 *
 * @throws {Error}
 * @method createCopy
 * @instance
 * @memberof Driver
 * @param {string} filename
 * @param {Dataset} src
 * @param {StringOptions} [options=null] An array or object containing driver-specific dataset creation options
 * @param {boolean} [strict=false] strict mode
 * @param {CreateOptions} [jsoptions] additional options
 * @param {ProgressCb} [jsoptions.progress_cb]
 * @return {Dataset}
 */

/**
 * Asynchronously create a copy of a dataset.
 *
 * @throws {Error}
 * @method createCopyAsync
 * @instance
 * @memberof Driver
 * @param {string} filename
 * @param {Dataset} src
 * @param {StringOptions} [options=null] An array or object containing driver-specific dataset creation options
 * @param {boolean} [strict=false] strict mode
 * @param {CreateOptions} [jsoptions] additional options
 * @param {ProgressCb} [jsoptions.progress_cb]
 * @param {callback<Dataset>} [callback=undefined]
 * @return {Promise<Dataset>}
 */
GDAL_ASYNCABLE_DEFINE(Driver::createCopy) {
  Driver *driver = info.This().Unwrap<Driver>();

  if (!driver->isAlive()) {
    Napi::Error::New(env, "Driver object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string filename;
  Dataset *src_dataset;
  StringList *options;

  NODE_ARG_STR(0, "filename", filename);

  // NODE_ARG_STR(1, "source dataset", src_dataset)
  if (info.Length() < 2) {
    Napi::Error::New(env, "source dataset must be provided").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (IS_WRAPPED(info[1], Dataset)) {
    src_dataset = info[1].As<Napi::Object>().Unwrap<Dataset>();
  } else {
    Napi::Error::New(env, "source dataset must be a Dataset object").ThrowAsJavaScriptException();
    return env.Null();
  }

  options = new StringList;
  if (info.Length() > 2 && options->parse(info[2])) {
    Napi::Error::New(env, "Failed parsing options").ThrowAsJavaScriptException();
    return env.Null(); // error parsing string list
  }

  bool strict = false;
  NODE_ARG_BOOL_OPT(3, "strict", strict);

  Napi::Object jsoptions;
  Napi::FunctionReference *progress_cb = nullptr;
  NODE_ARG_OBJECT_OPT(4, "jsoptions", jsoptions);
  if (!jsoptions.IsEmpty()) NODE_CB_FROM_OBJ_OPT(jsoptions, "progress_cb", progress_cb);

  GDALDriver *raw = driver->getGDALDriver();
  GDALDataset *raw_ds = src_dataset->get();
  GDALAsyncableJob<GDALDataset *> job(src_dataset->uid);
  job.rval = DatasetRval;
  job.persist(driver->handle());
  job.progress = progress_cb;

  job.main = [raw, filename, raw_ds, strict, options, progress_cb](const GDALExecutionProgress &progress) {
    std::unique_ptr<StringList> options_ptr(options);
    CPLErrorReset();
    GDALDataset *ds = raw->CreateCopy(
      filename.c_str(), raw_ds, strict, options->get(), progress_cb ? ProgressTrampoline : nullptr, (void *)&progress);
    if (!ds) throw CPLGetLastErrorMsg();
    return ds;
  };
  job.run(info, async, 5);
}

/**
 * Copy the files of a dataset.
 *
 * @throws {Error}
 * @method copyFiles
 * @instance
 * @memberof Driver
 * @param {string} name_old New name for the dataset.
 * @param {string} name_new Old name of the dataset.
 */
Napi::Value Driver::copyFiles(const Napi::CallbackInfo& info) {
  Driver *driver = info.This().Unwrap<Driver>();
  std::string old_name;
  std::string new_name;

  NODE_ARG_STR(0, "new name", new_name);
  NODE_ARG_STR(1, "old name", old_name);

  CPLErr err = driver->getGDALDriver()->CopyFiles(new_name.c_str(), old_name.c_str());
  if (err) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  return;
}

/**
 * Renames the dataset.
 *
 * @throws {Error}
 * @method rename
 * @instance
 * @memberof Driver
 * @param {string} new_name New name for the dataset.
 * @param {string} old_name Old name of the dataset.
 */
Napi::Value Driver::rename(const Napi::CallbackInfo& info) {
  Driver *driver = info.This().Unwrap<Driver>();
  std::string old_name;
  std::string new_name;

  NODE_ARG_STR(0, "new name", new_name);
  NODE_ARG_STR(1, "old name", old_name);

  CPLErr err = driver->getGDALDriver()->Rename(new_name.c_str(), old_name.c_str());
  if (err) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  return;
}

/**
 * Returns metadata about the driver.
 *
 * @throws {Error}
 * @method getMetadata
 * @instance
 * @memberof Driver
 * @param {string} [domain]
 * @return {any}
 */
Napi::Value Driver::getMetadata(const Napi::CallbackInfo& info) {
  Driver *driver = info.This().Unwrap<Driver>();

  Napi::Object result;

  std::string domain("");
  NODE_ARG_OPT_STR(0, "domain", domain);

  GDALDriver *raw = driver->getGDALDriver();
  char **md = raw->GetMetadata(domain.empty() ? NULL : domain.c_str());
  result = MajorObject::getMetadata(md);
  return result;
}

/**
 * Opens a dataset.
 *
 * @throws {Error}
 * @method open
 * @instance
 * @memberof Driver
 * @param {string} path
 * @param {string} [mode="r"] The mode to use to open the file: `"r"` or
 * `"r+"`
 * @param {StringOptions} [options] Driver-specific open options
 * @return {Dataset}
 */

/**
 * Opens a dataset.
 *
 * @throws {Error}
 * @method openAsync
 * @instance
 * @memberof Driver
 * @param {string} path
 * @param {string} [mode="r"] The mode to use to open the file: `"r"` or
 * `"r+"`
 * @param {StringOptions} [options] Driver-specific open options
 * @param {callback<Dataset>} [callback=undefined]
 * @return {Promise<Dataset>}
 */
GDAL_ASYNCABLE_DEFINE(Driver::open) {
  Driver *driver = info.This().Unwrap<Driver>();

  std::string path;
  std::string mode = "r";
  GDALAccess access = GA_ReadOnly;

  NODE_ARG_STR(0, "path", path);
  NODE_ARG_OPT_STR(1, "mode", mode);

  if (mode == "r+") {
    access = GA_Update;
  } else if (mode != "r") {
    Napi::Error::New(env, "Invalid open mode. Must be \"r\" or \"r+\"").ThrowAsJavaScriptException();
    return env.Null();
  }

  StringList *options = new StringList;
  if (info.Length() > 2 && options->parse(info[2])) {
    Napi::Error::New(env, "Failed parsing options").ThrowAsJavaScriptException();
    return env.Null(); // error parsing string list
  }

  GDALDriver *raw = driver->getGDALDriver();

  GDALAsyncableJob<GDALDataset *> job(0);
  job.persist(driver->handle());
  job.main = [raw, path, access, options](const GDALExecutionProgress &) {
    std::unique_ptr<StringList> options_ptr(options);
    const char *driver_list[2] = {raw->GetDescription(), nullptr};
    CPLErrorReset();
    GDALDataset *ds = (GDALDataset *)GDALOpenEx(path.c_str(), access, driver_list, options->get(), NULL);
    if (!ds) throw CPLGetLastErrorMsg();
    return ds;
  };
  job.rval = DatasetRval;

  job.run(info, async, 2);
}

} // namespace node_gdal
