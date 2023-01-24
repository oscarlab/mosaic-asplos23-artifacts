#!/bin/bash

function print_usage
{
  echo "usage: $0 [g5|xb|bt] [ice|cg] size run"
  exit
}

if [[ $# != 4 ]]; then print_usage; fi
if [[ "$1" != "g5" ]] && [[ "$1" != "xb" ]] && [[ "$1" != "bt" ]]; then print_usage; fi
if [[ "$2" != "ice" ]] && [[ "$2" != "cg" ]]; then print_usage; fi
if ! [[ $3 =~ ^[0-9]$ ]]; then print_usage; fi

echo $1$2$3

sudo swapoff -a

if [[ "$1" == "g5" ]]; then
CMD="./apps/seq-csr -s 22 -e $((31 + 2 * $3))"
elif [[ "$1" == "xb" ]]; then
CMD="./apps/XSBench -t 1 -g $((8313 + 512 * $3)) -p 500000"
elif [[ "$1" == "bt" ]]; then
CMD="./apps/BTree $((41800000 + 2570000 * $3))"
fi


if [[ "$2" == "ice" ]]; then
IFS=' ' read -r EXEC ARGS <<< "$CMD"
cp $EXEC mosaictest
CMD="./mosaictest "$ARGS
elif [[ "$2" == "cg" ]]; then
CMD='sudo systemd-run --scope -p "MemoryMax=4G" '$CMD
fi

FILENAME="$1_$3_$2_r$4_"
sar -W 1 > "${FILENAME}swap.txt" &

sleep 2
sudo swapon /dev/nbd0
sleep 2
echo $CMD
eval $CMD
sleep 2

sudo swapoff -a
if [[ "$2" == "ice" ]]; then
rm mosaictest
fi

sleep 3
trap 'kill $(jobs -p)' EXIT

