# Building for Electron

:warning: `gdal-async`, as well as all other Nan-based native modules, is not compatible with Electron on Windows due to [#29893](https://github.com/electron/electron/issues/29893)

```shell
export npm_config_disturl=https://electronjs.org/headers
export npm_config_runtime=electron
export npm_config_target=19.0.1
npm install --save gdal-async
```
