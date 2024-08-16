#!/usr/bin/env python3

import argparse
import socket
import os


def append_int(command_bits, n, length):
    for _ in range(length):
        command_bits.append(n & 1)
        n >>= 1


def fips_cardinal(fips_code):
    return (fips_code) // 100000


def fips_state(fips_code):
    return (fips_code % 100000) // 1000


def fips_county(fips_code):
    return fips_code % 1000


parser = argparse.ArgumentParser()
parser.add_argument("category", type=int)
parser.add_argument("message")
parser.add_argument("--fips", type=int, action="append", default=[])
parser.add_argument("--test", action="store_true")
parser.add_argument("--host", default="localhost")
parser.add_argument("--port", type=int, default=52000)
args = parser.parse_args()

fips = sorted(args.fips)

command_bits = []
append_int(command_bits, 0, 8)                                # unknown
append_int(command_bits, 0, 12)                               # CRC
append_int(command_bits, 0x49, 8)                             # unknown
append_int(command_bits, args.category, 6)                    # category type
append_int(command_bits, 0b1111 if args.test else 0b0000, 4)  # test indicator?
append_int(command_bits, 0b000101101101, 12)                  # unknown
append_int(command_bits, num_locations, 6)                    # number of locations

if num_locations >= 1:
    append_int(command_bits, fips[0], 20)

for i in range(1, num_locations):
    if fips_state(fips[i]) == fips_state(fips[i-1]):
        command_bits.append(0)
        compressed_fips = fips_cardinal(fips[i]) * 1000 + fips_county(fips[i])
        append_int(command_bits, compressed_fips, 14)
    else:
        command_bits.append(1)
        append_int(command_bits, fips[i], 20)

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
print(command)
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.host, args.port))
s.send(command.encode())
s.shutdown(socket.SHUT_RDWR)
s.close()
