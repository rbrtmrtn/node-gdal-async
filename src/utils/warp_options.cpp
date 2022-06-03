#include "warp_options.hpp"
#include "../gdal_common.hpp"
#include "../geometry/gdal_geometry.hpp"
#include <stdio.h>
namespace node_gdal {

WarpOptions::WarpOptions()
  : options(NULL),
    src(nullptr),
    dst(nullptr),
    additional_options(),
    src_bands("src band ids"),
    dst_bands("dst band ids"),
    src_nodata(NULL),
    dst_nodata(NULL),
    multi(false) {
  options = GDALCreateWarpOptions();
}

WarpOptions::~WarpOptions() {

  // Dont use: GDALDestroyWarpOptions( options ); - it assumes ownership of
  // everything
  if (options) CPLFree(options);
  if (src_nodata) delete src_nodata;
  if (dst_nodata) delete dst_nodata;
}

int WarpOptions::parseResamplingAlg(Napi::Value value) {
  Napi::Env env = value.Env();
  if (value.IsNull() || value.IsNull()) {
    options->eResampleAlg = GRA_NearestNeighbour;
    return 0;
  }
  if (!value.IsString()) {
    Napi::TypeError::New(env, "resampling property must be a string").ThrowAsJavaScriptException();

    return 1;
  }
  std::string name = value.As<Napi::String>().Utf8Value().c_str();

  if (name == "NearestNeighbor") {
    options->eResampleAlg = GRA_NearestNeighbour;
    return 0;
  }
  if (name == "NearestNeighbour") {
    options->eResampleAlg = GRA_NearestNeighbour;
    return 0;
  }
  if (name == "Bilinear") {
    options->eResampleAlg = GRA_Bilinear;
    return 0;
  }
  if (name == "Cubic") {
    options->eResampleAlg = GRA_Cubic;
    return 0;
  }
  if (name == "CubicSpline") {
    options->eResampleAlg = GRA_CubicSpline;
    return 0;
  }
  if (name == "Lanczos") {
    options->eResampleAlg = GRA_Lanczos;
    return 0;
  }
  if (name == "Average") {
    options->eResampleAlg = GRA_Average;
    return 0;
  }
  if (name == "Mode") {
    options->eResampleAlg = GRA_Mode;
    return 0;
  }

  Napi::Error::New(env, "Invalid resampling algorithm").ThrowAsJavaScriptException();

  return 1;
}

/*
 * {
 *   options : string[] | object
 *   memoryLimit : int
 *   resampleAlg : string
 *   src: Dataset
 *   dst: Dataset
 *   srcBands: int | int[]
 *   dstBands: int | int[]
 *   nBands: int
 *   srcAlphaBand: int
 *   dstAlphaBand: int
 *   srcNoData: double
 *   dstNoData: double
 *   cutline: geometry
 *   blend: double
 * }
 */
int WarpOptions::parse(Napi::Value value) {
  Napi::Env env = value.Env();
  Napi::HandleScope scope(env);

  if (!value.IsObject() || value.IsNull())
    Napi::TypeError::New(env, "Warp options must be an object").ThrowAsJavaScriptException();

  Napi::Object obj = value.As<Napi::Object>();
  Napi::Value prop;

  if (obj.HasOwnProperty("options") && additional_options.parse((obj).Get(Napi::String::New(env, "options")))) {
    return 1; // error parsing string list
  }

  options->papszWarpOptions = additional_options.get();

  if (obj.HasOwnProperty("memoryLimit")) {
    prop = (obj).Get(Napi::String::New(env, "memoryLimit"));
    if (prop.IsNumber()) {
      options->dfWarpMemoryLimit = prop.As<Napi::Number>().Int32Value();
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "memoryLimit property must be an integer").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("resampling")) {
    prop = (obj).Get(Napi::String::New(env, "resampling"));
    if (parseResamplingAlg(prop)) {
      return 1; // error parsing resampling algorithm
    }
  }
  if (obj.HasOwnProperty("src")) {
    prop = (obj).Get(Napi::String::New(env, "src"));
    if (prop.IsObject() && !prop.IsNull() && prop.ToObject().InstanceOf(Dataset::constructor.Value())) {
      this->src_obj = prop.As<Napi::Object>();
      this->src = Dataset::Unwrap(this->src_obj);
#if GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 3
      options->hSrcDS = static_cast<GDALDatasetH>(this->src->get());
#else
      options->hSrcDS = GDALDataset::ToHandle(this->src->get());
#endif
      if (!options->hSrcDS) {
        Napi::Error::New(env, "src dataset already closed").ThrowAsJavaScriptException();

        return 1;
      }
    } else {
      Napi::TypeError::New(env, "src property must be a Dataset object").ThrowAsJavaScriptException();

      return 1;
    }
  } else {
    Napi::Error::New(env, "Warp options must include a source dataset").ThrowAsJavaScriptException();

    return 1;
  }
  if (obj.HasOwnProperty("dst")) {
    prop = (obj).Get(Napi::String::New(env, "dst"));
    if (prop.IsObject() && !prop.IsNull() && prop.ToObject().InstanceOf(Dataset::constructor.Value())) {
      this->dst_obj = prop.As<Napi::Object>();
      this->dst = Dataset::Unwrap(this->dst_obj);
#if GDAL_VERSION_MAJOR == 2 && GDAL_VERSION_MINOR < 3
      options->hDstDS = static_cast<GDALDatasetH>(this->dst->get());
#else
      options->hDstDS = GDALDataset::ToHandle(this->dst->get());
#endif
      if (!options->hDstDS) {
        Napi::Error::New(env, "dst dataset already closed").ThrowAsJavaScriptException();

        return 1;
      }
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "dst property must be a Dataset object").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("srcBands")) {
    prop = (obj).Get(Napi::String::New(env, "srcBands"));
    if (src_bands.parse(prop)) {
      return 1; // error parsing number list
    }
    options->panSrcBands = src_bands.get();
    options->nBandCount = src_bands.length();
  }
  if (obj.HasOwnProperty("dstBands")) {
    prop = (obj).Get(Napi::String::New(env, "dstBands"));
    if (dst_bands.parse(prop)) {
      return 1; // error parsing number list
    }
    options->panDstBands = dst_bands.get();

    if (!options->panSrcBands) {
      Napi::Error::New(env, "srcBands must be provided if dstBands option is used").ThrowAsJavaScriptException();

      return 1;
    }
    if (dst_bands.length() != options->nBandCount) {
      Napi::Error::New(env, "Number of dst bands must equal number of src bands").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (options->panSrcBands && !options->panDstBands) {
    Napi::Error::New(env, "dstBands must be provided if srcBands option is used").ThrowAsJavaScriptException();

    return 1;
  }
  if (obj.HasOwnProperty("srcNodata")) {
    prop = (obj).Get(Napi::String::New(env, "srcNodata"));
    if (prop.IsNumber()) {
      src_nodata = new double(prop.As<Napi::Number>().DoubleValue());
      options->padfSrcNoDataReal = src_nodata;
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "srcNodata property must be a number").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("dstNodata")) {
    prop = (obj).Get(Napi::String::New(env, "dstNodata"));
    if (prop.IsNumber()) {
      dst_nodata = new double(prop.As<Napi::Number>().DoubleValue());
      options->padfDstNoDataReal = dst_nodata;
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "dstNodata property must be a number").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("srcAlphaBand")) {
    prop = (obj).Get(Napi::String::New(env, "srcAlphaBand"));
    if (prop.IsNumber()) {
      options->nSrcAlphaBand = prop.As<Napi::Number>().Int32Value();
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "srcAlphaBand property must be an integer").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("dstAlphaBand")) {
    prop = (obj).Get(Napi::String::New(env, "dstAlphaBand"));
    if (prop.IsNumber()) {
      options->nDstAlphaBand = prop.As<Napi::Number>().Int32Value();
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "dstAlphaBand property must be an integer").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("blend")) {
    prop = (obj).Get(Napi::String::New(env, "blend"));
    if (prop.IsNumber()) {
      options->dfCutlineBlendDist = prop.As<Napi::Number>().DoubleValue();
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "cutline blend distance must be a number").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("cutline")) {
    prop = (obj).Get(Napi::String::New(env, "cutline"));
    if (prop.IsObject() && !prop.IsNull() && prop.ToObject().InstanceOf(Geometry::constructor.Value())) {
      options->hCutline = (Geometry::Unwrap(prop.ToObject())).get();
    } else if (!prop.IsNull() && !prop.IsNull()) {
      Napi::TypeError::New(env, "cutline property must be a Geometry object").ThrowAsJavaScriptException();

      return 1;
    }
  }
  if (obj.HasOwnProperty("multi")) {
    prop = obj.Get("multi");
    if (!prop.IsBoolean()) {
      Napi::TypeError::New(env, "multi must be a Boolean").ThrowAsJavaScriptException();

      return 1;
    }
    if (prop.ToBoolean()) { multi = true; }
  }
  return 0;
}

} // namespace node_gdal
