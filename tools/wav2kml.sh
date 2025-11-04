#!/bin/sh

BASEDIR=$(dirname $0)

for x in $*
do
  $BASEDIR/wav2kml.py $x > $(dirname $x)/$(basename $x .wav).kml
done
