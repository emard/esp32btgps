#!/bin/sh

for x in $*
do
  rm -f /tmp/doc.kml
  cp $x /tmp/doc.kml
  zip --junk-paths $(dirname $x)/$(basename $x .kml).kmz.part /tmp/doc.kml
  mv $(dirname $x)/$(basename $x .kml).kmz.part $(dirname $x)/$(basename $x .kml).kmz
  rm -f /tmp/doc.kml
done
