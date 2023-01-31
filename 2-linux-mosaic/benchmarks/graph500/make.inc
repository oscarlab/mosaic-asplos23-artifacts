# -*- Makefile -*-
# Copyright 2010, Georgia Institute of Technology, USA.
# See COPYING for license.
CFLAGS = -g -std=c99
LDLIBS = -lm -lrt -lpthread
# Comment due to error at runtime 
CPPFLAGS = # -DUSE_MMAP_LARGE -DUSE_MMAP_LARGE_EXT

BUILD_OPENMP = Yes
CFLAGS_OPENMP = -fopenmp

BUILD_DGX = Yes
CFLAGS_DGX = -rdc=true -Xcompiler -fopenmp -O3 
