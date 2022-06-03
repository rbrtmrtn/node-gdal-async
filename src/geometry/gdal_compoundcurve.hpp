#ifndef __NODE_OGR_COMPOUNDCURVE_H__
#define __NODE_OGR_COMPOUNDCURVE_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// ogr
#include <ogrsf_frmts.h>

#include "gdal_curvebase.hpp"
#include "../collections/compound_curves.hpp"

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class CompoundCurve : public CurveBase<CompoundCurve, OGRCompoundCurve, CompoundCurveCurves> {
  friend CurveBase;

    public:
  static Napi::FunctionReference constructor;
  using CurveBase<CompoundCurve, OGRCompoundCurve, CompoundCurveCurves>::CurveBase;

  static void Initialize(Napi::Object target);
  using CurveBase<CompoundCurve, OGRCompoundCurve, CompoundCurveCurves>::New;
  static Napi::Value toString(const Napi::CallbackInfo &info);

  Napi::Value curvesGetter(const Napi::CallbackInfo &info);

    protected:
  static void SetPrivate(Napi::ADDON_REGISTER_FUNCTION_ARGS_TYPE, Napi::Value);
};

} // namespace node_gdal
#endif
