#include "gdal_mdarray.hpp"
#include "gdal_group.hpp"
#include "gdal_common.hpp"
#include "gdal_driver.hpp"
#include "collections/array_dimensions.hpp"
#include "collections/array_attributes.hpp"
#include "geometry/gdal_geometry.hpp"
#include "gdal_layer.hpp"
#include "gdal_majorobject.hpp"
#include "gdal_spatial_reference.hpp"
#include "utils/typed_array.hpp"

namespace node_gdal {

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

Napi::FunctionReference MDArray::constructor;

void MDArray::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, MDArray::New);

  lcons->SetClassName(Napi::String::New(env, "MDArray"));

  InstanceMethod("toString", &toString), Nan__SetPrototypeAsyncableMethod(lcons, "read", read);
  InstanceMethod("getView", &getView), InstanceMethod("getMask", &getMask), InstanceMethod("asDataset", &asDataset),

    ATTR_DONT_ENUM(lcons, "_uid", uidGetter, READ_ONLY_SETTER);
  ATTR(lcons, "srs", srsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "dataType", typeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "length", lengthGetter, READ_ONLY_SETTER);
  ATTR(lcons, "unitType", unitTypeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "scale", scaleGetter, READ_ONLY_SETTER);
  ATTR(lcons, "offset", offsetGetter, READ_ONLY_SETTER);
  ATTR(lcons, "noDataValue", noDataValueGetter, READ_ONLY_SETTER);
  ATTR(lcons, "description", descriptionGetter, READ_ONLY_SETTER);
  ATTR(lcons, "dimensions", dimensionsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "attributes", attributesGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "MDArray"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

MDArray::MDArray(std::shared_ptr<GDALMDArray> md) : Napi::ObjectWrap<MDArray>(), uid(0), this_(md) {
  LOG("Created MDArray [%p]", md.get());
}

MDArray::~MDArray() {
  dispose();
}

void MDArray::dispose() {
  if (this_) {

    LOG("Disposing array [%p]", this_.get());

    object_store.dispose(uid);

    LOG("Disposed array [%p]", this_.get());
  }
};

/**
 * A representation of an array with access methods.
 *
 * @class MDArray
 */
Napi::Value MDArray::New(const Napi::CallbackInfo &info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword")
      .ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 2 && info[0].IsExternal() && info[1].IsObject()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    MDArray *f = static_cast<MDArray *>(ptr);
    f->Wrap(info.This());

    Napi::Value dims = ArrayDimensions::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "dims_"), dims);
    Napi::Value attrs = ArrayAttributes::New(info.This(), info[1]);
    Napi::SetPrivate(info.This(), Napi::String::New(env, "attrs_"), attrs);

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create MDArray directly. Create with dataset instead.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return info.This();
}

Napi::Value MDArray::New(std::shared_ptr<GDALMDArray> raw, GDALDataset *parent_ds) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  MDArray *wrapped = new MDArray(raw);

  // add reference to datasource so datasource doesnt get GC'ed while group is
  // alive
  Napi::Object ds, group;
  if (object_store.has(parent_ds)) {
    ds = object_store.get(parent_ds);
  } else {
    LOG("MDArray's parent dataset disappeared from cache (array = %p, dataset = %p)", raw.get(), parent_ds);
    Napi::Error::New(env, "MDArray's parent dataset disappeared from cache").ThrowAsJavaScriptException();

    return scope.Escape(env.Undefined());
  }

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Value argv[] = {ext, ds};
  Napi::Object obj = Napi::NewInstance(Napi::GetFunction(Napi::New(env, MDArray::constructor)), 2, argv);

  size_t dim = raw->GetDimensionCount();

  Dataset *unwrapped_ds = ds.Unwrap<Dataset>();
  long parent_uid = unwrapped_ds->uid;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), parent_uid);
  wrapped->parent_ds = parent_ds;
  wrapped->parent_uid = parent_uid;
  wrapped->dimensions = dim;

  Napi::SetPrivate(obj, Napi::String::New(env, "ds_"), ds);

  return scope.Escape(obj);
}

Napi::Value MDArray::toString(const Napi::CallbackInfo &info) {
  return Napi::String::New(env, "MDArray");
}

/* Find the lowest possible element index for the given spans and strides */
static inline int
findLowest(int dimensions, std::shared_ptr<size_t> span, std::shared_ptr<GPtrDiff_t> stride, GPtrDiff_t offset) {
  GPtrDiff_t dimStride = 1;
  GPtrDiff_t lowest = 0;
  for (int dim = 0; dim < dimensions; dim++) {
    if (stride != nullptr) {
      // strides are given
      dimStride = stride.get()[dim];
    } else {
      // default strides: 1 on the first dimension -or- (size of the current dimensions) x (stride of the previous dimension)
      if (dim > 0) dimStride = dimStride * span.get()[dim - 1];
    }

    // Lowest address element on this dimension
    size_t element;
    if (dimStride < 0)
      element = span.get()[dim] - 1;
    else
      element = 0;

    lowest += element * dimStride;
  }

  return offset + lowest;
}

/* Find the highest possible element index for the given spans and strides */
static inline int
findHighest(int dimensions, std::shared_ptr<size_t> span, std::shared_ptr<GPtrDiff_t> stride, GPtrDiff_t offset) {
  GPtrDiff_t dimStride = 1;
  GPtrDiff_t highest = 0;
  for (int dim = 0; dim < dimensions; dim++) {
    if (stride != nullptr) {
      // strides are given
      dimStride = stride.get()[dim];
    } else {
      // default strides: 1 on the first dimension -or- (size of the current dimensions) x (stride of the previous dimension)
      if (dim > 0) dimStride = dimStride * span.get()[dim - 1];
    }

    // Highest address element on this dimension
    size_t element;
    if (dimStride > 0)
      element = span.get()[dim] - 1;
    else
      element = 0;

    highest += element * dimStride;
  }

  return offset + highest;
}

/**
 * @typedef {object} MDArrayOptions
 * @property {number[]} origin
 * @property {number[]} span
 * @property {number[]} [stride]
 * @property {string} [data_type]
 * @property {TypedArray} [data]
 * @property {number} [_offset]
 */

/**
 * Read data from the MDArray.
 *
 * This will extract the context of a (hyper-)rectangle from the array into a buffer.
 * If the buffer can be passed as an argument or it can be allocated by the function.
 * Generalized n-dimensional strides are supported.
 *
 * Although this method can be used in its raw form, it works best when used with the ndarray plugin.
 *
 * @method read
 * @instance
 * @memberof MDArray
 * @throws {Error}
 * @param {MDArrayOptions} options
 * @param {number[]} options.origin An array of the starting indices
 * @param {number[]} options.span An array specifying the number of elements to read in each dimension
 * @param {number[]} [options.stride] An array of strides for the output array, mandatory if the array is specified
 * @param {string} [options.data_type] See {@link GDT|GDT constants}
 * @param {TypedArray} [options.data] The `TypedArray` to put the data in. A new array is created if not given.
 * @return {TypedArray}
 */

/**
 * Read data from the MDArray.
 * @async
 *
 * This will extract the context of a (hyper-)rectangle from the array into a buffer.
 * If the buffer can be passed as an argument or it can be allocated by the function.
 * Generalized n-dimensional strides are supported.
 *
 * Although this method can be used in its raw form, it works best when used with the ndarray plugin.
 *
 * @method readAsync
 * @instance
 * @memberof MDArray
 * @throws {Error}
 * @param {MDArrayOptions} options
 * @param {number[]} options.origin An array of the starting indices
 * @param {number[]} options.span An array specifying the number of elements to read in each dimension
 * @param {number[]} [options.stride] An array of strides for the output array, mandatory if the array is specified
 * @param {string} [options.data_type] See {@link GDT|GDT constants}
 * @param {TypedArray} [options.data] The `TypedArray` to put the data in. A new array is created if not given.
 * @param {ProgressCb} [options.progress_cb]
 * @param {callback<TypedArray>} [callback=undefined]
 * @return {Promise<TypedArray>} A `TypedArray` of values.
 */
GDAL_ASYNCABLE_DEFINE(MDArray::read) {

  NODE_UNWRAP_CHECK(MDArray, info.This(), self);

  Napi::Object options;
  Napi::Array origin, span, stride;
  std::string type_name;
  GDALDataType type = GDT_Byte;
  GPtrDiff_t offset = 0;

  NODE_ARG_OBJECT(0, "options", options);
  NODE_ARRAY_FROM_OBJ(options, "origin", origin);
  NODE_ARRAY_FROM_OBJ(options, "span", span);
  NODE_ARRAY_FROM_OBJ_OPT(options, "stride", stride);
  NODE_STR_FROM_OBJ_OPT(options, "data_type", type_name);
  NODE_INT64_FROM_OBJ_OPT(options, "_offset", offset);
  if (!type_name.empty()) { type = GDALGetDataTypeByName(type_name.c_str()); }

  std::shared_ptr<GUInt64> gdal_origin;
  std::shared_ptr<size_t> gdal_span;
  std::shared_ptr<GPtrDiff_t> gdal_stride;
  try {
    gdal_origin = NumberArrayToSharedPtr<int64_t, GUInt64>(origin, self->dimensions);
    gdal_span = NumberArrayToSharedPtr<int64_t, size_t>(span, self->dimensions);
    gdal_stride = NumberArrayToSharedPtr<int64_t, GPtrDiff_t>(stride, self->dimensions);
  } catch (const char *e) {
    Napi::Error::New(env, e).ThrowAsJavaScriptException();
    return env.Null();
  }
  GPtrDiff_t highest = findHighest(self->dimensions, gdal_span, gdal_stride, offset);
  GPtrDiff_t lowest = findLowest(self->dimensions, gdal_span, gdal_stride, offset);
  size_t length = (highest - (lowest < 0 ? lowest : 0)) + 1;

  Napi::String sym = Napi::String::New(env, "data");
  Napi::Value data;
  Napi::Object array;
  if (Napi::HasOwnProperty(options, sym).FromMaybe(false)) {
    data = (options).Get(sym);
    if (!data.IsNull() && !data.IsNull()) {
      array = data.As<Napi::Object>();
      type = node_gdal::TypedArray::Identify(array);
      if (type == GDT_Unknown) {
        Napi::Error::New(env, "Invalid array").ThrowAsJavaScriptException();
        return env.Null();
      }
    }
  }

  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, self, gdal_mdarray);

  // create array if no array was passed
  if (data.IsEmpty()) {
    if (type_name.empty()) {
      auto exType = gdal_mdarray->GetDataType();
      if (exType.GetClass() != GEDTC_NUMERIC) {
        Napi::TypeError::New(env, "Reading of extended data types is not supported yet").ThrowAsJavaScriptException();
        return env.Null();
      }
      type = exType.GetNumericDataType();
    }
    data = node_gdal::TypedArray::New(type, length);
    if (data.IsEmpty() || !data.IsObject()) {
      Napi::Error::New(env, "Failed to allocate array").ThrowAsJavaScriptException();
      return env.Null(); // TypedArray::New threw an error
    }
    array = data.As<Napi::Object>();
  }

  if (lowest < 0) {
    Napi::RangeError::New(env, "Will have to read before the start of the array").ThrowAsJavaScriptException();
    return env.Null();
  }

  void *buffer = node_gdal::TypedArray::Validate(array, type, length);
  if (!buffer) {
    Napi::Error::New(env, "Failed to allocate array").ThrowAsJavaScriptException();
    return env.Null(); // TypedArray::Validate threw an error
  }

  GDALAsyncableJob<bool> job(self->parent_uid);
  job.persist("array", array);

  job.main =
    [buffer, gdal_mdarray, gdal_origin, gdal_span, gdal_stride, type, length, offset](const GDALExecutionProgress &) {
      int bytes_per_pixel = GDALGetDataTypeSize(type) / 8;
      CPLErrorReset();
      GDALExtendedDataType gdal_type = GDALExtendedDataType::Create(type);
      bool success = gdal_mdarray->Read(
        gdal_origin.get(),
        gdal_span.get(),
        nullptr,
        gdal_stride.get(),
        gdal_type,
        (void *)((uint8_t *)buffer + offset * bytes_per_pixel),
        buffer,
        length * bytes_per_pixel);
      if (!success) { throw CPLGetLastErrorMsg(); }
      return success;
    };
  job.rval = [](bool success, const GetFromPersistentFunc &getter) { return getter("array"); };
  job.run(info, async, 1);
}

/**
 * Get a partial view of the MDArray.
 *
 * The slice expression uses the same syntax as NumPy basic slicing and indexing. See (https://www.numpy.org/devdocs/reference/arrays.indexing.html#basic-slicing-and-indexing). Or it can use field access by name. See (https://www.numpy.org/devdocs/reference/arrays.indexing.html#field-access).
 *
 * @method getView
 * @instance
 * @memberof MDArray
 * @throws {Error}
 * @param {string} view
 * @return {MDArray}
 */
Napi::Value MDArray::getView(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);

  std::string viewExpr;
  NODE_ARG_STR(0, "view", viewExpr);
  GDAL_LOCK_PARENT(array);
  CPLErrorReset();
  std::shared_ptr<GDALMDArray> view = raw->GetView(viewExpr);
  if (view == nullptr) {
    Napi::Error::New(env, CPLGetLastErrorMsg()).ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Value obj = New(view, array->parent_ds);
  return obj;
}

/**
 * Return an array that is a mask for the current array.
 *
 * This array will be of type Byte, with values set to 0 to indicate invalid pixels of the current array, and values set to 1 to indicate valid pixels.
 *
 * The generic implementation honours the NoDataValue, as well as various netCDF CF attributes: missing_value, _FillValue, valid_min, valid_max and valid_range.
 *
 * @method getMask
 * @instance
 * @memberof MDArray
 * @throws {Error}
 * @return {MDArray}
 */
Napi::Value MDArray::getMask(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);

  GDAL_LOCK_PARENT(array);
  CPLErrorReset();
  std::shared_ptr<GDALMDArray> mask = raw->GetMask(NULL);
  if (mask == nullptr) {
    Napi::Error::New(env, CPLGetLastErrorMsg()).ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Value obj = New(mask, array->parent_ds);
  return obj;
}

/**
 * Return a view of this array as a gdal.Dataset (ie 2D)
 *
 * In the case of > 2D arrays, additional dimensions will be represented as raster bands.
 *
 * @method asDataset
 * @instance
 * @memberof MDArray
 * @param {number|string} x dimension to be used as X axis
 * @param {number|string} y dimension to be used as Y axis
 * @throws {Error}
 * @return {Dataset}
 */
Napi::Value MDArray::asDataset(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);

  int x = -1, y = -1;
  std::string dim;
  NODE_ARG_STR_INT(0, "x", dim, x, isXString);
  if (isXString) x = ArrayDimensions::__getIdx(raw, dim);
  NODE_ARG_STR_INT(1, "y", dim, y, isYString);
  if (isYString) y = ArrayDimensions::__getIdx(raw, dim);

  GDAL_LOCK_PARENT(array);
  CPLErrorReset();
  GDALDataset *ds = raw->AsClassicDataset(x, y);
  if (ds == nullptr) {
    Napi::Error::New(env, CPLGetLastErrorMsg()).ThrowAsJavaScriptException();
    return env.Null();
  }
  Napi::Value obj = Dataset::New(ds, array->parent_ds);
  return obj;
}

/**
 * Spatial reference associated with MDArray.
 *
 * @throws {Error}
 * @kind member
 * @name srs
 * @instance
 * @memberof MDArray
 * @type {SpatialReference}
 */
Napi::Value MDArray::srsGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);
  GDAL_LOCK_PARENT(array);
  std::shared_ptr<OGRSpatialReference> srs = raw->GetSpatialRef();
  if (srs == nullptr) {
    return env.Null();
    return;
  }

  return SpatialReference::New(srs.get(), false);
}

/**
 * Raster value offset.
 *
 * @kind member
 * @name offset
 * @instance
 * @memberof MDArray
 * @type {number}
 */
Napi::Value MDArray::offsetGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  bool hasOffset = false;
  GDAL_LOCK_PARENT(array);
  double result = array->this_->GetOffset(&hasOffset);
  if (hasOffset)
    return Napi::Number::New(env, result);
  else
    return Napi::Number::New(env, 0);
}

/**
 * Raster value scale.
 *
 * @kind member
 * @name scale
 * @instance
 * @memberof MDArray
 * @type {number}
 */
Napi::Value MDArray::scaleGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  bool hasScale = false;
  GDAL_LOCK_PARENT(array);
  double result = array->this_->GetScale(&hasScale);
  if (hasScale)
    return Napi::Number::New(env, result);
  else
    return Napi::Number::New(env, 1);
}

/**
 * No data value for this array.
 *
 * @kind member
 * @name noDataValue
 * @instance
 * @memberof MDArray
 * @type {number|null}
 */
Napi::Value MDArray::noDataValueGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  bool hasNoData = false;
  GDAL_LOCK_PARENT(array);
  double result = array->this_->GetNoDataValueAsDouble(&hasNoData);

  if (hasNoData && !std::isnan(result)) {
    return Napi::Number::New(env, result);
    return;
  } else {
    return env.Null();
    return;
  }
}

/**
 * Raster unit type (name for the units of this raster's values).
 * For instance, it might be `"m"` for an elevation model in meters,
 * or `"ft"` for feet. If no units are available, a value of `""`
 * will be returned.
 *
 * @kind member
 * @name unitType
 * @instance
 * @memberof MDArray
 * @type {string}
 */
Napi::Value MDArray::unitTypeGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_LOCK_PARENT(array);
  std::string unit = array->this_->GetUnit();
  return SafeString::New(unit.c_str());
}

/**
 * @readonly
 * @kind member
 * @name dataType
 * @instance
 * @memberof MDArray
 * @type {string}
 */
Napi::Value MDArray::typeGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);
  GDAL_LOCK_PARENT(array);
  GDALExtendedDataType type = raw->GetDataType();
  const char *r;
  switch (type.GetClass()) {
    case GEDTC_NUMERIC: r = GDALGetDataTypeName(type.GetNumericDataType()); break;
    case GEDTC_STRING: r = "String"; break;
    case GEDTC_COMPOUND: r = "Compound"; break;
    default: Napi::Error::New(env, "Invalid attribute type").ThrowAsJavaScriptException(); return;
  }
  return SafeString::New(r);
}

/**
 * @readonly
 * @kind member
 * @name dimensions
 * @instance
 * @memberof MDArray
 * @type {GroupDimensions}
 */
Napi::Value MDArray::dimensionsGetter(const Napi::CallbackInfo &info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "dims_"));
}

/**
 * @readonly
 * @kind member
 * @name attributes
 * @instance
 * @memberof MDArray
 * @type {ArrayAttributes}
 */
Napi::Value MDArray::attributesGetter(const Napi::CallbackInfo &info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "attrs_"));
}

/**
 * @readonly
 * @kind member
 * @name description
 * @instance
 * @memberof MDArray
 * @type {string}
 */
Napi::Value MDArray::descriptionGetter(const Napi::CallbackInfo &info) {
  NODE_UNWRAP_CHECK(MDArray, info.This(), array);
  GDAL_RAW_CHECK(std::shared_ptr<GDALMDArray>, array, raw);
  GDAL_LOCK_PARENT(array);
  std::string description = raw->GetFullName();
  return SafeString::New(description.c_str());
}

/**
 * The flattened length of the array.
 *
 * @readonly
 * @kind member
 * @name length
 * @instance
 * @memberof MDArray
 * @type {number}
 */
NODE_WRAPPED_GETTER_WITH_RESULT_LOCKED(MDArray, lengthGetter, Number, GetTotalElementsCount);

Napi::Value MDArray::uidGetter(const Napi::CallbackInfo &info) {
  MDArray *ds = info.This().Unwrap<MDArray>();
  return Napi::New(env, (int)ds->uid);
}

#endif

} // namespace node_gdal
