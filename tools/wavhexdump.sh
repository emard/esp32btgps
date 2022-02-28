#!/bin/sh
exec hexdump -s 8 -f hexdump.fmt $1
