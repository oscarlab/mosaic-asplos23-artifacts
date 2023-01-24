#!/usr/bin/env python3
import sys
import re
import os
import statistics

def parseFile(path,printresult=False):
  result = {}
  with open(path, 'r') as file:
    #remove three lines
    header = file.readline()
    header = file.readline()
    header = file.readline()


    time = 0
    sum_swapin = 0
    sum_swapout = 0
    while True:
      line = file.readline().strip()
      line = re.split("\ +", line)

      if line[-2] != "0.00" or line[-1] != "0.00":
        break
    while len(line) != 1 or line[0] != '':
      time += 1
      sum_swapin += float(line[-2])
      sum_swapout += float(line[-1])
      line = file.readline().strip()
      line = re.split("\ +", line)
  filename = path.split('_')

  result["bench"] = filename[0]
  result["system"] = filename[2]
  result["size"] = filename[1]
  result["run"] = filename[3]
  result["runtime"] = time
  result["swapin"] = sum_swapin
  result["swapout"] = sum_swapout
  if printresult:
    print("{},{},{},{},{},{:.2f},{:.2f}".format(filename[0],filename[2],filename[1],filename[3], time, sum_swapin/1000, sum_swapout/1000))
  return result
  
def printCurr(c):
  if "bench" in c:
    if len(c["swapin"]) == 1:
      print("{},{},{},{:.2f},0,{:.2f},0".format(c["bench"], c["system"], c["size"],  
                                                 statistics.mean(c["swapin"])/1000,
                                                 statistics.mean(c["swapout"])/1000))
    else:
      print("{},{},{},{:.2f},{:.2f},{:.2f},{:.2f}".format(c["bench"], c["system"], c["size"],  
                                                 statistics.mean(c["swapin"])/1000, statistics.stdev(c["swapin"])/1000,
                                                 statistics.mean(c["swapout"])/1000, statistics.stdev(c["swapout"])/1000))

if len(sys.argv) == 2:
  parseFile(sys.argv[1], True)
  exit()
elif len(sys.argv) != 1:
  print("Wrong input")
  exit()


curr = {}
for x in sorted(os.listdir()):
  if x.endswith("swap.txt"):
    ret = parseFile(x)
    if "bench" not in curr or curr["bench"] != ret["bench"] or curr["system"] != ret["system"] or curr["size"] != ret["size"]:
      printCurr(curr)
      curr["bench"] = ret["bench"]
      curr["system"] = ret["system"]
      curr["size"] = ret["size"]
      curr["swapin"] = []
      curr["swapout"] = []
    curr["swapin"].append(ret["swapin"])
    curr["swapout"].append(ret["swapout"])
printCurr(curr)
