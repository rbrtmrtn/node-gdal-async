#!/bin/bash

set -eu
shopt -s nullglob

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR/libcurl"

CURL_VERSION=7.79.1
dir_curl=./curl

#
# download curl source
#

rm -rf $dir_curl
if [[ ! -f curl-${CURL_VERSION}.tar.gz ]]; then
	curl -L https://curl.se/download/curl-${CURL_VERSION}.tar.gz -o curl-${CURL_VERSION}.tar.gz
fi
tar -xzf curl-${CURL_VERSION}.tar.gz
mv curl-${CURL_VERSION} $dir_curl

#
# apply patches
#
for PATCH in patches/*.diff; do
  echo "Applying ${PATCH}"
  patch -p1 < $PATCH
done

# update the CA bundle
curl -L https://curl.se/ca/cacert.pem -o cacert.pem
[ -d ${dir_curl} ] && rm -rf ${dir_curl}/{tests,projects}
