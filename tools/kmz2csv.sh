#!/bin/sh

BASEDIR=$(dirname $0)

for x in $*
do
  unzip $x
  mv doc.kml $x.kml
  xsltproc $BASEDIR/kml2csv.xsl $x.kml > $x.csv
  rm $x.kml
done
