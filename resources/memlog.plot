#!/usr/bin/gnuplot
#
# Copyright 2017, akashche at redhat.com
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE.md file that
# accompanied this code).

set terminal png size 800,480
set xlabel 'Seconds'
set ylabel 'MB RAM'
set output 'memlog.png'
plot 'memlog.dat' using 1:2 with lines title 'OS',\
     'memlog.dat' using 1:3 with lines title 'JVM'
