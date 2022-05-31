# node-gdal-async

![logo](https://raw.githubusercontent.com/mmomtchev/node-gdal-async/master/gdal-async-logo.svg)

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![npm version](https://img.shields.io/npm/v/gdal-async)](https://www.npmjs.com/package/gdal-async)
[![Test release](https://github.com/mmomtchev/node-gdal-async/actions/workflows/test-release.yml/badge.svg)](https://github.com/mmomtchev/node-gdal-async/actions/workflows/test-release.yml)
[![Test npm package](https://github.com/mmomtchev/node-gdal-async/actions/workflows/test-npm.yml/badge.svg)](https://github.com/mmomtchev/node-gdal-async/actions/workflows/test-npm.yml)
![Node.js CI](https://github.com/mmomtchev/node-gdal-async/workflows/Node.js%20CI/badge.svg)
[![codecov](https://codecov.io/gh/mmomtchev/node-gdal-async/branch/master/graph/badge.svg?token=FE9IJUZ07D)](https://codecov.io/gh/mmomtchev/node-gdal-async)
[![downloads](https://img.shields.io/npm/dt/gdal-async)](https://www.npmjs.com/package/gdal-async)

Read and write raster and vector geospatial datasets straight from [Node.js](http://nodejs.org) with this native asynchronous [GDAL](http://www.gdal.org/) binding. To get started, browse the [**API Documentation**](https://mmomtchev.github.io/node-gdal-async/classes/gdal.html) or [examples](examples/).

When in doubt on how to use a method, check also the [unit tests](https://github.com/mmomtchev/node-gdal-async/tree/master/test).

## Fork notes

This project is a fork of <https://github.com/contra/node-gdal-next> which is a fork of <https://github.com/naturalatlas/node-gdal>.

It adds a number of features:

- **Support for asynchronous IO** (since 3.0)
- **Full native TypeScript support** with autocompletion in IDEs that support it (since 3.1)
- **Built-in networking support** with native support for cloud services (since 3.2)
- **scijs/ndarray integration** with zero-copy I/O (since 3.3)
- **Read-only support for the new Multidimensional Raster Data Model of GDAL 3.1** (since 3.3)
- **Library versions of GDAL's command-line utilities `gdalinfo`, `gdal_translate`, `ogr2ogr` and `gdalwarp`** (since 3.4)
- **Node.js Streams API for raster datasets** (since 3.4)
- **An alternative `gdal_calc.py` implementation supporting JS functions** (since 3.4)
- **`ExprTk.js` integration for truly multi-threaded async computing** (since 3.5)
- **VRT pixel functions support - both with JS functions and ExprTk expressions** (since 3.5)
- Support for curve geometries (since 3.1)
- Progress callbacks (since 3.2)
- Built-in support for HDF5, NetCDF, GRIB, WMS, WMTS, WCS and FlatGeobuf (since 3.3)
- Numerous bugfixes including a number of memory leaks

Support for `worker_threads` is planned but it is not a priority project

## Installation

Pre-built binaries are provided for most recent Linux distributions, Windows 64 bit and OS X 10.15:

```sh
npm install gdal-async
```

### Linking with your own version of GDAL

By default all dependencies are the latest versions and bundled out of the box, but if you would like to link against a custom GDAL library you will have to rebuild it when installing using the following flags:

```sh
# --shared_gdal allows linking to the OS-provided libgdal, requires libgdal-dev
# (debian: sudo apt-get install libgdal-dev)
$ npm install gdal-async --build-from-source --shared_gdal
```

If you have multiple versions of GDAL or your library is not in its standard location, you can use the usual `CXXFLAGS` and `LDFLAGS` variables:
```sh
$ CXXFLAGS="-I/some/path/include" LDFLAGS="-L/some/path/lib" npm install gdal-async --build-from-source --shared_gdal
```

## Sample usage

### Synchronous

#### Raster

```js
const gdal = require("gdal-async")
const dataset = gdal.open("sample.tif")

console.log("number of bands: " + dataset.bands.count())
console.log("width: " + dataset.rasterSize.x)
console.log("height: " + dataset.rasterSize.y)
console.log("geotransform: " + dataset.geoTransform)
console.log("srs: " + (dataset.srs ? dataset.srs.toWKT() : 'null'))
```

#### Vector

```js
const gdal = require("gdal-async")
const dataset = gdal.open("sample.shp")
const layer = dataset.layers.get(0)

console.log("number of features: " + layer.features.count())
console.log("fields: " + layer.fields.getNames())
console.log("extent: " + JSON.stringify(layer.extent))
console.log("srs: " + (layer.srs ? layer.srs.toWKT() : 'null'))
```

### Asynchronous

Mixing of synchronous and asynchronous operations is supported.

#### Mixing of synchronous and asynchronous operations

Simultaneous operations on distinct dataset objects are always safe and can run it parallel.

Simultaneous operations on the same dataset object are safe too but they won't run in parallel. This is a limitation of GDAL. The only way to have multiple parallel operations on the same file is to use multiple dataset objects. Keep in mind that Node.js/libuv won't be able to detect which async contexts are waiting on each other, so if you launch 16 simultaneous operations on 4 different datasets, there is always a chance that libuv will pick 4 operations on the same dataset to run - which will take all 4 slots on the thread pool. It is recommended to either increase `UV_THREADPOOL_SIZE` or to make sure that every dataset has exactly one operation running at any given time. Take a look at `ASYNCIO.md` which explains this in detail.

Also be particularly careful when mixing synchronous and asynchronous operations in server code. If a GDAL operation is running in the background for any given Dataset, all synchronous operations on that same Dataset on the main thread will block the event loop until the background operation is finished. **This includes synchronous getters and setters that might otherwise be instantaneous.**. It is recommended to retrieve all values such as raster size or no data value or spatial reference **before** starting any I/O operations or use the new asynchronous getters available in 3.3.2 and later.

**Does not support `worker_threads` yet**

**The HDF5 driver is not thread safe on Windows**

#### With callbacks

If the last argument of an `xxxAsync` function is a callback,
it will be called on completion with standard *(e,r)* semantics

In this case the function will return a resolved *Promise*
```js
const gdal = require('gdal-async');
gdal.openAsync('sample.tif', (e, dataset) => {
        if (e) {
            console.error(e);
            return;
        }
        dataset.bands.get(1).pixels.readAsync(0, 0, dataset.rasterSize.x,
        dataset.rasterSize.y, (e, data) => {
            if (e) {
                console.error(e);
                return;
            }
            console.log(data);
        });
});
```

#### With promises

If there is no callback, the function will return a *Promise*

```js
const gdal = require('gdal-async')
gdal.openAsync('sample.tif')
    .then((dataset) => dataset.bands.getAsync(1))
    .then((band) => band.pixels.readAsync(0, 0, band.ds.rasterSize.x, band.ds.rasterSize.y))
    .then((data) => console.log(data))
    .catch((e) => console.error(e))
```

### With async/await

```js
const gdal = require('gdal-async');
try {
    const ds = await gdal.openAsync('sample.tif');
    const band = await ds.bands.getAsync(1);
    const rasterSize = await ds.rasterSizeAsync;    // starting with 3.3.2
    const data = await band.pixels.readAsync(0, 0, rasterSize.x, rasterSize.y);
} catch (e => console.error(e));
```

### TypeScript (starting from 3.1)

TypeScript support is available beginning with `gdal-async@3.1.0`
```ts
import * as gdal from 'gdal-async'

const ds1: gdal.Dataset = gdal.open('sample.tif')
const ds2: Promise<gdal.Dataset> = gdal.openAsync('sample.tif')
```
In TypeScript `gdal` is a collection of named exports - as this is the preferred way of importing a native module in TypeScript and it does not impose the use of the `esModuleInterop` flag.

### MJS (Node.js ES6 modules)

When importing from a so-called Michael Jackson Script environment (Node.js native ES6 modules), the correct import syntax is:
```js
import gdal from 'gdal-async'
```
In MJS `gdal` is a default export - as this allows for maximum compatibility with existing CJS applications.

### Built-in networking (starting from 3.2)

Built-in networking uses an embedded version of `libcurl`. It supports `zlib` compression through Node.js' own `zlib` support. It does not support `brotli` or `zstd`. Node.js includes `brotli`, but as of Node.js 16 it still does not export these symbols for use by add-ons (yes, go bug them - ask them for c-ares too).
SSL on Linux uses OpenSSL through Node.js' own support. It uses the curl trusted root CA store by default and another store can be provided through the `CURL_CA_BUNDLE` environment variable or GDAL config option. SSL on Windows and OSX uses the OS-provided mechanisms - Schannel and SecureTransport respectively - and thus the trusted root CA store will be the one provided by the OS.

### Breaking changes relative to node-gdal / node-gdal-next

 * If you are coming from `node-gdal-next`
    - Starting with 3.2
        * `gdal.Geometry.fromGeoJson` now throws an `Error` on error instead of returning a null geometry
    - Starting with 3.3
        * Drop support for linking against a shared GDAL 1.x library
        * Drop support for Node.js 10.x and 15.x
        * Linux uses lazy-binding meaning that parts of the binary won't be loaded in memory until the first function call
        * `gdal.DatasetBands.get{Async}()` now throws an `Error` if an invalid band is requested instead of returning a null object
        * `gdal.DatasetLayers.get{Async}()` now throws an `Error` if an invalid layer is requested instead of returning a null object
        * `gdal.FeatureDefnFields.get{Async}()` now throws an `Error` if an invalid field definition is requested instead of returning a null object
        * `gdal.FeatureFields.get{Async}()` now throws an `Error` if an invalid field is requested instead of returning a null object
          - it can still return `null` if the field exists in the definition but it is not set in the feature
        * `gdal.GDALDrivers.get()` now throws an `Error` if an invalid driver is requested instead of returning a null object
        * `gdal.GeometryCollectionChildren.get()` now throws an `Error` if an invalid sub-geometry is requested instead of returning a null object
        * `gdal.LayerFeatures.get{Async}()` now throws an `Error` if an invalid feature is requested instead of returning a null object
        * `gdal.LayerFields.get{Async}()` now throws an `Error` if an invalid field is requested instead of returning a null object
        * `gdal.LineStringPoints.get()` now throws an `Error` if an invalid point is requested instead of returning a null object
        * `gdal.PolygonRings.get()` now throws an `Error` if an invalid ring is requested instead of returning a null object
        * All `gdal.Geometry` methods that return a `gdal.Geometry` throw an `Error` on error instead of returning a null object
    - Starting with 3.4
        * GDAL >= 2.1 is required
        * All platforms use lazy-binding meaning that parts of the binary won't be loaded in memory until the first function call
        * `gdal.RasterBand.noDataValue` now returns `NaN` if the original value was `NaN` instead of null; when setting it to `null` it correctly clears the `NoDataValue` instead of setting it to `NaN`; one should still exercise care when using `NaN` as a `NoDataValue` - since even if the binary representation is defined by IEEE 754, different formats, compilers or architectures may exhibit different behavior
        * TypeScript bindings now internally use `strict: true` mode with many getters and functions receiving an additional explicit `null` return type, consider using `strictNullChecks: false` in your `tsconfig.json` if you need to suppress these new errors in existing code

 * If you are coming from `node-gdal`, in addition to all of the above
    - With PROJ 6+, the order of coordinates for EPSG geographic coordinate reference systems is latitude first,
    longitude second. If you don't want to make large code changes, you can replace code like `gdal.SpatialReference.fromEPSG(4326)` with `gdal.SpatialReference.fromProj4('+init=epsg:4326')`

### With `ndarray` from `scijs`

A separate plugin [ndarray-gdal](https://github.com/mmomtchev/ndarray-gdal) allows zero-copy I/O, with GDAL-backed interleaving in C++ using SIMD instructions, for all possible strides both for 2D raster data and N-dimensional `MDArray` data. The plugin requires at least `gdal-async@3.3`.

### With `ExprTk.js`

A separate plugin [gdal-exprtk](https://github.com/mmomtchev/gdal-exprtk) adds seamless integration with [ExprTk.js](https://github.com/mmomtchev/ExprTk.js) allowing for truly multi-threaded and asynchronous computing with raster datasets. The plugin requires at least `gdal-async@3.5`.

### Pruning the source tree

A special NPM script target, `npm run prune` allows to delete most of the bundled source packages - these are not needed unless rebuilding from source. It significantly reduces the overall size of this module.

### Using in Amazon Linux Lambdas

Amazon Linux will be supported starting from `gdal-async@3.3.4`. There is no shared GDAL support, only the bundled GDAL version is supported. It is highly recommended that you prune the module before deploying to an Amazon Lambda - when pruned the module is only 16MB zipped.

## Known issues

* [#2](https://github.com/mmomtchev/node-gdal-async/issues/2) When running multiple parallel async operations per `Dataset` and on multiple `Dataset`s, thread starvation is possible as explained in [`ASYNCIO.md`](https://github.com/mmomtchev/node-gdal-async/blob/master/ASYNCIO.md)
* [#11](https://github.com/mmomtchev/node-gdal-async/issues/11) Accessing a closed `Dataset` object results in a warning with a stack trace printed out to stdout
* [#17](https://github.com/mmomtchev/node-gdal-async/issues/17) HDF5 on Windows is not thread-safe

## Bundled Drivers

When using the bundled GDAL version, the following drivers will be available:
`AAIGrid`, `ACE2`, `ADRG`, `AIG`, `AVCBin`, `AVCE00`, `AirSAR`, `BLX`, `BMP`, `BT`, `carto`, `CEOS`, `COASP`, `COSAR`, `CPG`, `CSV`, `CTG`, `CTable2`, `DGN`, `DIMAP`, `DIPEx`, `DOQ1`, `DOQ2`, `DTED`, `DXF`, `ECRGTOC`, `EDIGEO`, `EHdr`, `EIR`, `ELAS`, `ENVI`, `ERS`, `ESAT`, `ESRI Shapefile`, `MapInfo File`, `MBTiles`, `FAST`, `FIT`, `FlatGeobuf`, `GFF`, `GML`, `GPSBabel`, `GPSTrackMaker`, `GPX`, `GRASSASCIIGrid`, `GRIB`, `GS7BG`, `GSAG`, `GSBG`, `GSC`, `GTX`, `GTiff`, `GenBin`, `GeoJSON`, `GeoRSS`, `Geoconcept`, `GPKG`, `HDF5`, `HF2`, `HFA`, `ILWIS`, `IRIS`, `ISIS2`, `ISIS3`, `Idrisi`, `JAXAPALSAR`, `JDEM`, `JPEG`, `KMLSUPEROVERLAY`, `KML`, `KRO`, `L1B`, `LAN`, `LCP`, `LOSLAS`, `Leveller`, `MAP`, `MEM`, `Memory`, `MFF2`, `MFF`, `MITAB`, `MVT`, `NDF`, `NetCDF`, `NGSGEOID`, `NITF`, `NTv2`, `NWT_GRC`, `NWT_GRD`, `OGR_PDS`, `OGR_SDTS`, `OGR_VRT`, `OpenJPEG`, `OSM`, `OpenFileGDB`, `PAux`, `PCIDSK`, `PDS`, `PGDUMP`, `PNG`, `PNM`, `RMF`, `ROI_PAC`, `RPFTOC`, `RS2`, `RST`, `R`, `S57`, `SAGA`, `SAR_CEOS`, `SDTS`, `SGI`, `SNODAS`, `SQLite`, `SRP`, `SRTMHGT`, `SVG`, `SXF`, `TIL`, `TSX`, `Terragen`, `UK .NTF`, `USGSDEM`, `VICAR`, `VRT`, `vsiaz`, `vsicurl`, `vsigs`, `vsigzip`, `vsimem`, `vsioss`, `vsis3`, `WAsP`, `WCS`, `WMS`, `WMTS`, `XPM`, `XYZ`, `ZMap`

When compiling against a system-installed shared GDAL, all drivers and projections supported by it, should also be supported by `gdal-async`.

## Contributors

This binding was originally the product of a collaboration between [Natural Atlas](https://github.com/naturalatlas) and [Mapbox](https://github.com/mapbox). Its contributors are [Brandon Reavis](https://github.com/brandonreavis), [Brian Reavis](https://github.com/brianreavis), [Dane Springmeyer](https://github.com/springmeyer), [Zac McCormick](https://github.com/zhm), and [others](https://github.com/naturalatlas/node-gdal/graphs/contributors).

node-gdal-next is maintained by [@contra](https://github.com/contra).

The async bindings, the curve geometries, the TypeScript support, the built-in networking and the multidimensional raster model are by [@mmomtchev](https://github.com/mmomtchev) who is the current maintainer.

Before submitting pull requests, please update the [tests](test) and make sure they all pass.

```sh
$ npm test # test against bundled gdal
$ npm run test:shared # test against most major versions
# test against shared gdal on given Linux version and Node.js version
$ npm run container dev {ubuntu|centos|fedora|debian|archlinux|amazon}:{version} 12|14|16|17|lts shared
```

## License

Copyright &copy; 2015–2017 [Natural Atlas, Inc.](https://github.com/naturalatlas) & [Contributors](https://github.com/naturalatlas/node-gdal/graphs/contributors)

Copyright &copy; 2020-2021 [Momtchil Momtchev, @mmomtchev](https://github.com/mmomtchev) & [Contributors](https://github.com/mmomtchev/node-gdal-async/graphs/contributors)

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at: http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

## Unit-tested platforms with pre-built binaries

Release binaries with pre-built bundled GDAL are tested against the full matrix of:

* Node.js versions: 12.x, 14.x, 16.x, 17.x, 18.x
* OS: Ubuntu 16.04, Ubuntu 18.04, Ubuntu 20.04, Ubuntu 21.10, CentOS 8 Stream, Fedora 33, Fedora 34, Debian 10 buster, Debian 11 bullseye, Arch Linux current, Amazon Linux, Windows Server 2019, macOS Catalina 10.15

On Ubuntu 16.04, Amazon Linux, Windows and macOS only the bundled GDAL version is officially supported. On all other platforms both the bundled and the system-installed versions are supported.

Node.js 17 on ArchLinux is not ABI compatible with the NodeSource binaries and requires rebuilding from source.

On Linux, the binaries require glibc 2.23 (Ubuntu 16), except for Node.js 18 which requires glibc 2.28 (Ubuntu 20 / Debian 10).

Development versions are unit tested for the following targets:

---
| Node | OS | GDAL |
| --- | --- | --- |
| Node.js 16.x | CentOS 8 Stream | system-installed GDAL 3.0.4
| Node.js 16.x | CentOS 8 Stream | bundled GDAL
| Node.js 16.x | Debian 10 buster | system-installed GDAL 2.1.2 (*earliest unit-tested GDAL*)
| Node.js 16.x | Debian 11 bullseye | system-installed GDAL 3.2.2
| Node.js 16.x | Debian 11 bullseye | bundled GDAL
| Node.js 16.x | Fedora 33 | system-installed GDAL 3.1.4
| Node.js 16.x | Fedora 34 | system-installed GDAL 3.2.2
| Node.js 16.x | Fedora 34 | bundled GDAL
| Node.js 18.x | Arch Linux current | system installed GDAL 3.2.3
| Node.js 18.x | Arch Linux current | bundled GDAL (*requires rebuilding*)
| Node.js 16.x | Ubuntu 16.04 | bundled GDAL (*glibc target platform*)
| Node.js 16.x | Ubuntu 18.04 | system-installed GDAL 2.2.3
| Node.js 16.x | Ubuntu 18.04 | bundled GDAL
| Node.js 12.x | Ubuntu 20.04 | system-installed GDAL 3.0.4
| Node.js 14.x | Ubuntu 20.04 | system-installed GDAL 3.0.4
| Node.js 16.x | Ubuntu 20.04 | system-installed GDAL 3.0.4
| Node.js 17.x | Ubuntu 20.04 | system-installed GDAL 3.0.4
| Node.js 18.x | Ubuntu 20.04 | system-installed GDAL 3.0.4
| Node.js 14.x | Ubuntu 22.04 | system-installed GDAL 3.4.1
| Node.js 12.x | Ubuntu 20.04 | bundled GDAL
| Node.js 14.x | Ubuntu 20.04 | bundled GDAL (*code coverage platform*)
| Node.js 16.x | Ubuntu 20.04 | bundled GDAL
| Node.js 17.x | Ubuntu 20.04 | bundled GDAL
| Node.js 18.x | Ubuntu 20.04 | bundled GDAL
| Node.js 14.x | Amazon Linux | bundled GDAL
| Node.js 12.x | Windows Server 2019 | bundled GDAL
| Node.js 14.x | Windows Server 2019 | bundled GDAL
| Node.js 16.x | Windows Server 2019 | bundled GDAL
| Node.js 17.x | Windows Server 2019 | bundled GDAL
| Node.js 18.x | Windows Server 2019 | bundled GDAL
| Node.js 12.x | macOS Catalina 10.15 | bundled GDAL
| Node.js 14.x | macOS Catalina 10.15 | bundled GDAL
| Node.js 16.x | macOS Catalina 10.15 | bundled GDAL
| Node.js 17.x | macOS Catalina 10.15 | bundled GDAL
| Node.js 18.x | macOS Catalina 10.15 | bundled GDAL
---
