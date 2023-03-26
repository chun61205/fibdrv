cd "perf"
reset                                                                           
set xlabel 'nth fibonacci'
set ylabel 'time(ns)'
set title 'perf'
set term png enhanced font 'Verdana,10'
set output 'test.png'
set format x "%10.0f"
set xtic 20

plot \
'time.txt' using 1:2 with linespoints linewidth 2 title 'utime',\
'time.txt' using 1:3 with linespoints linewidth 2 title 'kt',\

cd ".."