#!/bin/sh

BASEDIR=$(dirname $0)

for x in $*
do
  xsltproc $BASEDIR/kml2csv.xsl $x > $(dirname $x)/$(basename $x .kml).csv
done
