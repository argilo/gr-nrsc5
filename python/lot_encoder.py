#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2023 Clayton Smith.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

import os
import pmt
import struct
from datetime import datetime, timedelta, timezone
from gnuradio import gr


class lot_encoder(gr.basic_block):
    """Read a file and encode it as LOT packets"""
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

        self.message_port_register_in(pmt.intern("file"))
        self.set_msg_handler(pmt.intern("file"), self.handle_new_file)

        self.message_port_register_in(pmt.intern("ready"))
        self.set_msg_handler(pmt.intern("ready"), self.handle_notify)

        self.port = port
        with open(filename, "rb") as f:
            self.prepare_file(os.path.basename(filename), f.read(), lot_id)

        self.command_buffer = bytes()

        self.my_log = gr.logger(self.alias())

    def handle_new_file(self, msg):
        data = bytes(pmt.to_python(msg)[1])
        self.command_buffer += data

        command_end = self.command_buffer.find(b"\n")
        if command_end >= 0:
            command = self.command_buffer[:command_end]
            parts = command.split(b"|")

            if (parts[0] == b"streamfile") and (len(parts) == 4):
                lot_id = int(parts[1])
                size = int(parts[2])
                filename = parts[3]
                if len(self.command_buffer) >= command_end + 1 + size:
                    filedata = self.command_buffer[command_end + 1:command_end + 1 + size]
                    self.prepare_file(filename, filedata, lot_id)
                    self.command_buffer = self.command_buffer[command_end + 1 + size:]
            elif (parts[0] == b"file") and (len(parts) == 3):
                lot_id = int(parts[1])
                filename = parts[2]
                with open(filename, "rb") as f:
                    self.prepare_file(os.path.basename(filename), f.read(), lot_id)
                self.command_buffer = self.command_buffer[command_end + 1:]
            else:
                self.my_log.warn(f"Invalid command: {command}")
                self.command_buffer = self.command_buffer[command_end + 1:]

    def handle_notify(self, msg):
        port = pmt.to_python(msg)
        if port == self.port:
            self.send()

    def prepare_file(self, filename, data, lot_id):
        if isinstance(filename, str):
            filename = filename.encode()

        parts = []

        for offset in range(0, len(data), 256):
            chunk = data[offset:offset+256]
            seq = offset // 256

            if seq == 0:
                header_len = 24 + len(filename)
            else:
                header_len = 8

            repeat = 1

            header = struct.pack("<BBHI", header_len, repeat, lot_id, seq)
            if seq == 0:
                version = 1

                dt = datetime.now(timezone.utc) + timedelta(days=365)
                expiry = (dt.year << 20) | (dt.month << 16) | (dt.day << 11) | (dt.hour << 6) | dt.minute

                size = len(data)

                if data.startswith(self.PNG_START):
                    mime = self.MIMEHASH_PNG
                elif data.startswith(self.JPEG_START) and data.endswith(self.JPEG_END):
                    mime = self.MIMEHASH_JPEG
                elif filename.lower().endswith(b".png"):
                    mime = self.MIMEHASH_PNG
                elif filename.lower().endswith(b".jpg") or filename.lower().endswith(b".jpeg"):
                    mime = self.MIMEHASH_JPEG
                elif filename.lower().endswith(b".txt"):
                    mime = self.MIMEHASH_TEXT
                else:
                    raise ValueError("Unsupported file type. Supported types: PNG, JPG, TXT.")

                header += struct.pack("<IIII", version, expiry, size, mime) + filename

            parts.append(header + chunk)

        self.filename = filename
        self.lot_id = lot_id
        self.parts = parts

    def start(self):
        self.aas_seq = 0
        self.send()

    def send(self):
        self.my_log.info(f"Sending LOT file {self.lot_id}: {self.filename}")
        for part in self.parts:
            aas_pdu = struct.pack("<BHH", 0x21, self.port, self.aas_seq) + part
            msg = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(aas_pdu), list(aas_pdu)))
            self.message_port_pub(pmt.intern("aas"), msg)
            self.aas_seq = (self.aas_seq + 1) & 0xffff
