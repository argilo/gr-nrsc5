#!/usr/bin/env python3

import argparse
import socket
import os

parser = argparse.ArgumentParser()
parser.add_argument("lot_id", type=int)
parser.add_argument("filename")
parser.add_argument("--host", default="localhost")
parser.add_argument("--psd-port", type=int, default=52002)
parser.add_argument("--file-port", type=int, default=52004)
args = parser.parse_args()

with open(args.filename, "rb") as f:
    data = f.read()
basename = os.path.basename(args.filename)

command = f"streamfile|{args.lot_id}|{len(data)}|{basename}\n".encode()
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.host, args.file_port))
s.send(command)
s.send(data)
s.close()

command = f"lot{args.lot_id}\n".encode()
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((args.host, args.psd_port))
s.send(command)
s.close()
