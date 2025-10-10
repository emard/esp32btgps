#!/bin/sh

BASEDIR=$(dirname $0)

for x in $*
do
  #unzip $x
  #mv doc.kml $x.kml
  #xsltproc $BASEDIR/kml2csv.xsl $x.kml > $x.csv
  #rm $x.kml
  unzip -qc $x | xsltproc $BASEDIR/kml2csv.xsl - > $(dirname $x)/$(basename $x .kmz).csv
done

# rename 's/\.kmz.csv$/\.csv/' /tmp/data/*.kmz.csv
