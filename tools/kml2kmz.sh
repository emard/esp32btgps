#!/bin/sh

for x in $*
do
  ln -s $x /tmp/doc.kml
  zip -j $(dirname $x)/$(basename $x .kml).kmz /tmp/doc.kml
  rm /tmp/doc.kml
done
