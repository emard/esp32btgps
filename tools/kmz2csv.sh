#!/bin/sh

for x in $*
do
  unzip $x
  mv doc.kml $x.kml
  xsltproc kml2csv.xsl $x.kml > $x.csv
  rm $x.kml
done
