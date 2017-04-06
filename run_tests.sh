#!/bin/bash

nframes="3 10 20 30 40 50 60 70 80 90 100"
algorithms="rand fifo custom"
programs="sort scan focus"


for algo in $algorithms; do
	for prog in $programs; do
		for frame in $nframes; do
			echo "./virtmem 100 $frame $algo $prog"
			./virtmem 100 $frame $algo $prog
		done
	done
done
