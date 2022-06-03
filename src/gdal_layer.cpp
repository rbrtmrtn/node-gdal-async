
#include "gdal_layer.hpp"
#include "collections/layer_features.hpp"
#include "collections/layer_fields.hpp"
#include "gdal_common.hpp"
#include "gdal_dataset.hpp"
#include "gdal_feature.hpp"
#include "gdal_feature_defn.hpp"
#include "gdal_field_defn.hpp"
#include "geometry/gdal_geometry.hpp"
#include "gdal_spatial_reference.hpp"

#include <sstream>
#include <stdlib.h>

namespace node_gdal {

Napi::FunctionReference Layer::constructor;

void Layer::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, Layer::New);

  lcons->SetClassName(Napi::String::New(env, "Layer"));

  InstanceMethod("toString", &toString),
  InstanceMethod("getExtent", &getExtent),
  InstanceMethod("setAttributeFilter", &setAttributeFilter),
  InstanceMethod("setSpatialFilter", &setSpatialFilter),
  InstanceMethod("getSpatialFilter", &getSpatialFilter),
  InstanceMethod("testCapability", &testCapability),
  Nan__SetPrototypeAsyncableMethod(lcons, "flush", syncToDisk);

  ATTR_DONT_ENUM(lcons, "ds", dsGetter, READ_ONLY_SETTER);
  ATTR_DONT_ENUM(lcons, "_uid", uidGetter, READ_ONLY_SETTER);
  ATTR(lcons, "srs", srsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "features", featuresGetter, READ_ONLY_SETTER);
  ATTR(lcons, "fields", fieldsGetter, READ_ONLY_SETTER);
  ATTR(lcons, "name", nameGetter, READ_ONLY_SETTER);
  ATTR(lcons, "geomType", geomTypeGetter, READ_ONLY_SETTER);
  ATTR(lcons, "geomColumn", geomColumnGetter, READ_ONLY_SETTER);
  ATTR(lcons, "fidColumn", fidColumnGetter, READ_ONLY_SETTER);

  (target).Set(Napi::String::New(env, "Layer"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

Layer::Layer(OGRLayer *layer) : Napi::ObjectWrap<Layer>(), uid(0), this_(layer), parent_ds(0) {
  LOG("Created layer [%p]", layer);
}

Layer::Layer() : Napi::ObjectWrap<Layer>(), uid(0), this_(0), parent_ds(0) {
}

Layer::~Layer() {
  dispose();
}

void Layer::dispose() {
  if (this_) {

    LOG("Disposing layer [%p]", this_);

    object_store.dispose(uid);

    LOG("Disposed layer [%p]", this_);
    this_ = NULL;
  }
};

/**
 * A representation of a layer of simple vector features, with access methods.
 *
 * @class Layer
 */
Napi::Value Layer::New(const Napi::CallbackInfo& info) {

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    Layer *f = static_cast<Layer *>(ptr);
    f->Wrap(info.This());

    Napi::Value features = LayerFeatures::New(info.This());
    Napi::SetPrivate(info.This(), Napi::String::New(env, "features_"), features);

    Napi::Value fields = LayerFields::New(info.This());
    Napi::SetPrivate(info.This(), Napi::String::New(env, "fields_"), fields);

    return info.This();
    return;
  } else {
    Napi::Error::New(env, "Cannot create layer directly. Create with dataset instead.").ThrowAsJavaScriptException();
    return env.Null();
  }

  return info.This();
}

Napi::Value Layer::New(OGRLayer *raw, GDALDataset *raw_parent) {
  Napi::EscapableHandleScope scope(env);
  return scope.Escape(Layer::New(raw, raw_parent, false));
}

Napi::Value Layer::New(OGRLayer *raw, GDALDataset *raw_parent, bool result_set) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  Layer *wrapped = new Layer(raw);

  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, Layer::constructor)), 1, &ext);

  // add reference to datasource so datasource doesnt get GC'ed while layer is
  // alive
  Napi::Object ds;
  if (object_store.has(raw_parent)) {
    ds = object_store.get(raw_parent);
  } else {
    LOG("Layer's parent dataset disappeared from cache (layer = %p, dataset = %p)", raw, raw_parent);
    Napi::Error::New(env, "Layer's parent dataset disappeared from cache").ThrowAsJavaScriptException();

    return scope.Escape(env.Undefined());
    // ds = Dataset::New(raw_parent); //should never happen
  }

  Dataset *unwrapped = ds.Unwrap<Dataset>();
  long parent_uid = unwrapped->uid;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), parent_uid, result_set);
  wrapped->parent_ds = raw_parent;
  wrapped->parent_uid = parent_uid;
  Napi::SetPrivate(obj, Napi::String::New(env, "ds_"), ds);

  return scope.Escape(obj);
}

Napi::Value Layer::toString(const Napi::CallbackInfo& info) {

  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->this_) {
    return Napi::String::New(env, "Null layer");
    return;
  }

  std::ostringstream ss;
  GDAL_LOCK_PARENT(layer);
  ss << "Layer (" << layer->this_->GetName() << ")";

  return SafeString::New(ss.str().c_str());
}

/**
 * Flush pending changes to disk.
 *
 * @throws {Error}
 * @method flush
 * @instance
 * @memberof Layer
 */

/**
 * Flush pending changes to disk.
 * @async
 *
 * @throws {Error}
 * @method flushAsync
 * @instance
 * @memberof Layer
 * @param {callback<void>} [callback=undefined]
 * @return {Promise<void>}
 *
 */
NODE_WRAPPED_ASYNC_METHOD_WITH_OGRERR_RESULT_LOCKED(Layer, syncToDisk, SyncToDisk);

/**
 * Determines if the dataset supports the indicated operation.
 *
 * @method testCapability
 * @instance
 * @memberof Layer
 * @param {string} capability (see {@link OLC|capability list}
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT_1_STRING_PARAM_LOCKED(Layer, testCapability, Boolean, TestCapability, "capability");

/**
 * Fetch the extent of this layer.
 *
 * @throws {Error}
 * @method getExtent
 * @instance
 * @memberof Layer
 * @param {boolean} [force=true]
 * @return {Envelope} Bounding envelope
 */
Napi::Value Layer::getExtent(const Napi::CallbackInfo& info) {

  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  int force = 1;
  NODE_ARG_BOOL_OPT(0, "force", force);

  std::unique_ptr<OGREnvelope> envelope(new OGREnvelope());
  GDAL_LOCK_PARENT(layer);
  OGRErr err = layer->this_->GetExtent(envelope.get(), force);

  if (err) {
    Napi::Error::New(env, "Can't get layer extent without computing it").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Object obj = Napi::Object::New(env);
  (obj).Set(Napi::String::New(env, "minX"), Napi::Number::New(env, envelope->MinX));
  (obj).Set(Napi::String::New(env, "maxX"), Napi::Number::New(env, envelope->MaxX));
  (obj).Set(Napi::String::New(env, "minY"), Napi::Number::New(env, envelope->MinY));
  (obj).Set(Napi::String::New(env, "maxY"), Napi::Number::New(env, envelope->MaxY));

  return obj;
}

/**
 * This method returns the current spatial filter for this layer.
 *
 * @throws {Error}
 * @method getSpatialFilter
 * @instance
 * @memberof Layer
 * @return {Geometry}
 */
Napi::Value Layer::getSpatialFilter(const Napi::CallbackInfo& info) {

  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetSpatialFilter();

  return Geometry::New(r, false);
}

/**
 * This method sets the geometry to be used as a spatial filter when fetching
 * features via the `layer.features.next()` method. Only features that
 * geometrically intersect the filter geometry will be returned.
 *
 * Alernatively you can pass it envelope bounds as individual arguments.
 *
 * @example
 *
 * layer.setSpatialFilter(geometry);
 *
 * @throws {Error}
 * @method setSpatialFilter
 * @instance
 * @memberof Layer
 * @param {Geometry|null} filter
 */

/**
 * This method sets the geometry to be used as a spatial filter when fetching
 * features via the `layer.features.next()` method. Only features that
 * geometrically intersect the filter geometry will be returned.
 *
 * Alernatively you can pass it envelope bounds as individual arguments.
 *
 * @example
 *
 * layer.setSpatialFilter(minX, minY, maxX, maxY);
 *
 * @throws {Error}
 * @method setSpatialFilter
 * @instance
 * @memberof Layer
 * @param {number} minxX
 * @param {number} minyY
 * @param {number} maxX
 * @param {number} maxY
 */
Napi::Value Layer::setSpatialFilter(const Napi::CallbackInfo& info) {

  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() == 1) {
    Geometry *filter = NULL;
    NODE_ARG_WRAPPED_OPT(0, "filter", Geometry, filter);

    GDAL_LOCK_PARENT(layer);
    if (filter) {
      layer->this_->SetSpatialFilter(filter->get());
    } else {
      layer->this_->SetSpatialFilter(NULL);
    }
  } else if (info.Length() == 4) {
    double minX, minY, maxX, maxY;
    NODE_ARG_DOUBLE(0, "minX", minX);
    NODE_ARG_DOUBLE(1, "minY", minY);
    NODE_ARG_DOUBLE(2, "maxX", maxX);
    NODE_ARG_DOUBLE(3, "maxY", maxY);

    GDAL_LOCK_PARENT(layer);
    layer->this_->SetSpatialFilterRect(minX, minY, maxX, maxY);
  } else {
    Napi::Error::New(env, "Invalid number of arguments").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

/**
 * Sets the attribute query string to be used when fetching features via the
 * `layer.features.next()` method. Only features for which the query evaluates
 * as `true` will be returned.
 *
 * The query string should be in the format of an SQL WHERE clause. For instance
 * "population > 1000000 and population < 5000000" where `population` is an
 * attribute in the layer. The query format is normally a restricted form of
 * SQL WHERE clause as described in the "WHERE" section of the [OGR SQL
 * tutorial](https://gdal.org/user/ogr_sql_dialect.html). In some cases (RDBMS backed
 * drivers) the native capabilities of the database may be used to interprete
 * the WHERE clause in which case the capabilities will be broader than those
 * of OGR SQL.
 *
 * @example
 *
 * layer.setAttributeFilter('population > 1000000 and population < 5000000');
 *
 * @throws {Error}
 * @method setAttributeFilter
 * @instance
 * @memberof Layer
 * @param {string|null} [filter=null]
 */
Napi::Value Layer::setAttributeFilter(const Napi::CallbackInfo& info) {

  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::string filter = "";
  NODE_ARG_OPT_STR(0, "filter", filter);

  OGRErr err;
  GDAL_LOCK_PARENT(layer);
  if (filter.empty()) {
    err = layer->this_->SetAttributeFilter(NULL);
  } else {
    err = layer->this_->SetAttributeFilter(filter.c_str());
  }

  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }

  return;
}

/*
Napi::Value Layer::getLayerDefn(const Napi::CallbackInfo& info)
{
  Layer *layer = info.This().Unwrap<Layer>();

  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object already destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }

  return FeatureDefn::New(layer->this_->GetLayerDefn(),
false);
}*/

/**
 * @readonly
 * @kind member
 * @name ds
 * @instance
 * @memberof Layer
 * @type {Dataset}
 */
Napi::Value Layer::dsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "ds_"));
}

/**
 * @readonly
 * @kind member
 * @name srs
 * @instance
 * @memberof Layer
 * @type {SpatialReference}
 */
Napi::Value Layer::srsGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetSpatialRef();
  return SpatialReference::New(r, false);
}

/**
 * @readonly
 * @kind member
 * @name name
 * @instance
 * @memberof Layer
 * @type {string}
 */
Napi::Value Layer::nameGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetName();
  return SafeString::New(r);
}

/**
 * @readonly
 * @kind member
 * @name geomColumn
 * @instance
 * @memberof Layer
 * @type {string}
 */
Napi::Value Layer::geomColumnGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetGeometryColumn();
  return SafeString::New(r);
}

/**
 * @readonly
 * @kind member
 * @name fidColumn
 * @instance
 * @memberof Layer
 * @type {string}
 */
Napi::Value Layer::fidColumnGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetFIDColumn();
  return SafeString::New(r);
}

/**
 * @readonly
 * @kind member
 * @name geomType
 * @instance
 * @memberof Layer
 * @type {number} (see {@link wkbGeometry|geometry types}
 */
Napi::Value Layer::geomTypeGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  if (!layer->isAlive()) {
    Napi::Error::New(env, "Layer object has already been destroyed").ThrowAsJavaScriptException();
    return env.Null();
  }
  GDAL_LOCK_PARENT(layer);
  auto r = layer->this_->GetGeomType();
  return Napi::Number::New(env, r);
}

/**
 * @readonly
 * @kind member
 * @name features
 * @instance
 * @memberof Layer
 * @type {LayerFeatures}
 */
Napi::Value Layer::featuresGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "features_"));
}

/**
 * @readonly
 * @kind member
 * @name fields
 * @instance
 * @memberof Layer
 * @type {LayerFields}
 */
Napi::Value Layer::fieldsGetter(const Napi::CallbackInfo& info) {
  return Napi::GetPrivate(info.This(), Napi::String::New(env, "fields_"));
}

Napi::Value Layer::uidGetter(const Napi::CallbackInfo& info) {
  Layer *layer = info.This().Unwrap<Layer>();
  return Napi::New(env, (int)layer->uid);
}

} // namespace node_gdal
