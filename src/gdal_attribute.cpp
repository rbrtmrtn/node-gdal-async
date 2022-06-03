#include "gdal_attribute.hpp"
#include "gdal_common.hpp"
#include "gdal_driver.hpp"
#include "gdal_group.hpp"
#include "collections/group_groups.hpp"
#include "collections/group_arrays.hpp"
#include "geometry/gdal_geometry.hpp"
#include "gdal_layer.hpp"
#include "gdal_majorobject.hpp"
#include "gdal_rasterband.hpp"
#include "gdal_spatial_reference.hpp"

namespace node_gdal {

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)

Napi::FunctionReference Attribute::constructor;

void Attribute::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Attribute::New);

  lcons->SetClassName(Napi::String::New(env, "Attribute"));

  InstanceMethod("toString", &toString),

  ATTR_DONT_ENUM(lcons, "_uid", uidGetter, READ_ONLY_SETTER);
  ATTR(lcons, "dataType", typeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "value", valueGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Attribute"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Attribute::Attribute(std::shared_ptr<GDALAttribute> attribute) : Napi::ObjectWrap<Attribute>(), uid(0), this_(attribute), parent_ds(0) {
  LOG("Created attribute [%p]", attribute.get());
}

Attribute::Attribute() : Napi::ObjectWrap<Attribute>(), uid(0), this_(0), parent_ds(0) {
}

Attribute::~Attribute() {
  dispose();
}

void Attribute::dispose() {
  if (this_) {

    LOG("Disposing attribute [%p]", this_.get());

    object_store.dispose(uid);

    LOG("Disposed attribute [%p]", this_.get());
  }
};

/**
 * A representation of a group with access methods.
 *
 * @class Attribute
 */
Napi::Value Attribute::New(const Napi::CallbackInfo& info) {
  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 && info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    Attribute *f = static_cast<Attribute *>(ptr);
    f->Wrap(info.This());

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create attribute directly. Create with dataset instead.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return info.This();
}

Napi::Value Attribute::New(std::shared_ptr<GDALAttribute> raw, GDALDataset *parent_ds) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  Attribute *wrapped = new Attribute(raw);

  Napi::Object ds;
  if (object_store.has(parent_ds)) {
    ds = object_store.get(parent_ds);
  } else {
    LOG("Attribute's parent dataset disappeared from cache (array = %p, dataset = %p)", raw.get(), parent_ds);
    Napi::Error::New(env, "Attribute's parent dataset disappeared from cache").ThrowAsJavaScriptException();

    return scope.Escape(env.Undefined());
  }

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Attribute::constructor)), 1, &ext);

  Dataset *unwrapped_ds = ds.Unwrap<Dataset>();
  long parent_uid = unwrapped_ds->uid;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), parent_uid);
  wrapped->parent_ds = parent_ds;
  wrapped->parent_uid = parent_uid;

  Napi::SetPrivate(obj, Napi::String::New(env, "ds_"), ds);

  return scope.Escape(obj);
}

Napi::Value Attribute::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Attribute");
}

/**
 * Complex GDAL data types introduced in 3.1 are not yet supported
 * @readonly
 * @kind member
 * @name value
 * @instance
 * @memberof Attribute
 * @throws {Error}
 * @type {string|number}
 */
Napi::Value Attribute::valueGetter(const Napi::CallbackInfo& info) {
  NODE_UNWRAP_CHECK(Attribute, info.This(), attribute);
  GDAL_RAW_CHECK(std::shared_ptr<GDALAttribute>, attribute, raw);
  GDAL_LOCK_PARENT(attribute);
  GDALExtendedDataType type = raw->GetDataType();
  Napi::Value r;
  switch (type.GetClass()) {
    case GEDTC_NUMERIC: r = Napi::Number::New(env, raw->ReadAsDouble()); break;
    case GEDTC_STRING: r = SafeString::New(raw->ReadAsString()); break;
    default: Napi::Error::New(env, "Compound attributes are not supported yet").ThrowAsJavaScriptException();
 return;
  }

  return r;
}

/**
 * @readonly
 * @kind member
 * @name dataType
 * @instance
 * @memberof Attribute
 * @type {string}
 */
Napi::Value Attribute::typeGetter(const Napi::CallbackInfo& info) {
  NODE_UNWRAP_CHECK(Attribute, info.This(), attribute);
  GDAL_RAW_CHECK(std::shared_ptr<GDALAttribute>, attribute, raw);
  GDAL_LOCK_PARENT(attribute);
  GDALExtendedDataType type = raw->GetDataType();
  const char *r;
  switch (type.GetClass()) {
    case GEDTC_NUMERIC: r = GDALGetDataTypeName(type.GetNumericDataType()); break;
    case GEDTC_STRING: r = "String"; break;
    case GEDTC_COMPOUND: r = "Compound"; break;
    default: Napi::Error::New(env, "Invalid attribute type").ThrowAsJavaScriptException();
 return;
  }

  return SafeString::New(r);
}

Napi::Value Attribute::uidGetter(const Napi::CallbackInfo& info) {
  Attribute *group = info.This().Unwrap<Attribute>();
  return Napi::New(env, (int)group->uid);
}

#endif

} // namespace node_gdal
