#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2023 Clayton Smith.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import pmt
import struct
import threading
from datetime import datetime, timedelta, timezone
from gnuradio import gr


class lot_encoder(gr.basic_block):
    """Read a file and encode it as LOT packets"""
    INTERVAL = 10.0

    PNG_START = bytes.fromhex("89504E470D0A1A0A")
    JPEG_START = bytes.fromhex("FFD8")
    JPEG_END = bytes.fromhex("FFD9")

    MIMEHASH_PNG = 0x4F328CA0
    MIMEHASH_JPEG = 0x1E653E9C
    MIMEHASH_TEXT = 0xBB492AAC

    def __init__(self, filename="", lot_id=0, port=0x1001):
        gr.sync_block.__init__(
            self,
            name='LOT encoder',
            in_sig=[],
            out_sig=[]
        )
        self.message_port_register_out(pmt.intern("aas"))

        self.filename = filename
        self.port = port
        self.lot_id = lot_id

    def start(self):
        self.parts = []
        with open(self.filename, "rb") as f:
            data = f.read()

        for offset in range(0, len(data), 256):
            chunk = data[offset:offset+256]
            seq = offset // 256

            if seq == 0:
                header_len = 24 + len(self.filename.encode())
            else:
                header_len = 8

            repeat = 1

            header = struct.pack("<BBHI", header_len, repeat, self.lot_id, seq)
            if seq == 0:
                version = 1

                dt = datetime.now(timezone.utc) + timedelta(days=365)
                expiry = (dt.year << 20) | (dt.month << 16) | (dt.day << 11) | (dt.hour << 6) | dt.minute

                size = len(data)

                if data.startswith(self.PNG_START):
                    mime = self.MIMEHASH_PNG
                elif data.startswith(self.JPEG_START) and data.endswith(self.JPEG_END):
                    mime = self.MIMEHASH_JPEG
                elif self.filename.lower().endswith(".png"):
                    mime = self.MIMEHASH_PNG
                elif self.filename.lower().endswith(".jpg") or self.filename.lower().endswith(".jpeg"):
                    mime = self.MIMEHASH_JPEG
                elif self.filename.lower().endswith(".txt"):
                    mime = self.MIMEHASH_TEXT
                else:
                    raise ValueError("Unsupported file type. Supported types: PNG, JPG, TXT.")

                header += struct.pack("<IIII", version, expiry, size, mime) + self.filename.encode()

            self.parts.append(header + chunk)

        self.aas_seq = 0
        self.timer = threading.Timer(self.INTERVAL, self.send)
        self.timer.start()

    def stop(self):
        self.timer.cancel()

    def send(self):
        print(f"Sending LOT file: {self.filename}")
        for part in self.parts:
            aas_pdu = struct.pack("<BHH", 0x21, self.port, self.aas_seq) + part
            msg = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(aas_pdu), list(aas_pdu)))
            self.message_port_pub(pmt.intern("aas"), msg)
            self.aas_seq = (self.aas_seq + 1) & 0xffff

        self.timer = threading.Timer(self.INTERVAL, self.send)
        self.timer.start()
