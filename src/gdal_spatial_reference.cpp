
#include "gdal_spatial_reference.hpp"
#include "gdal_common.hpp"
#include "utils/string_list.hpp"
#include "async.hpp"

namespace node_gdal {

Napi::FunctionReference SpatialReference::constructor;

void SpatialReference::Initialize(Napi::Object target) {
  Napi::HandleScope scope(env);

  Napi::FunctionReference lcons = Napi::Function::New(env, SpatialReference::New);

  lcons->SetClassName(Napi::String::New(env, "SpatialReference"));

  Nan__SetAsyncableMethod(lcons, "fromUserInput", fromUserInput);
  Napi::SetMethod(lcons, "fromWKT", fromWKT);
  Napi::SetMethod(lcons, "fromProj4", fromProj4);
  Napi::SetMethod(lcons, "fromEPSG", fromEPSG);
  Napi::SetMethod(lcons, "fromEPSGA", fromEPSGA);
  Napi::SetMethod(lcons, "fromESRI", fromESRI);
  Napi::SetMethod(lcons, "fromWMSAUTO", fromWMSAUTO);
  Napi::SetMethod(lcons, "fromXML", fromXML);
  Napi::SetMethod(lcons, "fromURN", fromURN);
  Nan__SetAsyncableMethod(lcons, "fromCRSURL", fromCRSURL);
  Nan__SetAsyncableMethod(lcons, "fromURL", fromURL);
  Napi::SetMethod(lcons, "fromMICoordSys", fromMICoordSys);

  InstanceMethod("toString", &toString),
  InstanceMethod("toWKT", &exportToWKT),
  InstanceMethod("toPrettyWKT", &exportToPrettyWKT),
  InstanceMethod("toProj4", &exportToProj4),
  InstanceMethod("toXML", &exportToXML),

  InstanceMethod("clone", &clone),
  InstanceMethod("cloneGeogCS", &cloneGeogCS),
  InstanceMethod("setWellKnownGeogCS", &setWellKnownGeogCS),
  InstanceMethod("morphToESRI", &morphToESRI),
  InstanceMethod("morphFromESRI", &morphFromESRI),
  InstanceMethod("EPSGTreatsAsLatLong", &EPSGTreatsAsLatLong),
  InstanceMethod("EPSGTreatsAsNorthingEasting", &EPSGTreatsAsNorthingEasting),
  InstanceMethod("getLinearUnits", &getLinearUnits),
  InstanceMethod("getAngularUnits", &getAngularUnits),
  InstanceMethod("isGeographic", &isGeographic),
  InstanceMethod("isGeocentric", &isGeocentric),
  InstanceMethod("isProjected", &isProjected),
  InstanceMethod("isLocal", &isLocal),
  InstanceMethod("isVectical", &isVertical),
  InstanceMethod("isVertical", &isVertical),
  InstanceMethod("isCompound", &isCompound),
  InstanceMethod("isSameGeogCS", &isSameGeogCS),
  InstanceMethod("isSameVertCS", &isSameVertCS),
  InstanceMethod("isSame", &isSame),
  InstanceMethod("getAuthorityName", &getAuthorityName),
  InstanceMethod("getAuthorityCode", &getAuthorityCode),
  InstanceMethod("getAttrValue", &getAttrValue),
  InstanceMethod("autoIdentifyEPSG", &autoIdentifyEPSG),
  InstanceMethod("validate", &validate),

  (target).Set(Napi::String::New(env, "SpatialReference"), Napi::GetFunction(lcons));

  constructor.Reset(lcons);
}

SpatialReference::SpatialReference(OGRSpatialReference *srs) : Napi::ObjectWrap<SpatialReference>(), this_(srs), owned_(false) {
  LOG("Created SpatialReference [%p]", srs);
}

SpatialReference::SpatialReference() : Napi::ObjectWrap<SpatialReference>(), this_(0), owned_(false) {
}

SpatialReference::~SpatialReference() {
  dispose();
}

void SpatialReference::dispose() {
  if (this_) {
    LOG("Disposing SpatialReference [%p] (%s)", this_, owned_ ? "owned" : "unowned");
    object_store.dispose(uid);
    if (owned_) {
      // Decrements the reference count by one, and destroy if zero.
      this_->Release();
    }
    LOG("Disposed SpatialReference [%p]", this_);
    this_ = NULL;
  }
}

/**
 * This class respresents a OpenGIS Spatial Reference System, and contains
 * methods for converting between this object organization and well known text
 * (WKT) format.
 *
 * @constructor
 * @class SpatialReference
 * @param {string} [wkt]
 */
Napi::Value SpatialReference::New(const Napi::CallbackInfo& info) {
  SpatialReference *f;
  OGRSpatialReference *srs;
  std::string wkt("");

  if (!info.IsConstructCall()) {
    Napi::Error::New(env, "Cannot call constructor as function, you need to use 'new' keyword").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info[0].IsExternal()) {
    Napi::External ext = info[0].As<Napi::External>();
    void *ptr = ext->Value();
    f = static_cast<SpatialReference *>(ptr);
    f->Wrap(info.This());
  } else {
    NODE_ARG_OPT_STR(0, "wkt", wkt);
    // sets reference count to one
    srs = new OGRSpatialReference(wkt.empty() ? 0 : wkt.c_str());
    if (!wkt.empty()) {
      OGRChar *wkt_c = (OGRChar *)wkt.c_str();
      int err = srs->importFromWkt(&wkt_c);
      if (err) {
        delete srs;
        NODE_THROW_OGRERR(err);
        return;
      }
    }
    f = new SpatialReference(srs);
    f->owned_ = true;
    f->Wrap(info.This());

    f->uid = object_store.add(srs, f->persistent(), 0);
  }

  return info.This();
}

Napi::Value SpatialReference::New(OGRSpatialReference *srs) {
  Napi::EscapableHandleScope scope(env);
  return scope.Escape(SpatialReference::New(srs, false));
}

Napi::Value SpatialReference::New(OGRSpatialReference *raw, bool owned) {
  Napi::EscapableHandleScope scope(env);

  if (!raw) { return scope.Escape(env.Null()); }
  if (object_store.has(raw)) { return scope.Escape(object_store.get(raw)); }

  // make a copy of spatialreference owned by a layer, feature, etc
  // + no need to track when a layer is destroyed
  // + no need to throw errors when a method tries to modify an owned read-only srs
  // - is slower

  // Fixing this for srs obtained from a Layer is trivial
  // But fixing it for srs obtained from a Feature required moving the Features to the ObjectStore

  OGRSpatialReference *cloned_srs = raw;
  if (!owned) cloned_srs = raw->Clone();

  SpatialReference *wrapped = new SpatialReference(cloned_srs);
  wrapped->owned_ = true;
  Napi::Value ext = Napi::External::New(env, wrapped);
  Napi::Object obj =
    Napi::NewInstance(Napi::GetFunction(Napi::New(env, SpatialReference::constructor)), 1, &ext)
      ;

  wrapped->uid = object_store.add(raw, wrapped->persistent(), 0);

  return scope.Escape(obj);
}

Napi::Value SpatialReference::toString(const Napi::CallbackInfo& info) {
  return Napi::String::New(env, "SpatialReference");
}

/**
 * Set a GeogCS based on well known name.
 *
 * @method setWellKnownGeogCS
 * @instance
 * @memberof SpatialReference
 * @param {string} name
 */
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT_1_STRING_PARAM(
  SpatialReference, setWellKnownGeogCS, SetWellKnownGeogCS, "input");

/**
 * Convert in place to ESRI WKT format.
 *
 * @throws {Error}
 * @method morphToESRI
 * @instance
 * @memberof SpatialReference
 */
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT(SpatialReference, morphToESRI, morphToESRI);

/**
 * Convert in place from ESRI WKT format.
 *
 * @throws {Error}
 * @method morphFromESRI
 * @instance
 * @memberof SpatialReference
 */
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT(SpatialReference, morphFromESRI, morphFromESRI);

/**
 * This method returns `true` if EPSG feels this geographic coordinate system
 * should be treated as having lat/long coordinate ordering.
 *
 * Currently this returns `true` for all geographic coordinate systems with an
 * EPSG code set, and AXIS values set defining it as lat, long. Note that
 * coordinate systems with an EPSG code and no axis settings will be assumed
 * to not be lat/long.
 *
 * `false` will be returned for all coordinate systems that are not geographic,
 * or that do not have an EPSG code set.
 *
 * @method EPSGTreatsAsLatLong
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, EPSGTreatsAsLatLong, Boolean, EPSGTreatsAsLatLong);

/**
 * This method returns `true` if EPSG feels this projected coordinate system
 * should be treated as having northing/easting coordinate ordering.
 *
 * @method EPSGTreatsAsNorthingEasting
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, EPSGTreatsAsNorthingEasting, Boolean, EPSGTreatsAsNorthingEasting);

/**
 * Check if geocentric coordinate system.
 *
 * @method isGeocentric
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isGeocentric, Boolean, IsGeocentric);

/**
 * Check if geographic coordinate system.
 *
 * @method isGeographic
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isGeographic, Boolean, IsGeographic);

/**
 * Check if projected coordinate system.
 *
 * @method isProjected
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isProjected, Boolean, IsProjected);

/**
 * Check if local coordinate system.
 *
 * @method isLocal
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isLocal, Boolean, IsLocal);

/**
 * Check if vertical coordinate system.
 *
 * @method isVertical
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isVertical, Boolean, IsVertical);

/**
 * Check if compound coordinate system.
 *
 * @method isCompound
 * @instance
 * @memberof SpatialReference
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT(SpatialReference, isCompound, Boolean, IsCompound);

/**
 * Do the GeogCS'es match?
 *
 * @method isSameGeogCS
 * @instance
 * @memberof SpatialReference
 * @param {SpatialReference} srs
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  SpatialReference, isSameGeogCS, Boolean, IsSameGeogCS, SpatialReference, "srs");

/**
 * Do the VertCS'es match?
 *
 * @method isSameVertCS
 * @instance
 * @memberof SpatialReference
 * @param {SpatialReference} srs
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(
  SpatialReference, isSameVertCS, Boolean, IsSameVertCS, SpatialReference, "srs");

/**
 * Do these two spatial references describe the same system?
 *
 * @method isSame
 * @instance
 * @memberof SpatialReference
 * @param {SpatialReference} srs
 * @return {boolean}
 */
NODE_WRAPPED_METHOD_WITH_RESULT_1_WRAPPED_PARAM(SpatialReference, isSame, Boolean, IsSame, SpatialReference, "srs");

/**
 * Set EPSG authority info if possible.
 *
 * @throws {Error}
 * @method autoIdentifyEPSG
 * @instance
 * @memberof SpatialReference
 */
NODE_WRAPPED_METHOD_WITH_OGRERR_RESULT(SpatialReference, autoIdentifyEPSG, AutoIdentifyEPSG);

/**
 * Clones the spatial reference.
 *
 * @method clone
 * @instance
 * @memberof SpatialReference
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::clone(const Napi::CallbackInfo& info) {
  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  return SpatialReference::New(srs->this_->Clone(), true);
}

/**
 * Make a duplicate of the GEOGCS node of this OGRSpatialReference object.
 *
 * @method cloneGeogCS
 * @instance
 * @memberof SpatialReference
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::cloneGeogCS(const Napi::CallbackInfo& info) {
  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  return SpatialReference::New(srs->this_->CloneGeogCS(), true);
}

/**
 * Get the authority name for a node. The most common authority is "EPSG".
 *
 * @method getAuthorityName
 * @instance
 * @memberof SpatialReference
 * @param {string|null} [target_key] The partial or complete path to the node to get an authority from. ie. `"PROJCS"`, `"GEOGCS"`, "`GEOGCS|UNIT"` or `null` to search for an authority node on the root element.
 * @return {string}
 */
Napi::Value SpatialReference::getAuthorityName(const Napi::CallbackInfo& info) {

  std::string key = "";
  NODE_ARG_OPT_STR(0, "target key", key);

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();

  return SafeString::New(srs->this_->GetAuthorityName(key.Length() ? key.c_str() : NULL));
}

/**
 * Get the authority code for a node.
 *
 * @method getAuthorityCode
 * @instance
 * @memberof SpatialReference
 * @param {string|null} [target_key] The partial or complete path to the node to get an authority from. ie. `"PROJCS"`, `"GEOGCS"`, "`GEOGCS|UNIT"` or `null` to search for an authority node on the root element.
 * @return {string}
 */
Napi::Value SpatialReference::getAuthorityCode(const Napi::CallbackInfo& info) {

  std::string key = "";
  NODE_ARG_OPT_STR(0, "target key", key);

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();

  return SafeString::New(srs->this_->GetAuthorityCode(key.Length() ? key.c_str() : NULL));
}

/**
 * Convert this SRS into WKT format.
 *
 * @throws {Error}
 * @method toWKT
 * @instance
 * @memberof SpatialReference
 * @return {string}
 */
Napi::Value SpatialReference::exportToWKT(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  char *str;
  Napi::Value result;

  int err = srs->this_->exportToWkt(&str);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  result = SafeString::New(str);
  CPLFree(str);

  return result;
}

/**
 * Convert this SRS into a a nicely formatted WKT string for display to a
 * person.
 *
 * @throws {Error}
 * @method toPrettyWKT
 * @instance
 * @memberof SpatialReference
 * @param {boolean} [simplify=false]
 * @return {string}
 */
Napi::Value SpatialReference::exportToPrettyWKT(const Napi::CallbackInfo& info) {

  int simplify = 0;
  NODE_ARG_BOOL_OPT(0, "simplify", simplify);

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  char *str;
  Napi::Value result;

  int err = srs->this_->exportToPrettyWkt(&str, simplify);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  result = SafeString::New(str);
  CPLFree(str);

  return result;
}

/**
 * Export coordinate system in PROJ.4 format.
 *
 * @throws {Error}
 * @method toProj4
 * @instance
 * @memberof SpatialReference
 * @return {string}
 */
Napi::Value SpatialReference::exportToProj4(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  char *str;
  Napi::Value result;

  int err = srs->this_->exportToProj4(&str);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }

  if (str) {
    result = Napi::New(env, CPLString(str).Trim().c_str());
  } else {
    result = env.Null();
  }
  CPLFree(str);

  return result;
}

/**
 * Export coordinate system in XML format.
 *
 * @throws {Error}
 * @method toXML
 * @instance
 * @memberof SpatialReference
 * @return {string}
 */
Napi::Value SpatialReference::exportToXML(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  char *str;
  Napi::Value result;

  int err = srs->this_->exportToXML(&str);
  if (err) {
    NODE_THROW_OGRERR(err);
    return;
  }
  result = SafeString::New(str);
  CPLFree(str);

  return result;
}

/**
 * Fetch indicated attribute of named node.
 *
 * @method getAttrValue
 * @instance
 * @memberof SpatialReference
 * @param {string} node_name
 * @param {number} [attr_index=0]
 * @return {string}
 */
Napi::Value SpatialReference::getAttrValue(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();
  std::string node_name("");
  int child = 0;
  NODE_ARG_STR(0, "node name", node_name);
  NODE_ARG_INT_OPT(1, "child", child);
  return SafeString::New(srs->this_->GetAttrValue(node_name.c_str(), child));
}

/**
 * Creates a spatial reference from a WKT string.
 *
 * @static
 * @throws {Error}
 * @method fromWKT
 * @instance
 * @memberof SpatialReference
 * @param {string} wkt
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromWKT(const Napi::CallbackInfo& info) {

  std::string wkt("");
  NODE_ARG_STR(0, "wkt", wkt);
  OGRChar *str = (OGRChar *)wkt.c_str();

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromWkt(&str);
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Creates a spatial reference from a Proj.4 string.
 *
 * @static
 * @throws {Error}
 * @method fromProj4
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromProj4(const Napi::CallbackInfo& info) {

  std::string input("");
  NODE_ARG_STR(0, "input", input);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromProj4(input.c_str());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Creates a spatial reference from a WMSAUTO string.
 *
 * Note that the WMS 1.3 specification does not include the units code, while
 * apparently earlier specs do. GDAL tries to guess around this.
 *
 * @example
 *
 * var wms = 'AUTO:42001,99,8888';
 * var ref = gdal.SpatialReference.fromWMSAUTO(wms);
 *
 * @static
 * @throws {Error}
 * @method fromWMSAUTO
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromWMSAUTO(const Napi::CallbackInfo& info) {

  std::string input("");
  NODE_ARG_STR(0, "input", input);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromWMSAUTO(input.c_str());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Import coordinate system from XML format (GML only currently).
 *
 * @static
 * @throws {Error}
 * @method fromXML
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromXML(const Napi::CallbackInfo& info) {

  std::string input("");
  NODE_ARG_STR(0, "xml", input);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromXML(input.c_str());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Initialize from OGC URN.
 *
 * The OGC URN should be prefixed with "urn:ogc:def:crs:" per recommendation
 * paper 06-023r1. Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * @static
 * @throws {Error}
 * @method fromURN
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromURN(const Napi::CallbackInfo& info) {

  std::string input("");
  NODE_ARG_STR(0, "input", input);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromURN(input.c_str());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Initialize from OGC URL.
 *
 * The OGC URL should be prefixed with "http://opengis.net/def/crs" per best
 * practice paper 11-135. Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * @static
 * @throws {Error}
 * @method fromCRSURL
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */

/**
 * Initialize from OGC URL.
 * @async
 *
 * The OGC URL should be prefixed with "http://opengis.net/def/crs" per best
 * practice paper 11-135. Currently EPSG and OGC authority values are supported,
 * including OGC auto codes, but not including CRS1 or CRS88 (NAVD88).
 *
 * @static
 * @throws {Error}
 * @method fromCRSURLAsync
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @param {callback<SpatialReference>} [callback=undefined]
 * @return {Promise<SpatialReference>}
 */
GDAL_ASYNCABLE_DEFINE(SpatialReference::fromCRSURL) {

  std::string input("");
  NODE_ARG_STR(0, "url", input);

  GDALAsyncableJob<OGRSpatialReference *> job(0);

  job.main = [input](const GDALExecutionProgress &progress) {
    OGRSpatialReference *srs = new OGRSpatialReference();
    int err = srs->importFromCRSURL(input.c_str());
    if (err) {
      delete srs;
      throw getOGRErrMsg(err);
    }
    return srs;
  };
  job.rval = [](OGRSpatialReference *srs, const GetFromPersistentFunc &) { return SpatialReference::New(srs, true); };
  job.run(info, async, 1);
}

/**
 * Initialize spatial reference from a URL.
 *
 * This method will download the spatial reference from the given URL.
 *
 * @static
 * @throws {Error}
 * @method fromURL
 * @instance
 * @memberof SpatialReference
 * @param {string} url
 * @return {SpatialReference}
 */

/**
 * Initialize spatial reference from a URL.
 * {{async}}
 *
 * This method will download the spatial reference from the given URL.
 *
 * @static
 * @throws {Error}
 * @method fromURLAsync
 * @instance
 * @memberof SpatialReference
 * @param {string} url
 * @param {callback<SpatialReference>} [callback=undefined]
 * @return {Promise<SpatialReference>}
 */

GDAL_ASYNCABLE_DEFINE(SpatialReference::fromURL) {

  std::string input("");
  NODE_ARG_STR(0, "url", input);

  GDALAsyncableJob<OGRSpatialReference *> job(0);

  job.main = [input](const GDALExecutionProgress &progress) {
    OGRSpatialReference *srs = new OGRSpatialReference();
    OGRErr err = srs->importFromUrl(input.c_str());
    if (err) {
      delete srs;
      throw getOGRErrMsg(err);
    }
    return srs;
  };
  job.rval = [](OGRSpatialReference *srs, const GetFromPersistentFunc &) { return SpatialReference::New(srs, true); };
  job.run(info, async, 1);
}

/**
 * Initialize from a Mapinfo style CoordSys definition.
 *
 * @static
 * @throws {Error}
 * @method fromMICoordSys
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromMICoordSys(const Napi::CallbackInfo& info) {

  std::string input("");
  NODE_ARG_STR(0, "input", input);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromMICoordSys(input.c_str());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Initialize from an arbitrary spatial reference string.
 *
 * This method will examine the provided input, and try to deduce the format,
 * and then use it to initialize the spatial reference system.
 *
 * @static
 * @throws {Error}
 * @method fromUserInput
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @return {SpatialReference}
 */

/**
 * Initialize from an arbitrary spatial reference string.
 *
 * This method will examine the provided input, and try to deduce the format,
 * and then use it to initialize the spatial reference system.
 * @async
 *
 * @static
 * @throws {Error}
 * @method fromUserInputAsync
 * @instance
 * @memberof SpatialReference
 * @param {string} input
 * @param {callback<SpatialReference>} [callback=undefined]
 * @return {Promise<SpatialReference>}
 */
GDAL_ASYNCABLE_DEFINE(SpatialReference::fromUserInput) {

  std::string input("");
  NODE_ARG_STR(0, "url", input);

  GDALAsyncableJob<OGRSpatialReference *> job(0);

  job.main = [input](const GDALExecutionProgress &progress) {
    OGRSpatialReference *srs = new OGRSpatialReference();
    int err = srs->SetFromUserInput(input.c_str());
    if (err) {
      delete srs;
      throw getOGRErrMsg(err);
    }
    return srs;
  };
  job.rval = [](OGRSpatialReference *srs, const GetFromPersistentFunc &) { return SpatialReference::New(srs, true); };
  job.run(info, async, 1);
}

/**
 * Initialize from EPSG GCS or PCS code.
 *
 * @example
 *
 * var ref = gdal.SpatialReference.fromEPSGA(4326);
 *
 * @static
 * @throws {Error}
 * @method fromEPSG
 * @instance
 * @memberof SpatialReference
 * @param {number} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromEPSG(const Napi::CallbackInfo& info) {

  int epsg;
  NODE_ARG_INT(0, "epsg", epsg);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromEPSG(epsg);
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Initialize from EPSG GCS or PCS code.
 *
 * This method is similar to `fromEPSG()` except that EPSG preferred axis
 * ordering *will* be applied for geographic and projected coordinate systems.
 * EPSG normally defines geographic coordinate systems to use lat/long, and also
 * there are also a few projected coordinate systems that use northing/easting
 * order contrary to typical GIS use).
 *
 * @example
 *
 * var ref = gdal.SpatialReference.fromEPSGA(26910);
 *
 * @static
 * @throws {Error}
 * @method fromEPSGA
 * @instance
 * @memberof SpatialReference
 * @param {number} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromEPSGA(const Napi::CallbackInfo& info) {

  int epsg;
  NODE_ARG_INT(0, "epsg", epsg);

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromEPSGA(epsg);
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * Import coordinate system from ESRI .prj format(s).
 *
 * This function will read the text loaded from an ESRI .prj file, and translate
 * it into an OGRSpatialReference definition. This should support many (but by
 * no means all) old style (Arc/Info 7.x) .prj files, as well as the newer
 * pseudo-OGC WKT .prj files. Note that new style .prj files are in OGC WKT
 * format, but require some manipulation to correct datum names, and units on
 * some projection parameters. This is addressed within importFromESRI() by an
 * automatical call to morphFromESRI().
 *
 * Currently only GEOGRAPHIC, UTM, STATEPLANE, GREATBRITIAN_GRID, ALBERS,
 * EQUIDISTANT_CONIC, TRANSVERSE (mercator), POLAR, MERCATOR and POLYCONIC
 * projections are supported from old style files.
 *
 * @static
 * @throws {Error}
 * @method fromESRI
 * @instance
 * @memberof SpatialReference
 * @param {object|string[]} input
 * @return {SpatialReference}
 */
Napi::Value SpatialReference::fromESRI(const Napi::CallbackInfo& info) {

  StringList list;

  if (info.Length() < 1) {
    Napi::Error::New(env, "input string list must be provided").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (list.parse(info[0])) {
    return; // error parsing string list
  }

  OGRSpatialReference *srs = new OGRSpatialReference();
  int err = srs->importFromESRI(list.get());
  if (err) {
    delete srs;
    NODE_THROW_OGRERR(err);
    return;
  }

  return SpatialReference::New(srs, true);
}

/**
 * @typedef {object} units
 * @property {string} units
 * @property {number} value
 */

/**
 * Fetch linear geographic coordinate system units.
 *
 * @method getLinearUnits
 * @instance
 * @memberof SpatialReference
 * @return {units} An object containing `value` and `unit` properties.
 */
Napi::Value SpatialReference::getLinearUnits(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();

  OGRChar *unit_name;
  double units = srs->this_->GetLinearUnits(&unit_name);

  Napi::Object result = Napi::Object::New(env);
  (result).Set(Napi::String::New(env, "value"), Napi::Number::New(env, units));
  (result).Set(Napi::String::New(env, "units"), SafeString::New(unit_name));

  return result;
}

/**
 * Fetch angular geographic coordinate system units.
 *
 * @method getAngularUnits
 * @instance
 * @memberof SpatialReference
 * @return {units} An object containing `value` and `unit` properties.
 */
Napi::Value SpatialReference::getAngularUnits(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();

  OGRChar *unit_name;
  double units = srs->this_->GetAngularUnits(&unit_name);

  Napi::Object result = Napi::Object::New(env);
  (result).Set(Napi::String::New(env, "value"), Napi::Number::New(env, units));
  (result).Set(Napi::String::New(env, "units"), SafeString::New(unit_name));

  return result;
}

/**
 * Validate SRS tokens.
 *
 * This method attempts to verify that the spatial reference system is well
 * formed, and consists of known tokens. The validation is not comprehensive.
 *
 * @method validate
 * @instance
 * @memberof SpatialReference
 * @return {string|null} `"corrupt"`, '"unsupported"', `null` (if fine)
 */
Napi::Value SpatialReference::validate(const Napi::CallbackInfo& info) {

  SpatialReference *srs = info.This().Unwrap<SpatialReference>();

  OGRErr err = srs->this_->Validate();

  if (err == OGRERR_NONE) {
    return env.Null();
    return;
  }
  if (err == OGRERR_CORRUPT_DATA) {
    return Napi::String::New(env, "corrupt");
    return;
  }
  if (err == OGRERR_UNSUPPORTED_SRS) {
    return Napi::String::New(env, "unsupported");
    return;
  }

  NODE_THROW_OGRERR(err);
  return;
}

} // namespace node_gdal
