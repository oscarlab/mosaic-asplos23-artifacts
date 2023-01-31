#!/bin/sh

git clone https://github.com/ANL-CESAR/XSBench.git xsbench_code
cd xsbench_code
git checkout v20
make -C openmp-threading
cp openmp-threading/XSBench ../XSBench
