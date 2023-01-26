#!/bin/bash

./parse_results.py > results.txt
./generate_table3.py > table3.csv
./generate_table4.py > table4.csv
