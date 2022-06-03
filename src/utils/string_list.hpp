
#ifndef __STRING_LIST_H__
#define __STRING_LIST_H__

// node
#include <napi.h>
#include <uv.h>

// nan
#include <napi.h>

using namespace Napi;

namespace node_gdal {

// A class for parsing a V8::Value and constructing a GDAL string list
//
// inputs:
// {key: value, ...}, ["key=value", ...]
//
// outputs:
// ["key=value", ...]

class StringList {
    public:
  int parse(Napi::Value value);

  StringList();
  ~StringList();

  inline char **get() {
    return list;
  }

    private:
  char **list;
  std::string *strlist;
};

} // namespace node_gdal

#endif
