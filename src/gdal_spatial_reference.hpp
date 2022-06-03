#ifndef __NODE_OGR_SPATIALREFERENCE_H__
#define __NODE_OGR_SPATIALREFERENCE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "async.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

#if GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 3
typedef char OGRChar;
#else
typedef const char OGRChar;
#endif

class SpatialReference : public Napi::ObjectWrap<SpatialReference> {
    public:
  static Napi::FunctionReference constructor;
  static void Initialize(Napi::Object target);

  static Napi::Value New(const Napi::CallbackInfo &info);
  static Napi::Value New(OGRSpatialReference *srs);
  static Napi::Value New(OGRSpatialReference *srs, bool owned);
  static Napi::Value toString(const Napi::CallbackInfo &info);
  static Napi::Value clone(const Napi::CallbackInfo &info);
  static Napi::Value cloneGeogCS(const Napi::CallbackInfo &info);
  static Napi::Value exportToWKT(const Napi::CallbackInfo &info);
  static Napi::Value exportToPrettyWKT(const Napi::CallbackInfo &info);
  static Napi::Value exportToProj4(const Napi::CallbackInfo &info);
  static Napi::Value exportToXML(const Napi::CallbackInfo &info);
  static Napi::Value setWellKnownGeogCS(const Napi::CallbackInfo &info);
  static Napi::Value morphToESRI(const Napi::CallbackInfo &info);
  static Napi::Value morphFromESRI(const Napi::CallbackInfo &info);
  static Napi::Value EPSGTreatsAsLatLong(const Napi::CallbackInfo &info);
  static Napi::Value EPSGTreatsAsNorthingEasting(const Napi::CallbackInfo &info);
  static Napi::Value getLinearUnits(const Napi::CallbackInfo &info);
  static Napi::Value getAngularUnits(const Napi::CallbackInfo &info);
  static Napi::Value isGeocentric(const Napi::CallbackInfo &info);
  static Napi::Value isGeographic(const Napi::CallbackInfo &info);
  static Napi::Value isProjected(const Napi::CallbackInfo &info);
  static Napi::Value isLocal(const Napi::CallbackInfo &info);
  static Napi::Value isVertical(const Napi::CallbackInfo &info);
  static Napi::Value isCompound(const Napi::CallbackInfo &info);
  static Napi::Value isSameGeogCS(const Napi::CallbackInfo &info);
  static Napi::Value isSameVertCS(const Napi::CallbackInfo &info);
  static Napi::Value isSame(const Napi::CallbackInfo &info);
  static Napi::Value autoIdentifyEPSG(const Napi::CallbackInfo &info);
  static Napi::Value getAuthorityCode(const Napi::CallbackInfo &info);
  static Napi::Value getAuthorityName(const Napi::CallbackInfo &info);
  static Napi::Value getAttrValue(const Napi::CallbackInfo &info);
  static Napi::Value validate(const Napi::CallbackInfo &info);

  GDAL_ASYNCABLE_DECLARE(fromUserInput);
  static Napi::Value fromWKT(const Napi::CallbackInfo &info);
  static Napi::Value fromProj4(const Napi::CallbackInfo &info);
  static Napi::Value fromEPSG(const Napi::CallbackInfo &info);
  static Napi::Value fromEPSGA(const Napi::CallbackInfo &info);
  static Napi::Value fromESRI(const Napi::CallbackInfo &info);
  static Napi::Value fromWMSAUTO(const Napi::CallbackInfo &info);
  static Napi::Value fromXML(const Napi::CallbackInfo &info);
  static Napi::Value fromURN(const Napi::CallbackInfo &info);
  GDAL_ASYNCABLE_DECLARE(fromCRSURL);
  GDAL_ASYNCABLE_DECLARE(fromURL);
  static Napi::Value fromMICoordSys(const Napi::CallbackInfo &info);

  SpatialReference();
  SpatialReference(OGRSpatialReference *srs);
  inline OGRSpatialReference *get() {
    return this_;
  }
  inline bool isAlive() {
    return this_;
  }
  void dispose();
  long uid;

    private:
  ~SpatialReference();
  OGRSpatialReference *this_;
  bool owned_;
};

} // namespace node_gdal
#endif
