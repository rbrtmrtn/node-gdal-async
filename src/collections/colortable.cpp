#include <memory>
#include "colortable.hpp"
#include "../gdal_common.hpp"
#include "../gdal_dataset.hpp"
#include "../gdal_rasterband.hpp"
#include "../utils/string_list.hpp"

namespace node_gdal {

Napi::FunctionReference ColorTable::constructor;

void ColorTable::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, ColorTable::New);

  lcons->SetClassName(Napi::String::New(env, "ColorTable"));

  InstanceMethod("toString", &toString),
  InstanceMethod("isSame", &isSame),
  InstanceMethod("clone", &clone),
  InstanceMethod("count", &count),
  InstanceMethod("get", &get),
  InstanceMethod("set", &set),
  InstanceMethod("ramp", &ramp),
  ATTR(lcons, "interpretation", interpretationGetter, READ_ONLY_SETTER);

  ATTR_DONT_ENUM(lcons, "band", bandGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "ColorTable"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

ColorTable::ColorTable(GDALColorTable *raw, long parent_uid) : Napi::ObjectWrap<ColorTable>(), parent_uid(parent_uid), this_(raw) {
}

ColorTable::~ColorTable() {
  dispose();
}

void ColorTable::dispose() {
  if (this_) {
    LOG("Disposing ColorTable [%p]", this_);
    object_store.dispose(uid, false);
    if (parent_uid == 0 && this_ != nullptr) delete this_;
    this_ = nullptr;
  }
}
/**
 * An encapsulation of a {@link RasterBand}
 * color table.
 *
 * @example
 * var colorTable = band.colorTable;
 *
 * band.colorTable = new gdal.ColorTable(gdal.GPI_RGB);
 *
 * @class ColorTable
 * @param {string} interpretation palette interpretation
 */
Napi::Value ColorTable::New(const Napi::CallbackInfo& info) {
  ColorTable *f;

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<ColorTable *>(ptr);
  } else {
    std::string pi;
    NODE_ARG_STR(0, "palette interpretation", pi);
    GDALPaletteInterp gpi;
    if (pi == "Gray")
      gpi = GPI_Gray;
    else if (pi == "RGB")
      gpi = GPI_RGB;
    else if (pi == "CMYK")
      gpi = GPI_CMYK;
    else if (pi == "HLS")
      gpi = GPI_HLS;
    else {
      Napi::RangeError::New(env, "Invalid palette interpretation").ThrowAsJavaScriptException();
      return env.Null();
    }
    f = new ColorTable(new GDALColorTable(gpi), 0);
  }

  f->Wrap(info.This());
  f->uid = object_store.add(f->get(), f->persistent(), f->parent_uid);
  return info.This();
  return;
}

/*
 * Create a color table owned by a gdal.RasterBand
 */
Napi::Value ColorTable::New(GDALColorTable *raw, Napi::Value parent) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  RasterBand *band = parent.As<Napi::Object>().Unwrap<RasterBand>();

  ColorTable *wrapped = new ColorTable(raw, band->parent_uid);

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, ColorTable::constructor)), 1, &ext);

  Napi::SetPrivate(obj, Napi::String::New(env, "parent_"), parent);

  return scope.Escape(obj);
}

/*
 * Create a standalone color table.
 */
Napi::Value ColorTable::New(GDALColorTable *raw) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  ColorTable *wrapped = new ColorTable(raw, 0);

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, ColorTable::constructor)), 1, &ext);

  return scope.Escape(obj);
}

Napi::Value ColorTable::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "ColorTable");
}

/**
 * @typedef {object} Color
 * @memberof ColorTable
 * @property {number} c1
 * @property {number} c2
 * @property {number} c3
 * @property {number} c4
 */

/**
 * Clones the instance.
 * The newly created ColorTable is not owned by any RasterBand.
 *
 * @method clone
 * @instance
 * @memberof ColorTable
 * @return {ColorTable}
 */
Napi::Value ColorTable::clone(const Napi::CallbackInfo& info) {
  ColorTable *ct = info.This().Unwrap<ColorTable>();
  return ColorTable::New(ct->this_->Clone());
}

/**
 * Compares two ColorTable objects for equality.
 *
 * @method isSame
 * @instance
 * @memberof ColorTable
 * @param {ColorTable} other
 * @throws {Error}
 * @return {boolean}
 */
Napi::Value ColorTable::isSame(const Napi::CallbackInfo& info) {

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  ColorTable *other;
  NODE_ARG_WRAPPED(0, "other", ColorTable, other);
  GDAL_RAW_CHECK(GDALColorTable *, other, raw_other);

  CPLErrorReset();
  return Napi::Boolean::New(env, raw->IsSame(raw_other));
}

/**
 * Returns the color with the given ID.
 *
 * @method get
 * @instance
 * @memberof ColorTable
 * @param {number} index
 * @throws {Error}
 * @return {Color}
 */
Napi::Value ColorTable::get(const Napi::CallbackInfo& info) {

  int index;
  NODE_ARG_INT(0, "index", index);

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    Napi::Object parent = parentMaybe.As<Napi::Object>();
    NODE_UNWRAP_CHECK(RasterBand, parent, band);
  }

  CPLErrorReset();
  const GDALColorEntry *color = raw->GetColorEntry(index);
  if (color == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  Napi::Object result = Napi::Object::New(env);
  (result).Set(Napi::String::New(env, "c1"), Napi::Number::New(env, color->c1));
  (result).Set(Napi::String::New(env, "c2"), Napi::Number::New(env, color->c2));
  (result).Set(Napi::String::New(env, "c3"), Napi::Number::New(env, color->c3));
  (result).Set(Napi::String::New(env, "c4"), Napi::Number::New(env, color->c4));
  return result;
}

#define NODE_COLOR_FROM_OBJ(obj, color)                                                                                \
  NODE_INT_FROM_OBJ(obj, "c1", color.c1);                                                                              \
  NODE_INT_FROM_OBJ(obj, "c2", color.c2);                                                                              \
  NODE_INT_FROM_OBJ(obj, "c3", color.c3);                                                                              \
  NODE_INT_FROM_OBJ(obj, "c4", color.c4);

/**
 * Sets the color entry with the given ID.
 *
 * @method set
 * @instance
 * @memberof ColorTable
 * @throws {Error}
 * @param {number} index
 * @param {Color} color
 * @return {void}
 */

Napi::Value ColorTable::set(const Napi::CallbackInfo& info) {

  int index;
  NODE_ARG_INT(0, "index", index);

  Napi::Object color_obj;
  NODE_ARG_OBJECT(1, "color", color_obj);

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    Napi::Error::New(env, "RasterBand color tables are read-only, create a new one to modify it").ThrowAsJavaScriptException();
    return env.Null();
  }

  GDALColorEntry color;
  NODE_COLOR_FROM_OBJ(color_obj, color);

  CPLErrorReset();
  raw->SetColorEntry(index, &color);
}

/**
 * Creates a color ramp from one color entry to another.
 *
 * @method ramp
 * @instance
 * @memberof ColorTable
 * @throws {Error}
 * @param {number} start_index
 * @param {Color} start_color
 * @param {number} end_index
 * @param {Color} end_color
 * @return {number} total number of color entries
 */

Napi::Value ColorTable::ramp(const Napi::CallbackInfo& info) {

  int start_index, end_index;
  NODE_ARG_INT(0, "start_index", start_index);
  NODE_ARG_INT(2, "end_index", end_index);
  if (start_index < 0 || end_index < 0 || end_index < start_index) {
    Napi::RangeError::New(env, "Invalid color interval").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object start_color_obj, end_color_obj;
  NODE_ARG_OBJECT(1, "start_color", start_color_obj);
  NODE_ARG_OBJECT(3, "start_color", end_color_obj);

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  GDALColorEntry start_color, end_color;
  NODE_COLOR_FROM_OBJ(start_color_obj, start_color);
  NODE_COLOR_FROM_OBJ(end_color_obj, end_color);

  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    Napi::Error::New(env, "RasterBand color tables are read-only, create a new one to modify it").ThrowAsJavaScriptException();
    return env.Null();
  }

  int r = raw->CreateColorRamp(start_index, &start_color, end_index, &end_color);
  if (r == -1) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  return Napi::Number::New(env, r);
}

/**
 * Returns the number of color entries.
 *
 * @method count
 * @instance
 * @memberof ColorTable
 * @return {number}
 */
Napi::Value ColorTable::count(const Napi::CallbackInfo& info) {

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    Napi::Object parent = parentMaybe.As<Napi::Object>();
    NODE_UNWRAP_CHECK(RasterBand, parent, band);
  }

  CPLErrorReset();
  return raw->GetColorEntryCount();
}

/**
 * Color interpretation of the palette.
 *
 * @readonly
 * @kind member
 * @name interpretation
 * @instance
 * @memberof ColorTable
 * @type {string}
 */
Napi::Value ColorTable::interpretationGetter(const Napi::CallbackInfo& info) {

  NODE_UNWRAP_CHECK(ColorTable, info.This(), self);
  GDAL_RAW_CHECK(GDALColorTable *, self, raw);

  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    Napi::Object parent = parentMaybe.As<Napi::Object>();
    NODE_UNWRAP_CHECK(RasterBand, parent, band);
  }

  CPLErrorReset();
  GDALPaletteInterp interp = raw->GetPaletteInterpretation();
  std::string r;
  switch (interp) {
    case GPI_Gray: r = "Gray"; break;
    case GPI_RGB: r = "RGB"; break;
    case GPI_CMYK: r = "CMYK"; break;
    case GPI_HLS: r = "HLS"; break;
    default: r = "invalid"; break;
  }
  return SafeString::New(r.c_str());
}

/**
 * Returns the parent band.
 *
 * @readonly
 * @kind member
 * @name band
 * @instance
 * @memberof ColorTable
 * @type {RasterBand|undefined}
 */
Napi::Value ColorTable::bandGetter(const Napi::CallbackInfo& info) {
  MaybeNapi::Value parentMaybe = Napi::GetPrivate(info.This(), Napi::String::New(env, "parent_"));
  if (!parentMaybe.IsEmpty() && !parentMaybe->IsNullOrUndefined()) {
    return parentMaybe;
  }
}

} // namespace node_gdal
