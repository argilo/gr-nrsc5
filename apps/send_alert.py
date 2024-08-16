#!/usr/bin/env python3

import argparse
import socket
import os
import sys


def append_int(command_bits, n, length):
    for _ in range(length):
        command_bits.append(n & 1)
        n >>= 1


def middle_digits(location):
    return (location % 100000) // 1000


def compress(location):
    return ((location // 100000) * 1000) + (location % 1000)


parser = argparse.ArgumentParser()
parser.add_argument("category", type=int)
parser.add_argument("message")
parser.add_argument("--same", type=int, action="append", default=[])
parser.add_argument("--fips", type=int, action="append", default=[])
parser.add_argument("--zip", type=int, action="append", default=[])
parser.add_argument("--test", action="store_true")
parser.add_argument("--host", default="localhost")
parser.add_argument("--port", type=int, default=52000)
args = parser.parse_args()

if bool(args.same) + bool(args.fips) + bool(args.zip) > 1:
    print("All locations must use the same format", file=sys.stderr)
    exit(1)

location_format = 0
locations = args.same
full_len = 20
compressed_len = 14
if args.fips:
    location_format = 1
    locations = args.fips
    full_len = 17
    compressed_len = 10
elif args.zip:
    location_format = 2
    locations = args.zip
    full_len = 17
    compressed_len = 10

if len(locations) > 31:
    print("Can't use more than 31 locations", file=sys.stderr)
    exit(1)

locations.sort()

command_bits = []
append_int(command_bits, 0, 8)                                # unknown
append_int(command_bits, 0, 12)                               # CRC
append_int(command_bits, 0x49, 8)                             # unknown
append_int(command_bits, args.category, 5)                    # category type 1
append_int(command_bits, 30 if args.test else 0, 5)           # category type 2
append_int(command_bits, 0b101101101, 9)                      # unknown
append_int(command_bits, location_format, 3)                  # location format
append_int(command_bits, len(locations), 5)                   # number of locations
command_bits.append(0)                                        # unknown

if len(locations) >= 1:
    append_int(command_bits, locations[0], full_len)

for i in range(1, len(locations)):
    if middle_digits(locations[i]) == middle_digits(locations[i-1]):
        command_bits.append(0)
        append_int(command_bits, compress(locations[i]), compressed_len)
    else:
        command_bits.append(1)
        append_int(command_bits, locations[i], full_len)


while len(command_bits) % 16 != 8:
    command_bits.append(0)

command_bytes = []
for start in range(0, len(command_bits), 8):
    val = 0
    for offset in range(8):
        val |= (command_bits[start + offset] << offset)
    command_bytes.append(val)
command_bytes = bytes(command_bytes)

command = f"set_alert|{command_bytes.hex()}|{args.message}\n"
print(command[:-1])
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.host, args.port))
s.send(command.encode())
s.shutdown(socket.SHUT_RDWR)
s.close()
