#ifndef __NODE_GDAL_MAJOROBJECT_H__
#define __NODE_GDAL_MAJOROBJECT_H__

// node
#include <napi.h>
#include <uv.h>
#include <node_object_wrap.h>

// nan
#include <napi.h>

// gdal
#include <gdal_priv.h>

using namespace Napi;
using namespace Napi;

namespace node_gdal {

class MajorObject {
    public:
  static Napi::Object getMetadata(char **metadata);
};

} // namespace node_gdal
#endif
