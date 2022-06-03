#include "gdal_memfile.hpp"

namespace node_gdal {

/**
 * File operations specific to in-memory `/vsimem/` files.
 *
 * @namespace vsimem
 */

std::map<void *, Memfile *> Memfile::memfile_collection;

Memfile::Memfile(void *data, const std::string &filename) : data(data), filename(filename) {
}

Memfile::Memfile(void *data) : data(data) {
  char _filename[32];
  // The pointer makes for a perfect unique filename
  snprintf(_filename, sizeof(_filename), "/vsimem/%p", data);
  this->filename = _filename;
}

Memfile::~Memfile() {
  delete persistent;
}

void Memfile::weakCallback(const Napi::WeakCallbackInfo<Memfile> &file) {
  Memfile *mem = file.GetParameter();
  memfile_collection.erase(mem->data);
  VSIUnlink(mem->filename.c_str());
  delete mem;
}

void Memfile::Initialize(Napi::Object target) {
  Napi::Object vsimem = Napi::Object::New(env);
  (target).Set(Napi::String::New(env, "vsimem"), vsimem);
  Napi::SetMethod(vsimem, "_anonymous", Memfile::vsimemAnonymous); // not a public API
  Napi::SetMethod(vsimem, "set", Memfile::vsimemSet);
  Napi::SetMethod(vsimem, "release", Memfile::vsimemRelease);
  Napi::SetMethod(vsimem, "copy", Memfile::vsimemCopy);
}

// Anonymous buffers are handled by the GC
// Whenever the JS buffer goes out of scope, the file is deleted
Memfile *Memfile::get(Napi::Object buffer) {
  if (!buffer.IsBuffer()) return nullptr;
  void *data = Buffer::Data(buffer);
  if (data == nullptr) return nullptr;
  if (memfile_collection.count(data)) return memfile_collection.find(data)->second;

  size_t len = Buffer::Length(buffer);
  Memfile *mem = nullptr;
  mem = new Memfile(data);

  VSILFILE *vsi = VSIFileFromMemBuffer(mem->filename.c_str(), (GByte *)data, len, 0);
  if (vsi == nullptr) return nullptr;
  VSIFCloseL(vsi);

  mem->persistent = new Napi::ObjectReference(buffer);
  mem->persistent->SetWeak(mem, weakCallback, Napi::WeakCallbackType::kParameter);
  memfile_collection[data] = mem;
  return mem;
}

// Named buffers are protected from the GC and are owned by Node
Memfile *Memfile::get(Napi::Object buffer, const std::string &filename) {
  if (!buffer.IsBuffer()) return nullptr;
  void *data = buffer.As<Napi::Buffer<char>>().Data();
  if (data == nullptr) { return nullptr; }

  size_t len = buffer.As<Napi::Buffer<char>>().Length();
  Memfile *mem = nullptr;
  mem = new Memfile(data, filename);

  VSILFILE *vsi = VSIFileFromMemBuffer(mem->filename.c_str(), (GByte *)data, len, 0);
  if (vsi == nullptr) return nullptr;
  VSIFCloseL(vsi);

  mem->persistent = new Napi::ObjectReference(buffer);
  memfile_collection[data] = mem;
  return mem;
}

// GDAL buffers handled by GDAL and are not referenced by node-gdal-async
bool Memfile::copy(Napi::Object buffer, const std::string &filename) {
  if (!buffer.IsBuffer()) return false;
  void *data = buffer.As<Napi::Buffer<char>>().Data();
  if (data == nullptr) return false;

  size_t len = buffer.As<Napi::Buffer<char>>().Length();

  void *dataCopy = CPLMalloc(len);
  if (dataCopy == nullptr) return false;

  // If you malloc, you adjust external memory too (https://github.com/nodejs/node/issues/40936)
  Napi::AdjustExternalMemory(len);
  memcpy(dataCopy, data, len);

  VSILFILE *vsi = VSIFileFromMemBuffer(filename.c_str(), (GByte *)dataCopy, len, 1);
  if (vsi == nullptr) {
    CPLFree(dataCopy);
    return false;
  }
  VSIFCloseL(vsi);
  return true;
}

/**
 * Create an in-memory `/vsimem/` file from a `Buffer`.
 * This is a zero-copy operation - GDAL will read from the Buffer which will be
 * protected by the GC even if it goes out of scope.
 *
 * The file will stay in memory until it is deleted with `gdal.vsimem.release`.
 *
 * The file will be in read-write mode, but GDAL won't
 * be able to extend it as the allocated memory will be tied to the `Buffer` object.
 * Use `gdal.vsimem.copy` to create an extendable copy.
 *
 * @static
 * @method set
 * @memberof vsimem
 * @throws {Error}
 * @param {Buffer} data A binary buffer containing the file data
 * @param {string} filename A file name beginning with `/vsimem/`
 */
Napi::Value Memfile::vsimemSet(const Napi::CallbackInfo& info) {
  Napi::Object buffer;
  std::string filename;

  NODE_ARG_OBJECT(0, "buffer", buffer);
  NODE_ARG_STR(1, "filename", filename);

  Memfile *memfile = Memfile::get(buffer, filename);
  if (memfile == nullptr) Napi::Error::New(env, "Failed creating in-memory file").ThrowAsJavaScriptException();

}

/**
 * Create an in-memory `/vsimem/` file copying a `Buffer`.
 * This method copies the `Buffer` into GDAL's own memory heap
 * creating an in-memory file that can be freely extended by GDAL.
 * `gdal.vsimem.set` is the better choice unless the file needs to be extended.
 *
 * The file will stay in memory until it is deleted with `gdal.vsimem.release`.
 *
 * @static
 * @method copy
 * @memberof vsimem
 * @throws {Error}
 * @param {Buffer} data A binary buffer containing the file data
 * @param {string} filename A file name beginning with `/vsimem/`
 */
Napi::Value Memfile::vsimemCopy(const Napi::CallbackInfo& info) {
  Napi::Object buffer;
  std::string filename;

  NODE_ARG_OBJECT(0, "buffer", buffer);
  NODE_ARG_STR(1, "filename", filename);

  if (!Memfile::copy(buffer, filename)) Napi::Error::New(env, "Failed creating in-memory file").ThrowAsJavaScriptException();

}

/*
 * This creates an anonymous vsimem file from a Buffer.
 * It is automatically deleted when the Buffer goes out of scope.
 * This is not a public method as it is not always safe.
 */
Napi::Value Memfile::vsimemAnonymous(const Napi::CallbackInfo& info) {
  Napi::Object buffer;

  NODE_ARG_OBJECT(0, "buffer", buffer);

  Memfile *memfile = Memfile::get(buffer);
  if (memfile == nullptr)
    Napi::Error::New(env, "Failed creating in-memory file").ThrowAsJavaScriptException();

  else
    return Napi::String::New(env, memfile->filename);
}

/**
 * Delete and retrieve the contents of an in-memory `/vsimem/` file.
 * This is a very fast zero-copy operation.
 * It does not block the event loop.
 * If the file was created by `vsimem.set`, it will return a reference
 * to the same `Buffer` that was used to create it.
 * Otherwise it will construct a new `Buffer` object with the GDAL
 * allocated buffer as its backing store.
 *
 * ***WARNING***!
 *
 * The file must not be open or random memory corruption is possible with GDAL <= 3.3.1.
 * GDAL >= 3.3.2 will gracefully fail further operations and this function will always be safe.
 *
 * @static
 * @method release
 * @memberof vsimem
 * @param {string} filename A file name beginning with `/vsimem/`
 * @throws {Error}
 * @return {Buffer} A binary buffer containing all the data
 */
Napi::Value Memfile::vsimemRelease(const Napi::CallbackInfo& info) {
  vsi_l_offset len;
  std::string filename;
  NODE_ARG_STR(0, "filename", filename);

  CPLErrorReset();
  void *data = VSIGetMemFileBuffer(filename.c_str(), &len, false);
  if (data == nullptr) {
    NODE_THROW_LAST_CPLERR;
    return;
  }

  // Two cases:
  if (memfile_collection.count(data)) {
    // the file comes from a named buffer and the buffer is owned by Node
    // -> a reference to the existing buffer is returned
    Memfile *mem = memfile_collection.find(data)->second;
    memfile_collection.erase(mem->data);
    VSIUnlink(mem->filename.c_str());
    return Napi::New(env, *mem->persistent);
    delete mem;
  } else {
    // the file has been created by GDAL and the buffer is owned by GDAL
    // -> a new Buffer is constructed and GDAL has to relinquish control
    // The GC will call the lambda at some point to free the backing storage
    VSIGetMemFileBuffer(filename.c_str(), &len, true);
    // Alas we can't take the address of a capturing lambda
    // so we fall back to doing this like it was back in the day
    int *hint = new int{static_cast<int>(len)};
    return Napi::Buffer<char>::New(env, 
                                static_cast<char *>(data),
                                static_cast<size_t>(len),
                                [](char *data, void *hint) {
                                  int *len = reinterpret_cast<int *>(hint;
                                  Napi::AdjustExternalMemory(-(*len));
                                  delete len;
                                  CPLFree(data);
                                },
                                hint)
                                );
  }
}

} // namespace node_gdal
