#!/usr/bin/env python3

import re

g5_footprints = [4158,4413,4669,4924,5180,5436,5691,5947,6203,6459]
xb_footprints = [4156,4412,4668,4924,5180,5435,5691,5947,6203,6459]
bt_footprints = [4154,4409,4664,4919,5175,5430,5685,5940,6196,6451]
benches = ['g5','xb','bt']
bench_names = {'g5':'Graph500','xb':'XSBench','bt':'BTree'}
footprints_lists = {'g5':g5_footprints,'xb':xb_footprints,'bt':bt_footprints}


# Table 3
util_filename = "{}_{}_ice_util.txt"
pattern = "([0-9]{2,3}\.[0-9]{2}%)"
print("benchmark,footprints,firstconflict,utilization")
for i in range(4):
  for bench in benches:
    with open(util_filename.format(bench, i)) as f:
      util = re.findall(pattern, f.readline())
      print("{},{},{},{}".format(bench_names[bench], footprints_lists[bench][i], util[0], util[1]))

