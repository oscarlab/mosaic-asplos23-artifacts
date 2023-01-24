#!/usr/bin/env python3

import re

g5_footprints = [4158,4413,4669,4924,5180,5436,5691,5947,6203,6459]
xb_footprints = [4156,4412,4668,4924,5180,5435,5691,5947,6203,6459]
bt_footprints = [4154,4409,4664,4919,5175,5430,5685,5940,6196,6451]
benches = ['g5','xb','bt']
bench_names = {'g5':'Graph500','xb':'XSBench','bt':'BTree'}
footprints_lists = {'g5':g5_footprints,'xb':xb_footprints,'bt':bt_footprints}

# Table 4
resultfile = "results.txt"
results = {}
with open(resultfile) as f:
  for line in f:
    curr = line.strip().split(',')
    bench = curr[0]
    size = curr[2]
    platform = curr[1]
    if bench not in results:
      results[bench] = {}
    if size not in results[bench]:
      results[bench][size] = {}
    if platform not in results[bench][size]:
      results[bench][size][platform] = {"swpin":[], "swpout":[]}
    results[bench][size][platform]["swpin"] = float(curr[-4])
    results[bench][size][platform]["swpin_sd"] = float(curr[-3])
    results[bench][size][platform]["swpout"] = float(curr[-2])
    results[bench][size][platform]["swpout_sd"] = float(curr[-1])
    results[bench][size][platform]["total_swp"] = float(curr[-4]) + float(curr[-2])

print("benchmark,footprints,cgswap,iceswap,diff")

for bench in benches:
  footprints = footprints_lists[bench]
  for size in results[bench]:
    curr = results[bench][size]
    print("{},{},{:.2f},{:.2f},{:.2f}".format(bench_names[bench], footprints[int(size)],
      curr["cg"]["total_swp"],
      curr["ice"]["total_swp"],
      (curr["cg"]["total_swp"] - curr["ice"]["total_swp"])/curr["cg"]["total_swp"] * 100))


