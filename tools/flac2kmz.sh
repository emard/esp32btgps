#!/bin/sh

BASEDIR=$(dirname $0)

for x in $*
do
  $BASEDIR/wav2kml.py $x > /tmp/doc.kml
  zip -j $(dirname $x)/$(basename $x .flac).kmz.part /tmp/doc.kml
  mv $(dirname $x)/$(basename $x .flac).kmz.part $(dirname $x)/$(basename $x .flac).kmz
  rm /tmp/doc.kml
done

# rename 's/\.kmz.csv$/\.csv/' /tmp/data/*.kmz.csv
