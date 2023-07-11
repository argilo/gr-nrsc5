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
from gnuradio import gr

class lot_encoder(gr.basic_block):
    """Read a file and encode it as LOT packets"""
    INTERVAL = 10.0

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
                expiry = 0x7eb4a59e
                size = len(data)
                mime = 0x4f328ca0  # PNG
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
