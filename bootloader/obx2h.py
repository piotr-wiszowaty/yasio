#!/usr/bin/env python3

# usage: obx2h FILE ARRAY_NAME

import sys

fname = sys.argv[1]
array_name = sys.argv[2]

f = open(fname, "rb")
data = f.read()
f.close()

total_size = (len(data) + 127) & ~0x7f
print(f"uint8_t {array_name}[{total_size}] = {{", end="")

for i in range(total_size):
    if i & 0x0f == 0:
        print("\n\t", end="")
    if i < len(data):
        print(f"0x{data[i]:02X}", end="")
    else:
        print("0xFF", end="")
    if i < total_size:
        print(", ", end="")

print("\n};")
