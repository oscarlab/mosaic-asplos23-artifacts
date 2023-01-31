#!/bin/sh

git clone https://github.com/graph500/graph500.git
cd graph500
git checkout v2-spec
cp ../make.inc .
make
cp seq-csr/seq-csr ../seq-csr
