#!/bin/bash

if [ $# -lt 3 ]; then
    echo "Usage: $0 data_file output_format output_file [xlabel ylabel]"
    exit 1
fi

TITLES=($(head -n 1 $1))
TITLES=(${TITLES[@]:2})

(
    echo "set term $2"
    echo "set output '$3'"
    echo "set xlabel '$4'"
    echo "set ylabel '$5'"
    sim=${TITLES[0]}
    echo -n "plot '$1' u 1:2 t '$sim' w l"
    si=1
    while [ $si -lt ${#TITLES[@]} ]; do
	sim=${TITLES[$si]}
	sif=$[si + 2]
	echo -n ", '$1' u 1:$sif t '$sim' w l"
	si=$[si + 1]
    done
    echo ""
) | gnuplot
