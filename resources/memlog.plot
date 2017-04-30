#!/usr/bin/gnuplot

set terminal png size 800,480
set xlabel 'Seconds'
set ylabel 'MB RAM'
set output 'memlog.png'
plot 'memlog.dat' using 1:2 with lines title 'OS',\
     'memlog.dat' using 1:3 with lines title 'JVM'
