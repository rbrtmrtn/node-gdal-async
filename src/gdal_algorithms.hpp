#ifndef __GDAL_ALGORITHMS_H__
#define __GDAL_ALGORITHMS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_alg.h>
#include <gdal_priv.h>

// ogr
#include <ogrsf_frmts.h>

#include "async.hpp"

using namespace Napi;
using namespace Napi;

// Methods from gdal_alg.h
// https://gdal.org/doxygen/gdal__alg_8h.html

namespace node_gdal {
namespace Algorithms {

void Initialize(Napi::Object target);

GDAL_ASYNCABLE_GLOBAL(fillNodata);
GDAL_ASYNCABLE_GLOBAL(contourGenerate);
GDAL_ASYNCABLE_GLOBAL(sieveFilter);
GDAL_ASYNCABLE_GLOBAL(checksumImage);
GDAL_ASYNCABLE_GLOBAL(polygonize);
Napi::Value addPixelFunc(const Napi::CallbackInfo &info);
Napi::Value toPixelFunc(const Napi::CallbackInfo &info);
GDAL_ASYNCABLE_GLOBAL(_acquireLocks);
} // namespace Algorithms
} // namespace node_gdal

#endif
