#!/bin/sh
exec hexdump -s 8 -f wavhexdump.fmt $1
