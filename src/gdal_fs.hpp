#ifndef __NODE_GDAL_VSIFS_H__
#define __NODE_GDAL_VSIFS_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_buffer.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

#include "gdal_common.hpp"

#include "async.hpp"

using namespace Napi;
using namespace Napi;

// A vsimem file

namespace node_gdal {

namespace VSI {

void Initialize(Napi::Object target);
GDAL_ASYNCABLE_GLOBAL(stat);
GDAL_ASYNCABLE_GLOBAL(readDir);

} // namespace VSI
} // namespace node_gdal
#endif
