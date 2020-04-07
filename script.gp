#!/usr/local/bin/gnuplot 

scale = 7

set style line 1 \
  linecolor black \
  pointtype 5 pointsize 0.2

set multiplot
set xlabel 'Time (Days)'
set ylabel 'population' 
set title 'Cases per 100,000'
set size 1, 0.25

set origin 0.0, 0.75
plot "log.dat" u 1:($2/scale) w l t 'SUSCEPTIBLE', \
  "log.dat" u 1:($3/scale) w l t 'EXPOSED', \
  "log.dat" u 1:($4/scale) w l t 'INFECTIOUS', \
  "log.dat" u 1:($5/scale) w l t 'HOSPITALIZED', \
  "log.dat" u 1:($7/scale) w l t 'RECOVERED'

set origin 0.0, 0.50
plot "log.dat" u 1:($3/scale) w lp linestyle 1 t 'EXPOSED' 

set origin 0.0, 0.25
plot "log.dat" u 1:($6/scale) w lp linestyle 1 title 'CRITICAL'

set origin 0.0, 0.0
plot "log.dat" u 1:($8/scale) w lp linestyle 1 title 'DECEASED'
unset multiplot


