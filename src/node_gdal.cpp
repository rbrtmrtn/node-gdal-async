// node
#include <napi.h>
#include <uv.h>
#include <node_buffer.h>
#include <node_version.h>

// nan
#include <napi.h>

// gdal
#include <gdal.h>

// node-gdal
#include "gdal_algorithms.hpp"
#include "gdal_common.hpp"
#include "gdal_dataset.hpp"
#include "gdal_driver.hpp"
#include "gdal_rasterband.hpp"
#include "gdal_group.hpp"
#include "gdal_mdarray.hpp"
#include "gdal_dimension.hpp"
#include "gdal_attribute.hpp"
#include "gdal_warper.hpp"
#include "gdal_utils.hpp"

#include "gdal_coordinate_transformation.hpp"
#include "gdal_feature.hpp"
#include "gdal_feature_defn.hpp"
#include "gdal_field_defn.hpp"
#include "geometry/gdal_geometry.hpp"
#include "geometry/gdal_geometrycollection.hpp"
#include "gdal_layer.hpp"
#include "geometry/gdal_simplecurve.hpp"
#include "geometry/gdal_linearring.hpp"
#include "geometry/gdal_linestring.hpp"
#include "geometry/gdal_circularstring.hpp"
#include "geometry/gdal_compoundcurve.hpp"
#include "geometry/gdal_multilinestring.hpp"
#include "geometry/gdal_multicurve.hpp"
#include "geometry/gdal_multipoint.hpp"
#include "geometry/gdal_multipolygon.hpp"
#include "geometry/gdal_point.hpp"
#include "geometry/gdal_polygon.hpp"
#include "gdal_spatial_reference.hpp"
#include "gdal_memfile.hpp"
#include "gdal_fs.hpp"

#include "utils/field_types.hpp"

// collections
#include "collections/dataset_bands.hpp"
#include "collections/dataset_layers.hpp"
#include "collections/group_groups.hpp"
#include "collections/group_arrays.hpp"
#include "collections/group_dimensions.hpp"
#include "collections/group_attributes.hpp"
#include "collections/array_dimensions.hpp"
#include "collections/array_attributes.hpp"
#include "collections/feature_defn_fields.hpp"
#include "collections/feature_fields.hpp"
#include "collections/gdal_drivers.hpp"
#include "collections/geometry_collection_children.hpp"
#include "collections/layer_features.hpp"
#include "collections/layer_fields.hpp"
#include "collections/linestring_points.hpp"
#include "collections/polygon_rings.hpp"
#include "collections/compound_curves.hpp"
#include "collections/rasterband_overviews.hpp"
#include "collections/rasterband_pixels.hpp"
#include "collections/colortable.hpp"

// std
#include <sstream>
#include <string>
#include <vector>

namespace node_gdal {

using namespace Napi;
using namespace Napi;

FILE *log_file = NULL;
ObjectStore object_store;
bool eventLoopWarn = true;

Napi::Value LastErrorGetter(const Napi::CallbackInfo &info) {

  int errtype = CPLGetLastErrorType();
  if (errtype == CE_None) { return env.Null(); }

  Napi::Object result = Napi::Object::New(env);
  (result).Set(Napi::String::New(env, "code"), Napi::New(env, CPLGetLastErrorNo()));
  (result).Set(Napi::String::New(env, "message"), Napi::New(env, CPLGetLastErrorMsg()));
  (result).Set(Napi::String::New(env, "level"), Napi::New(env, errtype));
  return result;
}

void LastErrorSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {

  if (value.IsNull()) {
    CPLErrorReset();
  } else {
    Napi::Error::New(env, "'lastError' only supports being set to null").ThrowAsJavaScriptException();
    return env.Null();
  }
}

Napi::Value EventLoopWarningGetter(const Napi::CallbackInfo &info) {
  return Napi::Boolean::New(env, eventLoopWarn);
}

void EventLoopWarningSetter(const Napi::CallbackInfo &info, const Napi::Value &value) {
  if (!value->IsBoolean()) {
    Napi::Error::New(env, "'eventLoopWarning' must be a boolean value").ThrowAsJavaScriptException();
    return env.Null();
  }
  eventLoopWarn = value.As<Napi::Boolean>().Value().ToChecked();
}

extern "C" {

static Napi::Value QuietOutput(const Napi::CallbackInfo &info) {
  CPLSetErrorHandler(CPLQuietErrorHandler);
  return;
}

static Napi::Value VerboseOutput(const Napi::CallbackInfo &info) {
  CPLSetErrorHandler(CPLDefaultErrorHandler);
  return;
}

#ifdef ENABLE_LOGGING
static NAN_GC_CALLBACK(beforeGC) {
  LOG("%s", "Starting garbage collection");
}

static NAN_GC_CALLBACK(afterGC) {
  LOG("%s", "Finished garbage collection");
}
#endif

static Napi::Value StartLogging(const Napi::CallbackInfo &info) {

#ifdef ENABLE_LOGGING
  std::string filename = "";
  NODE_ARG_STR(0, "filename", filename);
  if (filename.empty()) {
    Napi::Error::New(env, "Invalid filename").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (log_file) fclose(log_file);
  log_file = fopen(filename.c_str(), "w");
  if (!log_file) {
    Napi::Error::New(env, "Error creating log file").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::AddGCPrologueCallback(beforeGC);
  Napi::AddGCEpilogueCallback(afterGC);

#else
  Napi::Error::New(env, "Logging requires node-gdal be compiled with --enable_logging=true")
    .ThrowAsJavaScriptException();

#endif

  return;
}

static Napi::Value StopLogging(const Napi::CallbackInfo &info) {
#ifdef ENABLE_LOGGING
  if (log_file) {
    fclose(log_file);
    log_file = NULL;
  }
#endif

  return;
}

static Napi::Value Log(const Napi::CallbackInfo &info) {
  std::string msg;
  NODE_ARG_STR(0, "message", msg);
  msg = msg + "\n";

#ifdef ENABLE_LOGGING
  if (log_file) {
    fputs(msg.c_str(), log_file);
    fflush(log_file);
  }
#endif

  return;
}

/*
 * Common code for sync and async opening.
 */
GDAL_ASYNCABLE_GLOBAL(gdal_open);
GDAL_ASYNCABLE_DEFINE(gdal_open) {

  std::string path;
  std::string mode = "r";

  NODE_ARG_STR(0, "path", path);
  NODE_ARG_OPT_STR(1, "mode", mode);

  unsigned int flags = 0;
  for (unsigned i = 0; i < mode.Length(); i++) {
    if (mode[i] == 'r') {
      if (i < mode.Length() - 1 && mode[i + 1] == '+') {
        flags |= GDAL_OF_UPDATE;
        i++;
      } else {
        flags |= GDAL_OF_READONLY;
      }
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
    } else if (mode[i] == 'm') {
      flags |= GDAL_OF_MULTIDIM_RASTER;
#endif
    } else {
      Napi::Error::New(env, "Invalid open mode. Must contain only \"r\" or \"r+\" and \"m\" ")
        .ThrowAsJavaScriptException();
      return env.Null();
    }
  }
  flags |= GDAL_OF_VERBOSE_ERROR;

  GDALAsyncableJob<GDALDataset *> job(0);
  job.rval = [](GDALDataset *ds, const GetFromPersistentFunc &) { return Dataset::New(ds); };
  job.main = [path, flags](const GDALExecutionProgress &) {
    GDALDataset *ds = (GDALDataset *)GDALOpenEx(path.c_str(), flags, NULL, NULL, NULL);
    if (!ds) throw CPLGetLastErrorMsg();
    return ds;
  };
  job.run(info, async, 2);
}

static Napi::Value setConfigOption(const Napi::CallbackInfo &info) {

  std::string name;

  NODE_ARG_STR(0, "name", name);

  if (info.Length() < 2) {
    Napi::Error::New(env, "string or null value must be provided").ThrowAsJavaScriptException();
    return env.Null();
  }
  if (info[1].IsString()) {
    std::string val = info[1].As<Napi::String>().Utf8Value().c_str();
    CPLSetConfigOption(name.c_str(), val.c_str());
  } else if (info[1].IsNull() || info[1].IsUndefined()) {
    CPLSetConfigOption(name.c_str(), NULL);
  } else {
    Napi::Error::New(env, "value must be a string or null").ThrowAsJavaScriptException();
    return env.Null();
  }

  return;
}

static Napi::Value getConfigOption(const Napi::CallbackInfo &info) {

  std::string name;
  NODE_ARG_STR(0, "name", name);

  return SafeString::New(CPLGetConfigOption(name.c_str(), NULL));
}

/**
 * @typedef {string[]|Record<string, string|number|(string|number)[]>} StringOptions
 */

/**
 * Convert decimal degrees to degrees, minutes, and seconds string.
 *
 * @static
 * @method decToDMS
 * @param {number} angle
 * @param {string} axis `"lat"` or `"long"`
 * @param {number} [precision=2]
 * @return {string} A string nndnn'nn.nn'"L where n is a number and L is either
 * N or E
 */
static Napi::Value decToDMS(const Napi::CallbackInfo &info) {

  double angle;
  std::string axis;
  int precision = 2;
  NODE_ARG_DOUBLE(0, "angle", angle);
  NODE_ARG_STR(1, "axis", axis);
  NODE_ARG_INT_OPT(2, "precision", precision);

  if (axis.Length() > 0) { axis[0] = toupper(axis[0]); }
  if (axis != "Lat" && axis != "Long") {
    Napi::Error::New(env, "Axis must be 'lat' or 'long'").ThrowAsJavaScriptException();
    return env.Null();
  }

  return SafeString::New(GDALDecToDMS(angle, axis.c_str(), precision));
}

/**
 * Set paths where proj will search it data.
 *
 * @static
 * @method setPROJSearchPaths
 * @param {string} path `c:\ProjData`
 */
static Napi::Value setPROJSearchPath(const Napi::CallbackInfo &info) {
  std::string path;

  NODE_ARG_STR(0, "path", path);

#if GDAL_VERSION_MAJOR >= 3
  const char *const paths[] = {path.c_str(), nullptr};
  OSRSetPROJSearchPaths(paths);
#endif
}

static Napi::Value ThrowDummyCPLError(const Napi::CallbackInfo &info) {
  CPLError(CE_Failure, CPLE_AppDefined, "Mock error");
  return;
}

static Napi::Value isAlive(const Napi::CallbackInfo &info) {

  long uid;
  NODE_ARG_INT(0, "uid", uid);

  return Napi::New(env, object_store.isAlive(uid));
}

void Cleanup(void *) {
  object_store.cleanup();
}

static void Init(Napi::Object target, Local<v8::Value>, void *) {
  static bool initialized = false;
  if (initialized) {
    Napi::Error::New(env, "gdal-async does not yet support multiple instances per V8 isolate")
      .ThrowAsJavaScriptException();
    return env.Null();
  }
  initialized = true;
  mainV8ThreadId = std::this_thread::get_id();

  Nan__SetAsyncableMethod(target, "open", gdal_open);
  exports.Set(Napi::String::New(env, "setConfigOption"), Napi::Function::New(env, setConfigOption));
  exports.Set(Napi::String::New(env, "getConfigOption"), Napi::Function::New(env, getConfigOption));
  exports.Set(Napi::String::New(env, "decToDMS"), Napi::Function::New(env, decToDMS));
  exports.Set(Napi::String::New(env, "setPROJSearchPath"), Napi::Function::New(env, setPROJSearchPath));
  exports.Set(Napi::String::New(env, "_triggerCPLError"), Napi::Function::New(env, ThrowDummyCPLError)); // for tests
  exports.Set(Napi::String::New(env, "_isAlive"), Napi::Function::New(env, isAlive));                    // for tests

  Warper::Initialize(env, target, module);
  Algorithms::Initialize(env, target, module);

  Driver::Initialize(env, target, module);
  Dataset::Initialize(env, target, module);
  RasterBand::Initialize(env, target, module);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  Group::Initialize(env, target, module);
  MDArray::Initialize(env, target, module);
  Dimension::Initialize(env, target, module);
  Attribute::Initialize(env, target, module);
#endif

  Layer::Initialize(env, target, module);
  Feature::Initialize(env, target, module);
  FeatureDefn::Initialize(env, target, module);
  FieldDefn::Initialize(env, target, module);
  Geometry::Initialize(env, target, module);
  Point::Initialize(env, target, module);
  SimpleCurve::Initialize(env, target, module);
  LineString::Initialize(env, target, module);
  LinearRing::Initialize(env, target, module);
  Polygon::Initialize(env, target, module);
  GeometryCollection::Initialize(env, target, module);
  MultiPoint::Initialize(env, target, module);
  MultiLineString::Initialize(env, target, module);
  MultiPolygon::Initialize(env, target, module);
  CircularString::Initialize(env, target, module);
  CompoundCurve::Initialize(env, target, module);
  MultiCurve::Initialize(env, target, module);

  SpatialReference::Initialize(env, target, module);
  CoordinateTransformation::Initialize(env, target, module);
  ColorTable::Initialize(env, target, module);

  DatasetBands::Initialize(env, target, module);
  DatasetLayers::Initialize(env, target, module);
#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  GroupGroups::Initialize(env, target, module);
  GroupArrays::Initialize(env, target, module);
  GroupDimensions::Initialize(env, target, module);
  GroupAttributes::Initialize(env, target, module);
  ArrayDimensions::Initialize(env, target, module);
  ArrayAttributes::Initialize(env, target, module);
#endif
  LayerFeatures::Initialize(env, target, module);
  FeatureFields::Initialize(env, target, module);
  LayerFields::Initialize(env, target, module);
  FeatureDefnFields::Initialize(env, target, module);
  GeometryCollectionChildren::Initialize(env, target, module);
  PolygonRings::Initialize(env, target, module);
  LineStringPoints::Initialize(env, target, module);
  CompoundCurveCurves::Initialize(env, target, module);
  RasterBandOverviews::Initialize(env, target, module);
  RasterBandPixels::Initialize(env, target, module);
  Memfile::Initialize(env, target, module);
  Utils::Initialize(env, target, module);
  VSI::Initialize(env, target, module);

  /**
   * The collection of all drivers registered with GDAL
   *
   * @readonly
   * @static
   * @constant
   * @name drivers
   * @type {GDALDrivers}
   */
  GDALDrivers::Initialize(env, target, module); // calls GDALRegisterAll()
  (target).Set(Napi::String::New(env, "drivers"), GDALDrivers::New());

  /*
   * DMD Constants
   */

  /**
   * @final
   * @constant
   * @type {string}
   * @name DMD_LONGNAME
   */
  (target).Set(Napi::String::New(env, "DMD_LONGNAME"), Napi::New(env, GDAL_DMD_LONGNAME));
  /**
   * @final
   * @constant
   * @name DMD_MIMETYPE
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DMD_MIMETYPE"), Napi::New(env, GDAL_DMD_MIMETYPE));
  /**
   * @final
   * @constant
   * @name DMD_HELPTOPIC
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DMD_HELPTOPIC"), Napi::New(env, GDAL_DMD_HELPTOPIC));
  /**
   * @final
   * @constant
   * @name DMD_EXTENSION
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DMD_EXTENSION"), Napi::New(env, GDAL_DMD_EXTENSION));
  /**
   * @final
   * @constant
   * @name DMD_CREATIONOPTIONLIST
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DMD_CREATIONOPTIONLIST"), Napi::New(env, GDAL_DMD_CREATIONOPTIONLIST));
  /**
   * @final
   * @constant
   * @name DMD_CREATIONDATATYPES
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DMD_CREATIONDATATYPES"), Napi::New(env, GDAL_DMD_CREATIONDATATYPES));

  /*
   * CE Error levels
   */

  /**
   * Error level: (no error)
   *
   * @final
   * @constant
   * @name CE_None
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CE_None"), Napi::New(env, CE_None));
  /**
   * Error level: Debug
   *
   * @final
   * @constant
   * @name CE_Debug
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CE_Debug"), Napi::New(env, CE_Debug));
  /**
   * Error level: Warning
   *
   * @final
   * @constant
   * @name CE_Warning
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CE_Warning"), Napi::New(env, CE_Warning));
  /**
   * Error level: Failure
   *
   * @final
   * @constant
   * @name CE_Failure
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CE_Failure"), Napi::New(env, CE_Failure));
  /**
   * Error level: Fatal
   *
   * @final
   * @constant
   * @name CE_Fatal
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CE_Fatal"), Napi::New(env, CE_Fatal));

  /*
   * CPL Error codes
   */

  /**
   * @final
   * @constant
   * @name CPLE_None
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_None"), Napi::New(env, CPLE_None));
  /**
   * @final
   * @constant
   * @name CPLE_AppDefined
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_AppDefined"), Napi::New(env, CPLE_AppDefined));
  /**
   * @final
   * @constant
   * @name CPLE_OutOfMemory
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_OutOfMemory"), Napi::New(env, CPLE_OutOfMemory));
  /**
   * @final
   * @constant
   * @name CPLE_FileIO
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_FileIO"), Napi::New(env, CPLE_FileIO));
  /**
   * @final
   * @constant
   * @name CPLE_OpenFailed
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_OpenFailed"), Napi::New(env, CPLE_OpenFailed));
  /**
   * @final
   * @constant
   * @name CPLE_IllegalArg
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_IllegalArg"), Napi::New(env, CPLE_IllegalArg));
  /**
   * @final
   * @constant
   * @name CPLE_NotSupported
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_NotSupported"), Napi::New(env, CPLE_NotSupported));
  /**
   * @final
   * @constant
   * @name CPLE_AssertionFailed
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_AssertionFailed"), Napi::New(env, CPLE_AssertionFailed));
  /**
   * @final
   * @constant
   * @name CPLE_NoWriteAccess
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_NoWriteAccess"), Napi::New(env, CPLE_NoWriteAccess));
  /**
   * @final
   * @constant
   * @name CPLE_UserInterrupt
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_UserInterrupt"), Napi::New(env, CPLE_UserInterrupt));
  /**
   * @final
   * @constant
   * @name CPLE_objectNull
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "CPLE_ObjectNull"), Napi::New(env, CPLE_ObjectNull));

  /*
   * Driver Dataset creation constants
   */

  /**
   * @final
   * @constant
   * @name DCAP_CREATE
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DCAP_CREATE"), Napi::New(env, GDAL_DCAP_CREATE));
  /**
   * @final
   * @constant
   * @name DCAP_CREATECOPY
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DCAP_CREATECOPY"), Napi::New(env, GDAL_DCAP_CREATECOPY));
  /**
   * @final
   * @constant
   * @name DCAP_VIRTUALIO
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DCAP_VIRTUALIO"), Napi::New(env, GDAL_DCAP_VIRTUALIO));

  /*
   * OLC Constants
   */

  /**
   * @final
   * @constant
   * @name OLCRandomRead
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCRandomRead"), Napi::New(env, OLCRandomRead));
  /**
   * @final
   * @constant
   * @name OLCSequentialWrite
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCSequentialWrite"), Napi::New(env, OLCSequentialWrite));
  /**
   * @final
   * @constant
   * @name OLCRandomWrite
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCRandomWrite"), Napi::New(env, OLCRandomWrite));
  /**
   * @final
   * @constant
   * @name OLCFastSpatialFilter
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCFastSpatialFilter"), Napi::New(env, OLCFastSpatialFilter));
  /**
   * @final
   * @constant
   * @name OLCFastFeatureCount
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCFastFeatureCount"), Napi::New(env, OLCFastFeatureCount));
  /**
   * @final
   * @constant
   * @name OLCFastGetExtent
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCFastGetExtent"), Napi::New(env, OLCFastGetExtent));
  /**
   * @final
   * @constant
   * @name OLCCreateField
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCCreateField"), Napi::New(env, OLCCreateField));
  /**
   * @final
   * @constant
   * @name OLCDeleteField
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCDeleteField"), Napi::New(env, OLCDeleteField));
  /**
   * @final
   * @constant
   * @name OLCReorderFields
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCReorderFields"), Napi::New(env, OLCReorderFields));
  /**
   * @final
   * @constant
   * @name OLCAlterFieldDefn
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCAlterFieldDefn"), Napi::New(env, OLCAlterFieldDefn));
  /**
   * @final
   * @constant
   * @name OLCTransactions
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCTransactions"), Napi::New(env, OLCTransactions));
  /**
   * @final
   * @constant
   * @name OLCDeleteFeature
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCDeleteFeature"), Napi::New(env, OLCDeleteFeature));
  /**
   * @final
   * @constant
   * @name OLCFastSetNextByIndex
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCFastSetNextByIndex"), Napi::New(env, OLCFastSetNextByIndex));
  /**
   * @final
   * @constant
   * @name OLCStringsAsUTF8
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCStringsAsUTF8"), Napi::New(env, OLCStringsAsUTF8));
  /**
   * @final
   * @constant
   * @name OLCIgnoreFields
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCIgnoreFields"), Napi::New(env, OLCIgnoreFields));

#ifdef OLCCreateGeomField
  /**
   * @final
   * @constant
   * @name OLCCreateGeomField
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OLCCreateGeomField"), Napi::New(env, OLCCreateGeomField));
#endif
#ifdef ODsCCreateGeomFieldAfterCreateLayer

  /*
   * ODsC constants
   */

  /**
   * @final
   * @constant
   * @name ODsCCreateLayer
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "ODsCCreateLayer"), Napi::New(env, ODsCCreateLayer));
  /**
   * @final
   * @constant
   * @name ODsCDeleteLayer
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "ODsCDeleteLayer"), Napi::New(env, ODsCDeleteLayer));
  /**
   * @final
   * @constant
   * @name ODsCCreateGeomFieldAfterCreateLayer
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "ODsCCreateGeomFieldAfterCreateLayer"), Napi::New(env, ODsCCreateGeomFieldAfterCreateLayer));
#endif
  /**
   * @final
   * @constant
   * @name ODrCCreateDataSource
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "ODrCCreateDataSource"), Napi::New(env, ODrCCreateDataSource));
  /**
   * @final
   * @constant
   * @name ODrCDeleteDataSource
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "ODrCDeleteDataSource"), Napi::New(env, ODrCDeleteDataSource));

  /*
   * open flags
   */

  /**
   * @final
   * @constant
   * @name GA_Readonly
   * @type {number}
   */
  NODE_DEFINE_CONSTANT(target, GA_ReadOnly);

  /**
   * @final
   * @constant
   * @name GA_Update
   * @type {number}
   */
  NODE_DEFINE_CONSTANT(target, GA_Update);

  /*
   * RasterIO flags
   */

  /**
   * @final
   * @constant
   * @name GF_Read
   * @type {number}
   */
  NODE_DEFINE_CONSTANT(target, GF_Read);

  /**
   * @final
   * @constant
   * @name GF_Write
   * @type {number}
   */
  NODE_DEFINE_CONSTANT(target, GF_Write);

  /*
   * Pixel data types.
   */

  /**
   * Unknown or unspecified type
   * @final
   * @constant
   * @name GDT_Unknown
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Unknown"), env.Undefined());
  /**
   * Eight bit unsigned integer
   * @final
   * @constant
   * @name GDT_Byte
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Byte"), Napi::New(env, GDALGetDataTypeName(GDT_Byte)));
  /**
   * Sixteen bit unsigned integer
   * @final
   * @constant
   * @name GDT_UInt16
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_UInt16"), Napi::New(env, GDALGetDataTypeName(GDT_UInt16)));
  /**
   * Sixteen bit signed integer
   * @final
   * @constant
   * @name GDT_Int16
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Int16"), Napi::New(env, GDALGetDataTypeName(GDT_Int16)));
  /**
   * Thirty two bit unsigned integer
   * @final
   * @constant
   * @name GDT_UInt32
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_UInt32"), Napi::New(env, GDALGetDataTypeName(GDT_UInt32)));
  /**
   * Thirty two bit signed integer
   * @final
   * @constant
   * @name GDT_Int32
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Int32"), Napi::New(env, GDALGetDataTypeName(GDT_Int32)));
  /**
   * Thirty two bit floating point
   * @final
   * @constant
   * @name GDT_Float32
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Float32"), Napi::New(env, GDALGetDataTypeName(GDT_Float32)));
  /**
   * Sixty four bit floating point
   * @final
   * @constant
   * @name GDT_Float64
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_Float64"), Napi::New(env, GDALGetDataTypeName(GDT_Float64)));
  /**
   * Complex Int16
   * @final
   * @constant
   * @name GDT_CInt16
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_CInt16"), Napi::New(env, GDALGetDataTypeName(GDT_CInt16)));
  /**
   * Complex Int32
   * @final
   * @constant
   * @name GDT_CInt32
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_CInt32"), Napi::New(env, GDALGetDataTypeName(GDT_CInt32)));
  /**
   * Complex Float32
   * @final
   * @constant
   * @name GDT_CFloat32
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_CFloat32"), Napi::New(env, GDALGetDataTypeName(GDT_CFloat32)));
  /**
   * Complex Float64
   * @final
   * @constant
   * @name GDT_CFloat64
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GDT_CFloat64"), Napi::New(env, GDALGetDataTypeName(GDT_CFloat64)));

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  /**
   * String extended type for MDArrays (GDAL >= 3.1)
   * @final
   * @constant
   * @name GEDTC_String
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GEDTC_String"), Napi::String::New(env, "String"));

  /**
   * String extended type for MDArrays (GDAL >= 3.1)
   * @final
   * @constant
   * @name GEDTC_Compound
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GEDTC_Compound"), Napi::String::New(env, "Compound"));
#endif

  /*
   * Justification
   */

  /**
   * @final
   * @constant
   * @name OJUndefined
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OJUndefined"), env.Undefined());
  /**
   * @final
   * @constant
   * @name OJLeft
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OJLeft"), Napi::String::New(env, "Left"));
  /**
   * @final
   * @constant
   * @name OJRight
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OJRight"), Napi::String::New(env, "Right"));

  /*
   * Color interpretation constants
   */

  /**
   * @final
   * @constant
   * @name GCI_Undefined
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_Undefined"), env.Undefined());
  /**
   * @final
   * @constant
   * @name GCI_GrayIndex
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_GrayIndex"), Napi::New(env, GDALGetColorInterpretationName(GCI_GrayIndex)));
  /**
   * @final
   * @constant
   * @name GCI_PaletteIndex
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_PaletteIndex"), Napi::New(env, GDALGetColorInterpretationName(GCI_PaletteIndex)));
  /**
   * @final
   * @constant
   * @name GCI_RedBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_RedBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_RedBand)));
  /**
   * @final
   * @constant
   * @name GCI_GreenBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_GreenBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_GreenBand)));
  /**
   * @final
   * @constant
   * @name GCI_BlueBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_BlueBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_BlueBand)));
  /**
   * @final
   * @constant
   * @name GCI_AlphaBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_AlphaBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_AlphaBand)));
  /**
   * @final
   * @constant
   * @name GCI_HueBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_HueBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_HueBand)));
  /**
   * @final
   * @constant
   * @name GCI_SaturationBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_SaturationBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_SaturationBand)));
  /**
   * @final
   * @constant
   * @name GCI_LightnessBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_LightnessBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_LightnessBand)));
  /**
   * @final
   * @constant
   * @name GCI_CyanBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_CyanBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_CyanBand)));
  /**
   * @final
   * @constant
   * @name GCI_MagentaBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_MagentaBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_MagentaBand)));
  /**
   * @final
   * @constant
   * @name GCI_YellowBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_YellowBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_YellowBand)));
  /**
   * @final
   * @constant
   * @name GCI_BlackBand
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GCI_BlackBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_BlackBand)));
  /**
   * @final
   * @constant
   * @name GCI_YCbCr_YBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_YCbCr_YBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_YCbCr_YBand)));
  /**
   * @final
   * @constant
   * @name GCI_YCbCr_CbBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_YCbCr_CbBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_YCbCr_CbBand)));
  /**
   * @final
   * @constant
   * @name GCI_YCbCr_CrBand
   * @type {string}
   */
  (target).Set(
    Napi::String::New(env, "GCI_YCbCr_CrBand"), Napi::New(env, GDALGetColorInterpretationName(GCI_YCbCr_CrBand)));

  /*
   * Palette types.
   */

  /**
   * Grayscale, only c1 defined
   * @final
   * @constant
   * @name GPI_Gray
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GPI_Gray"), Napi::String::New(env, "Gray"));

  /**
   * RGBA, alpha in c4
   * @final
   * @constant
   * @name GPI_RGB
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GPI_RGB"), Napi::String::New(env, "RGB"));

  /**
   * CMYK
   * @final
   * @constant
   * @name GPI_CMYK
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GPI_CMYK"), Napi::String::New(env, "CMYK"));

  /**
   * HLS, c4 is not defined
   * @final
   * @constant
   * @name GPI_HLS
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GPI_HLS"), Napi::String::New(env, "HLS"));

  /*
   * WKB Variants
   */

  /**
   * Old-style 99-402 extended dimension (Z) WKB types.
   * Synonymous with 'wkbVariantOldOgc' (gdal >= 2.0)
   *
   * @final
   * @constant
   * @name wkbVariantOgc
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbVariantOgc"), Napi::String::New(env, "OGC"));

  /**
   * Old-style 99-402 extended dimension (Z) WKB types.
   * Synonymous with 'wkbVariantOgc' (gdal < 2.0)
   *
   * @final
   * @constant
   * @name wkbVariantOldOgc
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbVariantOldOgc"), Napi::String::New(env, "OGC"));

  /**
   * SFSQL 1.2 and ISO SQL/MM Part 3 extended dimension (Z&M) WKB types.
   *
   * @final
   * @constant
   * @name wkbVariantIso
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbVariantIso"), Napi::String::New(env, "ISO"));

  /*
   * WKB Byte Ordering
   */

  /**
   * @final
   * @constant
   * @name wkbXDR
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbXDR"), Napi::String::New(env, "MSB"));
  /**
   * @final
   * @constant
   * @name wkbNDR
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbNDR"), Napi::String::New(env, "LSB"));

  /*
   * WKB Geometry Types
   */

  /**
   * @final
   *
   * The `wkb25DBit` constant can be used to convert between 2D types to 2.5D
   * types
   *
   * @example
   *
   * // 2 -> 2.5D
   * wkbPoint25D = gdal.wkbPoint | gdal.wkb25DBit
   *
   * // 2.5D -> 2D (same as wkbFlatten())
   * wkbPoint = gdal.wkbPoint25D & (~gdal.wkb25DBit)
   *
   * @constant
   * @name wkb25DBit
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkb25DBit"), Napi::Number::New(env, wkb25DBit));

  int wkbLinearRing25D = wkbLinearRing | wkb25DBit;

  /**
   * @final
   * @constant
   * @name wkbUnknown
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbUnknown"), Napi::Number::New(env, wkbUnknown));
  /**
   * @final
   * @constant
   * @name wkbPoint
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbPoint"), Napi::Number::New(env, wkbPoint));
  /**
   * @final
   * @constant
   * @name wkbLineString
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbLineString"), Napi::Number::New(env, wkbLineString));
  /**
   * @final
   * @constant
   * @name wkbCircularString
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbCircularString"), Napi::Number::New(env, wkbCircularString));
  /**
   * @final
   * @constant
   * @name wkbCompoundCurve
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbCompoundCurve"), Napi::Number::New(env, wkbCompoundCurve));
  /**
   * @final
   * @constant
   * @name wkbMultiCurve
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiCurve"), Napi::Number::New(env, wkbMultiCurve));
  /**
   * @final
   * @constant
   * @name wkbPolygon
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbPolygon"), Napi::Number::New(env, wkbPolygon));
  /**
   * @final
   * @constant
   * @name wkbMultiPoint
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiPoint"), Napi::Number::New(env, wkbMultiPoint));
  /**
   * @final
   * @constant
   * @name wkbMultiLineString
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiLineString"), Napi::Number::New(env, wkbMultiLineString));
  /**
   * @final
   * @constant
   * @name wkbMultiPolygon
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiPolygon"), Napi::Number::New(env, wkbMultiPolygon));
  /**
   * @final
   * @constant
   * @name wkbGeometryCollection
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbGeometryCollection"), Napi::Number::New(env, wkbGeometryCollection));
  /**
   * @final
   * @constant
   * @name wkbNone
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbNone"), Napi::Number::New(env, wkbNone));
  /**
   * @final
   * @constant
   * @name wkbLinearRing
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "wkbLinearRing"), Napi::Number::New(env, wkbLinearRing));
  /**
   * @final
   * @constant
   * @name wkbPoint25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbPoint25D"), Napi::Number::New(env, wkbPoint25D));
  /**
   * @final
   * @constant
   * @name wkbLineString25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbLineString25D"), Napi::Number::New(env, wkbLineString25D));
  /**
   * @final
   * @constant
   * @name wkbPolygon25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbPolygon25D"), Napi::Number::New(env, wkbPolygon25D));
  /**
   * @final
   * @constant
   * @name wkbMultiPoint25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiPoint25D"), Napi::Number::New(env, wkbMultiPoint25D));
  /**
   * @final
   * @constant
   * @name wkbMultiLineString25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiLineString25D"), Napi::Number::New(env, wkbMultiLineString25D));
  /**
   * @final
   * @constant
   * @name wkbMultiPolygon25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbMultiPolygon25D"), Napi::Number::New(env, wkbMultiPolygon25D));
  /**
   * @final
   * @constant
   * @name wkbGeometryCollection25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbGeometryCollection25D"), Napi::Number::New(env, wkbGeometryCollection25D));
  /**
   * @final
   * @constant
   * @name wkbLinearRing25D
   * @type {number}
   */
  (target).Set(Napi::String::New(env, "wkbLinearRing25D"), Napi::Number::New(env, wkbLinearRing25D));

  /*
   * Field types
   */

  /**
   * @final
   * @constant
   * @name OFTInteger
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTInteger"), Napi::New(env, getFieldTypeName(OFTInteger)));
  /**
   * @final
   * @constant
   * @name OFTIntegerList
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTIntegerList"), Napi::New(env, getFieldTypeName(OFTIntegerList)));

  /**
   * @final
   * @constant
   * @name OFTInteger64
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTInteger64"), Napi::New(env, getFieldTypeName(OFTInteger64)));
  /**
   * @final
   * @constant
   * @name OFTInteger64List
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTInteger64List"), Napi::New(env, getFieldTypeName(OFTInteger64List)));
  /**
   * @final
   * @constant
   * @name OFTReal
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTReal"), Napi::New(env, getFieldTypeName(OFTReal)));
  /**
   * @final
   * @constant
   * @name OFTRealList
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTRealList"), Napi::New(env, getFieldTypeName(OFTRealList)));
  /**
   * @final
   * @constant
   * @name OFTString
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTString"), Napi::New(env, getFieldTypeName(OFTString)));
  /**
   * @final
   * @constant
   * @name OFTStringList
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTStringList"), Napi::New(env, getFieldTypeName(OFTStringList)));
  /**
   * @final
   * @constant
   * @name OFTWideString
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTWideString"), Napi::New(env, getFieldTypeName(OFTWideString)));
  /**
   * @final
   * @constant
   * @name OFTWideStringList
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTWideStringList"), Napi::New(env, getFieldTypeName(OFTWideStringList)));
  /**
   * @final
   * @constant
   * @name OFTBinary
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTBinary"), Napi::New(env, getFieldTypeName(OFTBinary)));
  /**
   * @final
   * @constant
   * @name OFTDate
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTDate"), Napi::New(env, getFieldTypeName(OFTDate)));
  /**
   * @final
   * @constant
   * @name OFTTime
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTTime"), Napi::New(env, getFieldTypeName(OFTTime)));
  /**
   * @final
   * @constant
   * @name OFTDateTime
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "OFTDateTime"), Napi::New(env, getFieldTypeName(OFTDateTime)));

  /*
   * Resampling options that can be used with the gdal.reprojectImage() and gdal.RasterBandPixels.read methods.
   */

  /**
   * @final
   * @constant
   * @name GRA_NearestNeighbor
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_NearestNeighbor"), Napi::String::New(env, "NearestNeighbor"));
  /**
   * @final
   * @constant
   * @name GRA_Bilinear
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_Bilinear"), Napi::String::New(env, "Bilinear"));
  /**
   * @final
   * @constant
   * @name GRA_Cubic
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_Cubic"), Napi::String::New(env, "Cubic"));
  /**
   * @final
   * @constant
   * @name GRA_CubicSpline
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_CubicSpline"), Napi::String::New(env, "CubicSpline"));
  /**
   * @final
   * @constant
   * @name GRA_Lanczos
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_Lanczos"), Napi::String::New(env, "Lanczos"));
  /**
   * @final
   * @constant
   * @name GRA_Average
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_Average"), Napi::String::New(env, "Average"));
  /**
   * @final
   * @constant
   * @name GRA_Mode
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "GRA_Mode"), Napi::String::New(env, "Mode"));

#if GDAL_VERSION_MAJOR > 3 || (GDAL_VERSION_MAJOR == 3 && GDAL_VERSION_MINOR >= 1)
  /*
   * Dimension types for gdal.Dimension (GDAL >= 3.3)
   */

  /**
   * @final
   * @constant
   * @name DIM_HORIZONTAL_X
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIM_HORIZONTAL_X"), Napi::New(env, GDAL_DIM_TYPE_HORIZONTAL_X));

  /**
   * @final
   * @constant
   * @name DIM_HORIZONTAL_Y
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIM_HORIZONTAL_Y"), Napi::New(env, GDAL_DIM_TYPE_HORIZONTAL_Y));

  /**
   * @final
   * @constant
   * @name DIM_VERTICAL
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIM_VERTICAL"), Napi::New(env, GDAL_DIM_TYPE_VERTICAL));

  /**
   * @final
   * @constant
   * @name DIM_TEMPORAL
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIM_TEMPORAL"), Napi::New(env, GDAL_DIM_TYPE_TEMPORAL));

  /**
   * @final
   * @constant
   * @name DIM_PARAMETRIC
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIM_PARAMETRIC"), Napi::New(env, GDAL_DIM_TYPE_PARAMETRIC));
#endif

  /*
   * Direction types for gdal.Dimension (GDAL >= 3.3)
   */

  /**
   * @final
   * @constant
   * @name DIR_EAST
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_EAST"), Napi::String::New(env, "EAST"));

  /**
   * @final
   * @constant
   * @name DIR_WEST
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_WEST"), Napi::String::New(env, "WEST"));

  /**
   * @final
   * @constant
   * @name DIR_SOUTH
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_SOUTH"), Napi::String::New(env, "SOUTH"));

  /**
   * @final
   * @constant
   * @name DIR_NORTH
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_NORTH"), Napi::String::New(env, "NORTH"));

  /**
   * @final
   * @constant
   * @name DIR_UP
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_UP"), Napi::String::New(env, "UP"));

  /**
   * @final
   * @constant
   * @name DIR_DOWN
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_DOWN"), Napi::String::New(env, "DOWN"));

  /**
   * @final
   * @constant
   * @name DIR_FUTURE
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_FUTURE"), Napi::String::New(env, "FUTURE"));

  /**
   * @final
   * @constant
   * @name DIR_PAST
   * @type {string}
   */
  (target).Set(Napi::String::New(env, "DIR_PAST"), Napi::String::New(env, "PAST"));

  /**
   * GDAL version (not the binding version)
   *
   * @final
   * @constant {string} version
   */
  (target).Set(Napi::String::New(env, "version"), Napi::New(env, GDAL_RELEASE_NAME));

  /**
   * GDAL library - system library (false) or bundled (true)
   *
   * @final
   * @constant {boolean} bundled
   */
#ifdef BUNDLED_GDAL
  (target).Set(Napi::String::New(env, "bundled"), Napi::New(env, true));
#else
  (target).Set(Napi::String::New(env, "bundled"), Napi::New(env, false));
#endif

  /**
   * Details about the last error that occurred. The property
   * will be null or an object containing three properties: "number",
   * "message", and "type".
   *
   * @var {object} lastError
   */
  Napi::SetAccessor(target, Napi::String::New(env, "lastError"), LastErrorGetter, LastErrorSetter);

  /**
   * Should a warning be emitted to stderr when a synchronous operation
   * is blocking the event loop, can be safely disabled unless
   * the user application needs to remain responsive at all times
   * Use `(gdal as any).eventLoopWarning = false` to set the value from TypeScript
   *
   * @var {boolean} eventLoopWarning
   */
  Napi::SetAccessor(target, Napi::String::New(env, "eventLoopWarning"), EventLoopWarningGetter, EventLoopWarningSetter);

  // Napi::Object versions = Napi::Object::New(env);
  // (versions).Set(Napi::String::New(env, "node"),
  // Napi::New(env, NODE_VERSION+1)); (versions).Set(// Napi::String::New(env, "v8"), Napi::New(env, V8::GetVersion()));
  // (target).Set(Napi::String::New(env, "versions"), versions);

  /**
   * Disables all output.
   *
   * @static
   * @method quiet
   */
  exports.Set(Napi::String::New(env, "quiet"), Napi::Function::New(env, QuietOutput));

  /**
   * Displays extra debugging information from GDAL.
   *
   * @static
   * @method verbose
   */
  exports.Set(Napi::String::New(env, "verbose"), Napi::Function::New(env, VerboseOutput));

  exports.Set(Napi::String::New(env, "startLogging"), Napi::Function::New(env, StartLogging));
  exports.Set(Napi::String::New(env, "stopLogging"), Napi::Function::New(env, StopLogging));
  exports.Set(Napi::String::New(env, "log"), Napi::Function::New(env, Log));

  Napi::Object supports = Napi::Object::New(env);
  (target).Set(Napi::String::New(env, "supports"), supports);

  NODE_DEFINE_CONSTANT(target, CPLE_OpenFailed);
  NODE_DEFINE_CONSTANT(target, CPLE_IllegalArg);
  NODE_DEFINE_CONSTANT(target, CPLE_NotSupported);
  NODE_DEFINE_CONSTANT(target, CPLE_AssertionFailed);
  NODE_DEFINE_CONSTANT(target, CPLE_NoWriteAccess);
  NODE_DEFINE_CONSTANT(target, CPLE_UserInterrupt);

  auto *env = GetCurrentEnvironment(target->GetIsolate()->GetCurrentContext());
  AtExit(env, Cleanup, nullptr);
}
}

} // namespace node_gdal

NODE_API_MODULE(NODE_GYP_MODULE_NAME, node_gdal::Init);
