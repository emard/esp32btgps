#!/bin/sh

# usage
# ./frf2csv.sh < frf_road_axle_car.txt > frf_road_axle_car.csv

# see multiline sed info
# http://www.refining-linux.org/archives/27/20-Multi-line-sed-search-and-replace/

#cat "frf_road_axle_car.txt" | \
#  sed -n '1!N; s/ *\([0-9]*\)\t\([0-9.e+-]*\),[0-9.e+-]*\n\t\([0-9.e+-]*\),[0-9.e+-]*/\1 \2 \3/ p'

# works only for every 2nd line
#cat "/tmp/frf_road_axle_car.txt" | \
#  sed -n 'N; s/ *\([0-9]*\)\t\([0-9.e+-]*\),[0-9.e+-]*\n\t\([0-9.e+-]*\),[0-9.e+-]*/\1 \2 \3/ p'

sed -n '/ *\([0-9]*\)\t\([0-9.e+-]*\),[0-9.e+-]*/{ N; s/ *\([0-9]*\)\t\([0-9.e+-]*\),[0-9.e+-]*\n\t\([0-9.e+-]*\),[0-9.e+-]*/\1 \2 \3/ p}'
