#ifndef __GDAL_UTILS_H__
#define __GDAL_UTILS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_alg.h>
#include <gdal_priv.h>
#include <gdal_utils.h>

// ogr
#include <ogrsf_frmts.h>

#include "async.hpp"

using namespace Napi;
using namespace Napi;

// Methods and classes from gdal_utils.h
// https://gdal.org/development/rfc/rfc59.1_utilities_as_a_library.html

namespace node_gdal {
namespace Utils {

void Initialize(Napi::Object target);

GDAL_ASYNCABLE_GLOBAL(info);
GDAL_ASYNCABLE_GLOBAL(translate);
GDAL_ASYNCABLE_GLOBAL(vectorTranslate);
GDAL_ASYNCABLE_GLOBAL(warp);
GDAL_ASYNCABLE_GLOBAL(buildvrt);
GDAL_ASYNCABLE_GLOBAL(rasterize);

} // namespace Utils
} // namespace node_gdal

#endif
