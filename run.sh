#!/bin/bash

rm agent
g++ -std=c++11 -g -Wall -fPIC agent.cpp -o agent && ./agent > log.dat
gnuplot -persist script.gp
