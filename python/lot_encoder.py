#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2023 Clayton Smith.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

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

        self.filename = filename
        self.port = port
        self.lot_id = lot_id
        self.my_log = gr.logger(self.alias())

        #params for file input
        #status of message (idle, or expecting more segments)
        self.inputIdle = True
        #file size to expect in bytes
        self.filesize = 0
        #file bytes input via pdu
        self.receivedFile = bytearray()
        #vars to keep track of broken up meta messages
        self.expectMore = False
        self.configstr = ""
        self.expectedLen = 0

    def handle_new_file(self, msg):
        data = pmt.to_python(msg)
        if not (isinstance(data, tuple) and len(data) == 2):
            print('Expected tuple of (None, str)')
            return
        #only attempt to parse text if we are waiting for a new file
        if self.inputIdle or self.expectMore:
            try:
                text = bytes(data[1]).decode()
            except:
                print("LOT Invalid Packet received")
                return
            startmessage = "newfile"
            #Make sure we get the whole message first
            if text.startswith(startmessage):
                lengthfield = text[len(startmessage):].split('|')[0]
                self.expectedLen = len(lengthfield)+int(lengthfield)
                self.configstr += text
                if len(text) < self.expectedLen:
                    self.expectMore = True
                    return
                else:
                    self.expectMore = False
            if self.expectMore:
                self.configstr += text
            if len(self.configstr) < self.expectedLen:
                return
            else:
                self.expectMore = False
            if self.configstr.startswith(startmessage):
                fields = self.configstr[len(startmessage):].split('|')
                self.filename = fields[1]
                self.lot_id = int(fields[2])
                self.filesize = int(fields[3])
                self.inputIdle = False
                self.configstr = ""
        else:
            self.receivedFile.extend(bytes(data[1]))
            #check if we've received enough data
            if len(self.receivedFile) >= self.filesize:
                self.inputIdle = True
                self.update_file()
                print("New file ready to send:", self.filename, " LOT:", self.lot_id)

    def handle_notify(self, msg):
        port = pmt.to_python(msg)
        if port == self.port:
            self.send()

    def update_file(self):
        self.parts = []
        if self.receivedFile:
            data = self.receivedFile
            self.receivedFile = bytearray()
        else:
            with open(self.filename, "rb") as f:
                data = f.read()
                if "/" in self.filename:
                    self.filename = self.filename.split("/")[-1]

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

    def start(self):
        self.update_file()
        self.aas_seq = 0
        self.send()

    def send(self):
        self.my_log.info(f"Sending LOT file: {self.filename}")
        for part in self.parts:
            aas_pdu = struct.pack("<BHH", 0x21, self.port, self.aas_seq) + part
            msg = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(aas_pdu), list(aas_pdu)))
            self.message_port_pub(pmt.intern("aas"), msg)
            self.aas_seq = (self.aas_seq + 1) & 0xffff
