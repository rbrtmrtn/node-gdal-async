#include "gdal_dimension.hpp"
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

Napi::FunctionReference Dimension::constructor;

void Dimension::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Dimension::New);

  lcons->SetClassName(Napi::String::New(env, "Dimension"));

  InstanceMethod("toString", &toString),

  ATTR_DONT_ENUM(lcons, "_uid", uidGetter, READ_ONLY_SETTER);
  ATTR(lcons, "size", sizeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "description", descriptionGetter, READ_ONLY_SETTER);
  ATTR(lcons, "type", typeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "direction", directionGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Dimension"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Dimension::Dimension(std::shared_ptr<GDALDimension> dimension) : Napi::ObjectWrap<Dimension>(), uid(0), this_(dimension), parent_ds(0) {
  LOG("Created dimension [%p]", dimension.get());
}

Dimension::Dimension() : Napi::ObjectWrap<Dimension>(), uid(0), this_(0), parent_ds(0) {
}

Dimension::~Dimension() {
  dispose();
}

void Dimension::dispose() {
  if (this_) {

    LOG("Disposing dimension [%p]", this_.get());

    object_store.dispose(uid);

    LOG("Disposed dimension [%p]", this_.get());
  }
};

/**
 * A representation of a group with access methods.
 *
 * @class Dimension
 */
Napi::Value Dimension::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1 && info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    Dimension *f = static_cast<Dimension *>(ptr);
    f->Wrap(info.This());

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create dimension directly. Create with dataset instead.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return info.This();
}

Napi::Value Dimension::New(std::shared_ptr<GDALDimension> raw, GDALDataset *parent_ds) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  Dimension *wrapped = new Dimension(raw);

  Napi::Object ds;
  if (object_store.has(parent_ds)) {
    ds = object_store.get(parent_ds);
  } else {
    LOG("Dimension's parent dataset disappeared from cache (array = %p, dataset = %p)", raw.get(), parent_ds);
    Napi::Error::New(env, "Dimension's parent dataset disappeared from cache").ThrowAsJavaScriptException();

    return scope.Escape(env.Undefined());
  }

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Dimension::constructor)), 1, &ext);

  Dataset *unwrapped_ds = ds.Unwrap<Dataset>();
  long parent_uid = unwrapped_ds->uid;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), parent_uid);
  wrapped->parent_ds = parent_ds;
  wrapped->parent_uid = parent_uid;

  Napi::SetPrivate(obj, Napi::String::New(env, "ds_"), ds);

  return scope.Escape(obj);
}

Napi::Value Dimension::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "Dimension");
}

/**
 * @readonly
 * @kind member
 * @name size
 * @instance
 * @memberof Dimension
 * @type {number}
 */
NODE_WRAPPED_GETTER_WITH_RESULT_LOCKED(Dimension, sizeGetter, Number, GetSize);

/**
 * @readonly
 * @kind member
 * @name description
 * @instance
 * @memberof Dimension
 * @type {string}
 */
NODE_WRAPPED_GETTER_WITH_STRING_LOCKED(Dimension, descriptionGetter, GetFullName);

/**
 * @readonly
 * @kind member
 * @name direction
 * @instance
 * @memberof Dimension
 * @type {string}
 */
NODE_WRAPPED_GETTER_WITH_STRING_LOCKED(Dimension, directionGetter, GetDirection);

/**
 * @readonly
 * @kind member
 * @name type
 * @instance
 * @memberof Dimension
 * @type {string}
 */
NODE_WRAPPED_GETTER_WITH_STRING_LOCKED(Dimension, typeGetter, GetType);

Napi::Value Dimension::uidGetter(const Napi::CallbackInfo& info) {
  Dimension *group = info.This().Unwrap<Dimension>();
  return Napi::New(env, (int)group->uid);
}

#endif

} // namespace node_gdal
