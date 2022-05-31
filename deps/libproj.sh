#!/bin/bash

set -eu

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/libproj"

PROJ_VERSION=9.0.0
dir_proj=./proj

#
# download proj source
#

rm -rf $dir_proj
if [[ ! -f proj-${PROJ_VERSION}.tar.gz ]]; then
	curl -L http://download.osgeo.org/proj/proj-${PROJ_VERSION}.tar.gz -o proj-${PROJ_VERSION}.tar.gz
fi
tar -xzf proj-${PROJ_VERSION}.tar.gz
mv proj-${PROJ_VERSION} $dir_proj

# build proj.db
(
  cd $dir_proj
  mkdir build
  cd build
  cmake ..
  cmake --build . --target generate_proj_db
  mv data/proj.db ../data
)
rm -rf $dir_proj/build

# Fix for error:
# static library deps/libproj/libproj.gyp:libproj#target has several files with the same basename:
#   geocent: proj/src/geocent.cpp proj/src/conversions/geocent.cpp
#   utils: proj/src/utils.cpp proj/src/apps/utils.cpp
#   internal: proj/src/internal.cpp proj/src/iso19111/internal.cpp
# libtool on Mac cannot handle that. Use --no-duplicate-basename-check to disable this validation.

mv $dir_proj/src/apps/utils.cpp $dir_proj/src/apps/utils_apps.cpp
mv $dir_proj/src/conversions/geocent.cpp $dir_proj/src/conversions/geocent_conversions.cpp
mv $dir_proj/src/iso19111/internal.cpp $dir_proj/src/iso19111/internal_iso19111.cpp
