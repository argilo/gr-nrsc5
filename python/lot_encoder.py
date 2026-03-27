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
import re
from datetime import datetime, timedelta, timezone
from gnuradio import gr


class lot_encoder(gr.basic_block):
    """Read a file and encode it as LOT packets"""
    PNG_START = bytes.fromhex("89504E470D0A1A0A")
    JPEG_START = bytes.fromhex("FFD8")
    JPEG_END = bytes.fromhex("FFD9")
    GIF87A_START = bytes.fromhex("474946383761")  # "GIF87a"
    GIF89A_START = bytes.fromhex("474946383961")  # "GIF89a"

    MIMEHASH_PNG = 0x4F328CA0
    MIMEHASH_JPEG = 0x1E653E9C
    MIMEHASH_TEXT = 0xBB492AAC
    MIMEHASH_GIF = 0x87A4FD95

    def __init__(self, filename="", lot_id=0, port=0x1001, expiry="45 minutes"):
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
        self.default_expiry = expiry
        self.current_part_index = 0
        self.command_buffer = bytes()

        # Initialize logger first before using it
        self.my_log = gr.logger(self.alias())

        # Now prepare the file (which may use the logger)
        with open(filename, "rb") as f:
            self.prepare_file(os.path.basename(filename), f.read(), lot_id, expiry)

    def parse_relative_time(self, time_str):
        """
        Parse relative time strings like '15 minutes', '2 weeks', '1 year', etc.
        Returns a timedelta object.
        """
        time_str = time_str.strip().lower()

        # Pattern: number + optional space + unit
        pattern = r'^(\d+(?:\.\d+)?)\s*(ms|mil|milliseconds?|s|sec|seconds?|m|min|minutes?|h|hr|hours?|d|days?|w|wk|weeks?|mn|month|months?|y|yr|year|years?)$'
        match = re.match(pattern, time_str)

        if not match:
            raise ValueError(f"Invalid relative time format: {time_str}")

        value = float(match.group(1))
        unit = match.group(2)

        # Convert to timedelta
        if unit in ('ms', 'mil', 'millisecond', 'milliseconds'):
            return timedelta(milliseconds=value)
        elif unit in ('s', 'sec', 'second', 'seconds'):
            return timedelta(seconds=value)
        elif unit in ('m', 'min', 'minute', 'minutes'):
            return timedelta(minutes=value)
        elif unit in ('h', 'hr', 'hour', 'hours'):
            return timedelta(hours=value)
        elif unit in ('d', 'day', 'days'):
            return timedelta(days=value)
        elif unit in ('w', 'wk', 'week', 'weeks'):
            return timedelta(weeks=value)
        elif unit in ('mn', 'month', 'months'):
            return timedelta(days=value * 30.44)  # Average month length
        elif unit in ('y', 'yr', 'year', 'years'):
            return timedelta(days=value * 365.25)  # Account for leap years
        else:
            raise ValueError(f"Unknown time unit: {unit}")

    def parse_expiry(self, expiry_str):
        """
        Parse expiry date/time. Supports:
        - TZ format: ISO 8601 datetime with timezone (e.g., '2026-12-31T23:59:59+00:00')
        - Relative time: '15 minutes', '2 weeks', '1 year', etc.
        Returns a datetime object in UTC.
        """
        if not expiry_str or expiry_str.strip() == "":
            # Default to 45 minutes
            return datetime.now(timezone.utc) + timedelta(minutes=45)

        expiry_str = expiry_str.strip()

        # Try parsing as ISO 8601 / TZ format first
        try:
            # Try various ISO 8601 formats
            for fmt in ['%Y-%m-%dT%H:%M:%S%z', '%Y-%m-%dT%H:%M:%S.%f%z',
                       '%Y-%m-%d %H:%M:%S%z', '%Y-%m-%dT%H:%M:%SZ']:
                try:
                    if fmt.endswith('Z'):
                        # Handle 'Z' suffix for UTC
                        dt = datetime.strptime(expiry_str, fmt).replace(tzinfo=timezone.utc)
                    else:
                        dt = datetime.strptime(expiry_str, fmt)
                    return dt.astimezone(timezone.utc)
                except ValueError:
                    continue
        except Exception:
            pass

        # Try parsing as relative time
        try:
            delta = self.parse_relative_time(expiry_str)
            return datetime.now(timezone.utc) + delta
        except ValueError as e:
            self.my_log.error(f"Failed to parse expiry time '{expiry_str}': {e}")
            # Default to 45 minutes
            return datetime.now(timezone.utc) + timedelta(minutes=45)

    def handle_new_file(self, msg):
        data = bytes(pmt.to_python(msg)[1])
        self.command_buffer += data

        command_end = self.command_buffer.find(b"\n")
        if command_end >= 0:
            command = self.command_buffer[:command_end]
            parts = command.split(b"|")

            if (parts[0] == b"streamfile") and (len(parts) >= 4):
                lot_id = int(parts[1])
                size = int(parts[2])
                filename = parts[3]
                expiry = parts[4].decode('utf-8') if len(parts) >= 5 else self.default_expiry
                if len(self.command_buffer) >= command_end + 1 + size:
                    filedata = self.command_buffer[command_end + 1:command_end + 1 + size]
                    self.prepare_file(filename, filedata, lot_id, expiry)
                    self.command_buffer = self.command_buffer[command_end + 1 + size:]
            elif (parts[0] == b"file") and (len(parts) >= 3):
                lot_id = int(parts[1])
                filename = parts[2]
                expiry = parts[3].decode('utf-8') if len(parts) >= 4 else self.default_expiry
                with open(filename, "rb") as f:
                    self.prepare_file(os.path.basename(filename), f.read(), lot_id, expiry)
                self.command_buffer = self.command_buffer[command_end + 1:]
            else:
                self.my_log.warn(f"Invalid command: {command}")
                self.command_buffer = self.command_buffer[command_end + 1:]

    def handle_notify(self, msg):
        port = pmt.to_python(msg)
        if port == self.port:
            self.send()

    def prepare_file(self, filename, data, lot_id, expiry_str=None):
        if isinstance(filename, str):
            filename = filename.encode()

        # Parse expiry time
        if expiry_str is None:
            expiry_str = self.default_expiry

        dt = self.parse_expiry(expiry_str)

        # Encode expiry in HD Radio format: YYYYMMMMDDDDDHHHHHHMMMMMM (26 bits)
        # Year: 12 bits (0-4095), Month: 4 bits (1-12), Day: 5 bits (1-31),
        # Hour: 5 bits (0-23), Minute: 6 bits (0-59), Second: not included
        expiry_encoded = (dt.year << 20) | (dt.month << 16) | (dt.day << 11) | (dt.hour << 6) | dt.minute

        self.my_log.info(f"LOT {lot_id}: Expiry set to {dt.isoformat()} (encoded: 0x{expiry_encoded:08x})")

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

                size = len(data)

                if data.startswith(self.PNG_START):
                    mime = self.MIMEHASH_PNG
                elif data.startswith(self.JPEG_START) and data.endswith(self.JPEG_END):
                    mime = self.MIMEHASH_JPEG
                elif data.startswith(self.GIF87A_START) or data.startswith(self.GIF89A_START):
                    mime = self.MIMEHASH_GIF
                elif filename.lower().endswith(b".png"):
                    mime = self.MIMEHASH_PNG
                elif filename.lower().endswith(b".jpg") or filename.lower().endswith(b".jpeg"):
                    mime = self.MIMEHASH_JPEG
                elif filename.lower().endswith(b".gif"):
                    mime = self.MIMEHASH_GIF
                elif filename.lower().endswith(b".txt"):
                    mime = self.MIMEHASH_TEXT
                else:
                    raise ValueError("Unsupported file type. Supported types: PNG, JPG, GIF, TXT.")

                header += struct.pack("<IIII", version, expiry_encoded, size, mime) + filename

            parts.append(header + chunk)

        self.filename = filename
        self.lot_id = lot_id
        self.parts = parts
        self.current_part_index = 0

    def start(self):
        self.aas_seq = 0
        self.send()

    def send(self):
        # Send the entire file to the queue immediately
        # The L2 encoder will manage the transmission rate
        # This eliminates the terrible round-trip delay bottleneck

        packets_sent = 0
        if self.current_part_index == 0:
            self.my_log.info(f"Sending LOT file {self.lot_id}: {self.filename} ({len(self.parts)} packets)")

        # Send all remaining packets
        while self.current_part_index < len(self.parts):
            part = self.parts[self.current_part_index]
            aas_pdu = struct.pack("<BHH", 0x21, self.port, self.aas_seq) + part
            msg = pmt.cons(pmt.make_dict(), pmt.init_u8vector(len(aas_pdu), list(aas_pdu)))
            self.message_port_pub(pmt.intern("aas"), msg)
            self.aas_seq = (self.aas_seq + 1) & 0xffff
            self.current_part_index += 1
            packets_sent += 1

        # When we finish the file, loop back to the beginning for continuous transmission
        if self.current_part_index >= len(self.parts):
            self.my_log.info(f"Completed sending LOT {self.lot_id}: sent {packets_sent} packets in this burst")
            self.current_part_index = 0
