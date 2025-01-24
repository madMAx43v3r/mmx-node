#!/bin/bash

set -e

./make_release.sh

VERSION=$(cat config/default/build.json | jq -r .version | sed 's/^v//')
ARCH=${ARCH:-any}
DIST=${DIST:-generic}
DST=mmx-node-$VERSION-$ARCH-$DIST

rm -rf $DST
mkdir -m 755 -p $DST/DEBIAN
mkdir -m 755 -p $DST/usr

echo "Version: $VERSION" >> $DST/DEBIAN/control
echo "Architecture: ${ARCH}" >> $DST/DEBIAN/control
cat cmake/debian/${DIST}/control >> $DST/DEBIAN/control

cp -a build/dist/. $DST/usr/

fakeroot dpkg-deb --build $DST

rm -rf $DST
