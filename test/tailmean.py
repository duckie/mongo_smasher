#!/usr/bin/env python3

import argparse, sys
parser = argparse.ArgumentParser(description='Compute means of latest values')
parser.add_argument('-t','--tail', type=int, nargs='?', default=10)
args = parser.parse_args()

nb_values = args.tail
values = []

for line in sys.stdin:
    value = int(line)
    values.append(value)
    if nb_values < len(values):
        values.pop(0)
    mean = float(sum(values))/float(len(values))
    print(int(mean))
