#!/usr/bin/env python3
"""
HD Radio Data Bytes & Bitrate Calculator

Calculates maximum data bytes and audio bitrate allocations for HD Radio
Layer 2 encoders based on transmission mode, subchannel configuration,
album art/station logo sizes, and PSD metadata settings.

All calculations are derived from the gr-nrsc5 source code
(l1_fm_encoder_impl, l1_am_encoder_impl, l2_encoder_impl, hdc_encoder_impl,
psd_encoder_impl, lot_encoder).
"""

import argparse
import math
import re
import sys
import textwrap


# =============================================================================
# Constants from gr-nrsc5 source code
# =============================================================================

# L2 encoder constants (l2_encoder_impl.h)
RS_PARITY_LEN = 8
CONTROL_WORD_LEN = 6
HEF_LEN = 3
MAX_PROGRAMS = 8

# HDC encoder constants (hdc_encoder_impl.h)
HDC_SAMPLE_RATE = 44100
SAMPLES_PER_FRAME = 2048

# HDLC overhead: CRC16 (2 bytes) + flag (1 byte) + ~3% byte stuffing average
HDLC_OVERHEAD_BYTES = 3
HDLC_STUFFING_RATIO = 0.03

# Safety margin: the L2 encoder packs programs sequentially into the PDU.
# Program 0 gets first pick of all available space, then program 1, etc.
# fdk-aac CBR mode produces slightly variable frame sizes (typically +/- 1-2 bytes).
# With target_nop=32 frames per PDU, this variance can accumulate to ~32-64 extra bytes.
# Additionally, locator table size changes as nop changes, and partial frame splitting
# adds overhead. We reserve a per-program safety margin to prevent the last program
# from running out of space and falling behind.
SAFETY_MARGIN_PER_PROGRAM = 48  # bytes per program reserved as headroom

# LOT packet structure overhead
LOT_FIRST_HEADER_BASE = 24  # first packet header (before filename)
LOT_SUBSEQUENT_HEADER = 8   # subsequent packet headers
LOT_CHUNK_SIZE = 256         # max data per LOT packet
AAS_HEADER_SIZE = 5          # AAS packet header (0x21 + port + seq)

# BBM (Block Boundary Marker) overhead in fixed data subchannel
BBM_SIZE = 4
BBM_PERIOD = 255 + 4  # BBM repeats every 259 bytes in the data subchannel

# FM NRSC-5 OFDM parameters
FM_FFT_SIZE = 2048
FM_BLOCKS_PER_FRAME = 16
FM_SYMBOLS_PER_BLOCK = 32
FM_SYMBOLS_PER_FRAME = FM_BLOCKS_PER_FRAME * FM_SYMBOLS_PER_BLOCK  # 512
FM_SAMPLE_RATE = 744187.5  # Complex baseband sample rate (Hz)
FM_FRAME_DURATION = (FM_SYMBOLS_PER_FRAME * FM_FFT_SIZE) / FM_SAMPLE_RATE  # ~1.4091s

# AM NRSC-5 OFDM parameters
AM_FFT_SIZE = 256
AM_BLOCKS_PER_FRAME = 8
AM_SYMBOLS_PER_BLOCK = 32
AM_SYMBOLS_PER_FRAME = AM_BLOCKS_PER_FRAME * AM_SYMBOLS_PER_BLOCK  # 256
AM_SAMPLE_RATE = 46511.71875  # Complex baseband sample rate (Hz)
AM_FRAME_DURATION = (AM_SYMBOLS_PER_FRAME * AM_FFT_SIZE) / AM_SAMPLE_RATE  # ~1.4091s

# HDC audio frame duration
HDC_FRAME_DURATION = SAMPLES_PER_FRAME / HDC_SAMPLE_RATE  # ~0.04644s

# =============================================================================
# Mode definitions from L1 encoder source code
# =============================================================================

# FM modes: { mode_name: { port_name: (frame_size_bits, pdus_per_l1_frame) } }
FM_MODES = {
    "MP1": {
        "P1": (146176, 1),
    },
    "MP2": {
        "P1": (146176, 1),
        "P3": (2304, 8),
    },
    "MP3": {
        "P1": (146176, 1),
        "P3": (4608, 8),
    },
    "MP5": {
        "P1": (4608, 8),
        "P2": (109312, 1),
        "P3": (4608, 8),
    },
    "MP6": {
        "P1": (9216, 8),
        "P2": (72448, 1),
    },
    "MP11": {
        "P1": (146176, 1),
        "P3": (4608, 8),
        "P4": (4608, 8),
    },
}

# AM modes: { mode_name: { port_name: (frame_size_bits, pdus_per_l1_frame) } }
AM_MODES = {
    "MA1": {
        "P1": (3750, 1),
        "P3": (24000, 1),
    },
    "MA3": {
        "P1": (3750, 1),
        "P3": (30000, 1),
    },
}

# Maximum recommended audio bitrates per PDU size (from l2_encoder_impl.cc error handler)
MAX_BITRATE_BY_SIZE = {
    146176: 96,
    109312: 72,
    72448: 48,
    30000: 32,
    24000: 32,
    9216: 24,
    4608: 12,
    3750: 10,
    2304: 6,
}

# L2 encoder mode parameters by PDU size (from l2_encoder_impl.cc constructor)
def get_l2_params(size):
    """Get L2 encoder parameters based on PDU size in bits."""
    if size in (146176, 109312, 72448, 30000, 24000):
        return {
            "target_nop": 32,
            "lc_bits": 16,
            "psd_bytes": 128,
            "pdu_seq_len": 2,
            "mode_type": "audio",
        }
    elif size in (9216, 4608, 3750, 2304):
        return {
            "target_nop": 4,
            "lc_bits": 12,
            "psd_bytes": 8,
            "pdu_seq_len": 8,
            "mode_type": "data",
        }
    else:
        raise ValueError(f"Unknown PDU size: {size} bits")


def len_locators(lc_bits, nop):
    """Calculate locator table size (from l2_encoder_impl.cc:len_locators)."""
    return ((lc_bits * nop) + 4) // 8


def payload_bytes(size):
    """Calculate payload bytes from PDU size in bits (from l2_encoder_impl.cc)."""
    return (size - 22) // 8


def get_frame_duration(mode_name):
    """Get L1 frame duration in seconds."""
    if mode_name.startswith("MP"):
        return FM_FRAME_DURATION
    elif mode_name.startswith("MA"):
        return AM_FRAME_DURATION
    else:
        raise ValueError(f"Unknown mode: {mode_name}")


def get_pdu_rate(mode_name, port_name):
    """Get PDU production rate in PDUs per second for a given port."""
    if mode_name.startswith("MP"):
        modes = FM_MODES
    else:
        modes = AM_MODES

    if mode_name not in modes:
        raise ValueError(f"Unknown mode: {mode_name}")
    if port_name not in modes[mode_name]:
        raise ValueError(f"Port {port_name} not available in mode {mode_name}")

    _, pdus_per_frame = modes[mode_name][port_name]
    frame_dur = get_frame_duration(mode_name)
    return pdus_per_frame / frame_dur


def calculate_per_program_overhead(l2_params, nop, psd_bytes=None):
    """Calculate per-program overhead in bytes within a PDU.

    Args:
        l2_params: L2 encoder parameters dict
        nop: Number of packets (frames) per PDU
        psd_bytes: Override PSD bytes for this program (uses l2_params default if None)
    """
    if psd_bytes is None:
        psd_bytes = l2_params["psd_bytes"]
    return (RS_PARITY_LEN +
            CONTROL_WORD_LEN +
            len_locators(l2_params["lc_bits"], nop) +
            HEF_LEN +
            psd_bytes)


def calculate_total_data_width(data_bytes, ccc_width=24):
    """Calculate total data subchannel width (from l2_encoder_impl.cc)."""
    if data_bytes > 0:
        return data_bytes + ccc_width + 1  # +1 for sync channel
    return 0


def calculate_data_throughput(data_bytes, pdu_rate, ccc_width=24):
    """
    Calculate actual usable data throughput in bytes/sec and bits/sec.

    Accounts for:
    - BBM overhead (4 bytes every 259 byte positions)
    - HDLC framing overhead on AAS packets (CRC + flag + stuffing)
    """
    if data_bytes <= 0:
        return 0.0, 0.0

    # Usable bytes per frame after BBM overhead
    # BBM occupies 4 bytes every BBM_PERIOD (259) byte positions
    # Calculate how many BBM bytes fall within data_bytes positions per frame
    # The BBM counter is persistent across frames (aas_block_offset), so on average:
    bbm_bytes_per_frame = data_bytes * BBM_SIZE / BBM_PERIOD
    usable_bytes_per_frame = data_bytes - bbm_bytes_per_frame

    # Raw data throughput (before HDLC overhead)
    raw_bytes_per_sec = usable_bytes_per_frame * pdu_rate

    # Account for HDLC overhead on the AAS packets being transmitted
    # Each AAS packet has: 5-byte AAS header + payload + 2-byte CRC + 1-byte flag + stuffing
    # The HDLC overhead is approximately 3 + ~3% stuffing bytes per packet
    # For continuous streaming, average overhead per byte is about 3-5%
    effective_bytes_per_sec = raw_bytes_per_sec * (1.0 - HDLC_STUFFING_RATIO)

    return effective_bytes_per_sec, effective_bytes_per_sec * 8


def calculate_lot_transfer_time(file_size_bytes, data_throughput_bytes_sec, num_ports=1, filename_len=16):
    """
    Calculate time to transfer a LOT file (album art or station logo).

    LOT packets: first has ~24+filename header, subsequent have 8-byte header.
    Each carries up to 256 bytes of data. HDLC + AAS overhead per packet.
    """
    if data_throughput_bytes_sec <= 0:
        return float('inf')

    # Calculate number of LOT packets needed
    first_header = LOT_FIRST_HEADER_BASE + filename_len
    first_data = max(0, LOT_CHUNK_SIZE - first_header)

    if file_size_bytes <= first_data:
        num_packets = 1
    else:
        remaining = file_size_bytes - first_data
        data_per_subsequent = LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER
        num_packets = 1 + math.ceil(remaining / data_per_subsequent)

    # Total bytes to transmit (including all headers, AAS headers, HDLC overhead)
    total_bytes = 0
    for i in range(num_packets):
        if i == 0:
            packet_size = min(LOT_CHUNK_SIZE, first_header + file_size_bytes)
        else:
            remaining = file_size_bytes - first_data - (i - 1) * (LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER)
            data_in_packet = min(LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER, remaining)
            packet_size = LOT_SUBSEQUENT_HEADER + data_in_packet

        # Add AAS header
        aas_packet_size = AAS_HEADER_SIZE + packet_size
        # Add HDLC overhead (CRC + flag + stuffing)
        hdlc_size = aas_packet_size + HDLC_OVERHEAD_BYTES + int(aas_packet_size * HDLC_STUFFING_RATIO)
        total_bytes += hdlc_size

    # If there are multiple ports sharing the data subchannel, throughput is shared
    effective_throughput = data_throughput_bytes_sec / num_ports if num_ports > 0 else data_throughput_bytes_sec

    return total_bytes / effective_throughput if effective_throughput > 0 else float('inf')


def calculate_max_audio_bitrate(size, num_progs, data_bytes, ccc_width=24, psd_bytes_per_prog=None):
    """
    Calculate the maximum total audio bitrate for a given L2 encoder configuration.

    Returns the max bitrate in kbps that all programs combined can sustain.

    The L2 encoder packs programs sequentially into the PDU buffer. Each program
    consumes overhead + audio data, and the next program gets whatever remains.
    We include a safety margin per program to account for fdk-aac frame size
    variance (CBR is not perfectly constant) and locator table size changes.

    Args:
        psd_bytes_per_prog: list of PSD bytes per program (uses l2_params default if None)
    """
    l2_params = get_l2_params(size)
    pb = payload_bytes(size)
    tdw = calculate_total_data_width(data_bytes, ccc_width)

    # Space available for all programs
    space_for_programs = pb - tdw

    # Per-program overhead with target_nop
    nop = l2_params["target_nop"]

    # Calculate total overhead for all programs (may differ per program)
    total_overhead = 0
    for i in range(num_progs):
        prog_psd = psd_bytes_per_prog[i] if psd_bytes_per_prog else None
        total_overhead += calculate_per_program_overhead(l2_params, nop, prog_psd)

    # Reserve safety margin per program for HDC frame size variance
    total_safety = SAFETY_MARGIN_PER_PROGRAM * num_progs
    effective_space = space_for_programs - total_safety - total_overhead

    if effective_space <= 0:
        return 0.0

    # Audio bytes available per program per PDU (evenly split)
    audio_bytes_per_prog = effective_space // num_progs

    if audio_bytes_per_prog <= 0:
        return 0.0

    # Each audio frame in the PDU has 1 extra CRC8 byte appended.
    # bytes_per_hdc_frame = bitrate * SAMPLES_PER_FRAME / HDC_SAMPLE_RATE / 8
    # audio_bytes_per_prog = nop * (bytes_per_hdc_frame + 1)
    bytes_per_hdc_frame = (audio_bytes_per_prog / nop) - 1  # subtract CRC8 byte per frame
    if bytes_per_hdc_frame <= 0:
        return 0.0

    bitrate_per_prog = bytes_per_hdc_frame * HDC_SAMPLE_RATE * 8 / SAMPLES_PER_FRAME
    total_bitrate = bitrate_per_prog * num_progs

    return total_bitrate / 1000.0  # Convert to kbps


def calculate_max_data_bytes(size, num_progs, target_bitrates_kbps, ccc_width=24, psd_bytes_per_prog=None):
    """
    Calculate the maximum data_bytes given desired audio bitrates per program.

    target_bitrates_kbps: list of bitrate in kbps for each program
    Returns maximum data_bytes that still allows the target audio bitrates.

    Includes a safety margin per program for HDC frame size variance.

    Args:
        psd_bytes_per_prog: list of PSD bytes per program (uses l2_params default if None)
    """
    l2_params = get_l2_params(size)
    pb = payload_bytes(size)
    nop = l2_params["target_nop"]

    # Calculate total audio space needed
    total_audio_space_needed = 0
    for i, bitrate_kbps in enumerate(target_bitrates_kbps):
        bitrate_bps = bitrate_kbps * 1000
        bytes_per_hdc_frame = bitrate_bps * SAMPLES_PER_FRAME / HDC_SAMPLE_RATE / 8
        # Each audio frame in PDU has +1 CRC8 byte
        audio_space = nop * (bytes_per_hdc_frame + 1)
        prog_psd = psd_bytes_per_prog[i] if psd_bytes_per_prog else None
        overhead = calculate_per_program_overhead(l2_params, nop, prog_psd)
        total_audio_space_needed += audio_space + overhead

    # Add safety margin for HDC frame size variance
    total_safety = SAFETY_MARGIN_PER_PROGRAM * num_progs

    # Maximum data width
    max_data_width = pb - total_audio_space_needed - total_safety
    if max_data_width <= 0:
        return 0

    # total_data_width = data_bytes + ccc_width + 1
    max_data_bytes = int(max_data_width - ccc_width - 1)
    return max(0, max_data_bytes)


def parse_subchannels(subchannel_str):
    """
    Parse subchannel specification string.

    Format: "HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps"
    Or:     "HD1,Stereo - HD2,Stereo - HD3,Stereo" (bitrate omitted)

    Returns list of dicts with keys: name, channels (1 or 2), bitrate_kbps (or None)
    """
    subchannels = []
    parts = [p.strip() for p in subchannel_str.split("-")]
    for part in parts:
        fields = [f.strip() for f in part.split(",")]
        if len(fields) < 2:
            raise ValueError(f"Invalid subchannel spec: '{part}'. Expected at least 'HDx,Stereo/Mono'")

        name = fields[0].strip().upper()
        mode_str = fields[1].strip().lower()

        if mode_str in ("stereo", "2ch", "2"):
            channels = 2
        elif mode_str in ("mono", "1ch", "1"):
            channels = 1
        else:
            raise ValueError(f"Invalid audio mode: '{mode_str}'. Use 'Stereo' or 'Mono'")

        bitrate_kbps = None
        if len(fields) >= 3:
            br_str = fields[2].strip().lower().replace("kbps", "").replace("k", "")
            try:
                bitrate_kbps = float(br_str)
            except ValueError:
                raise ValueError(f"Invalid bitrate: '{fields[2]}'. Use format like '32kbps' or '32'")

        subchannels.append({
            "name": name,
            "channels": channels,
            "bitrate_kbps": bitrate_kbps,
        })

    return subchannels


def parse_images(image_str):
    """
    Parse album art/station logo specification string.

    Format: "HD1, Station Logo 3kb, Album Art 8kb - HD2, Station Logo 3kb, Album Art 7kb"

    Returns dict: { "HD1": {"station_logo_kb": 3, "album_art_kb": 8}, ... }
    """
    images = {}
    parts = [p.strip() for p in image_str.split("-")]
    for part in parts:
        fields = [f.strip() for f in part.split(",")]
        if len(fields) < 2:
            raise ValueError(f"Invalid image spec: '{part}'")

        name = fields[0].strip().upper()
        images[name] = {"station_logo_kb": 0, "album_art_kb": 0}

        for field in fields[1:]:
            field_lower = field.strip().lower()
            # Extract size value
            size_val = None
            for word in field_lower.split():
                cleaned = word.replace("kb", "").replace("k", "")
                try:
                    size_val = float(cleaned)
                except ValueError:
                    continue

            if size_val is None:
                raise ValueError(f"Could not parse size from: '{field}'")

            if "logo" in field_lower or "station" in field_lower:
                images[name]["station_logo_kb"] = size_val
            elif "art" in field_lower or "album" in field_lower:
                images[name]["album_art_kb"] = size_val
            else:
                raise ValueError(f"Unknown image type in: '{field}'. Use 'Station Logo' or 'Album Art'")

    return images


def parse_psd_config(psd_str):
    """
    Parse PSD (metadata) present string.

    Format: "HD1,true,128 - HD2,true,128 - HD3,true,128"
    Or:     "HD1,true - HD2,true - HD3,true"  (bytes/frame defaults to 128 for audio, 8 for data modes)

    Returns dict: { "HD1": {"present": True, "bytes_frame_limit": 128}, ... }
    """
    psd = {}
    parts = [p.strip() for p in psd_str.split("-")]
    for part in parts:
        fields = [f.strip() for f in part.split(",")]
        if len(fields) < 2:
            raise ValueError(f"Invalid PSD spec: '{part}'")

        name = fields[0].strip().upper()
        present = fields[1].strip().lower() in ("true", "yes", "1", "on")
        bytes_limit = None
        if len(fields) >= 3:
            try:
                bytes_limit = int(fields[2].strip())
            except ValueError:
                raise ValueError(f"Invalid PSD bytes/frame limit: '{fields[2]}'")

        psd[name] = {"present": present, "bytes_frame_limit": bytes_limit}

    return psd


def parse_percentages(pct_str):
    """Parse comma-separated percentages. Returns list of floats."""
    return [float(p.strip()) for p in pct_str.split(",")]


def parse_time_value(time_str):
    """
    Parse a flexible time string into seconds.

    Supported formats:
      Seconds:      "34s", "34", "34seconds", "34sec", "34 seconds", "34 sec",
                    "34 s", "0.09s", "0.09"
      Minutes:      "1.5min", "1min30sec", "1.5 minutes", "1.5m", "1.5 min"
      Milliseconds: "90ms", "90 miliseconds", "90 ms", "90 mil", "90 mili"
      Colon format: "0:34" (m:ss), "00:34" (m:ss), "1:30" (m:ss),
                    "00:00:34" (h:mm:ss), "00:01:30" (h:mm:ss),
                    "0:34:00" (h:mm:ss), "01:30:00" (h:mm:ss),
                    "00:00:34:00" (h:mm:ss:ms), "00:01:30:00" (h:mm:ss:ms)
    """
    original = time_str
    s = time_str.strip()

    # Try colon-separated format first: could be m:ss, h:mm:ss, or h:mm:ss:ms
    if ":" in s:
        parts = s.split(":")
        try:
            parts = [float(p) for p in parts]
        except ValueError:
            raise ValueError(f"Invalid time format: '{original}'")

        if len(parts) == 2:
            # m:ss
            return parts[0] * 60 + parts[1]
        elif len(parts) == 3:
            # h:mm:ss
            return parts[0] * 3600 + parts[1] * 60 + parts[2]
        elif len(parts) == 4:
            # h:mm:ss:ms
            return parts[0] * 3600 + parts[1] * 60 + parts[2] + parts[3] / 1000.0
        else:
            raise ValueError(f"Invalid time format: '{original}'")

    # Try compound format like "1min30sec"
    compound_match = re.match(
        r'^(\d+(?:\.\d+)?)\s*(?:min(?:utes?)?|m)\s*(\d+(?:\.\d+)?)\s*(?:sec(?:onds?)?|s)$',
        s, re.IGNORECASE
    )
    if compound_match:
        return float(compound_match.group(1)) * 60 + float(compound_match.group(2))

    # Try milliseconds: "90ms", "90 miliseconds", "90 ms", "90 mil", "90 mili"
    ms_match = re.match(
        r'^(\d+(?:\.\d+)?)\s*(?:ms|mil(?:i(?:seconds?)?)?|miliseconds?)$',
        s, re.IGNORECASE
    )
    if ms_match:
        return float(ms_match.group(1)) / 1000.0

    # Try minutes: "1.5min", "1.5 minutes", "1.5m", "1.5 min"
    min_match = re.match(
        r'^(\d+(?:\.\d+)?)\s*(?:min(?:utes?)?|m)$',
        s, re.IGNORECASE
    )
    if min_match:
        return float(min_match.group(1)) * 60

    # Try seconds: "34s", "34seconds", "34sec", "34 seconds", "34 sec", "34 s"
    sec_match = re.match(
        r'^(\d+(?:\.\d+)?)\s*(?:sec(?:onds?)?|s)$',
        s, re.IGNORECASE
    )
    if sec_match:
        return float(sec_match.group(1))

    # Try bare number (interpreted as seconds)
    try:
        return float(s)
    except ValueError:
        raise ValueError(
            f"Could not parse time value: '{original}'. "
            f"Use formats like '34s', '1.5min', '90ms', '1:30', '0:34', etc."
        )


def parse_transfer_speed(speed_str):
    """
    Parse target image transfer speed specification string.

    Format: "HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s"

    Returns dict: { "HD1": {"station_logo_sec": 24.0, "album_art_sec": 30.0}, ... }
    """
    speeds = {}
    parts = [p.strip() for p in speed_str.split("-")]
    for part in parts:
        fields = [f.strip() for f in part.split(",")]
        if len(fields) < 2:
            raise ValueError(f"Invalid transfer speed spec: '{part}'")

        name = fields[0].strip().upper()
        speeds[name] = {"station_logo_sec": None, "album_art_sec": None}

        for field in fields[1:]:
            field_lower = field.strip().lower()

            # Split into label and time value
            # Find where the time value starts (first digit or colon after label text)
            time_val = None
            if "logo" in field_lower or "station" in field_lower:
                # Extract everything after "logo" or "station logo"
                match = re.search(r'(?:station\s+)?logo\s+(.+)', field.strip(), re.IGNORECASE)
                if match:
                    time_val = parse_time_value(match.group(1))
                speeds[name]["station_logo_sec"] = time_val
            elif "art" in field_lower or "album" in field_lower:
                match = re.search(r'(?:album\s+)?art\s+(.+)', field.strip(), re.IGNORECASE)
                if match:
                    time_val = parse_time_value(match.group(1))
                speeds[name]["album_art_sec"] = time_val
            else:
                raise ValueError(
                    f"Unknown image type in: '{field}'. Use 'Station Logo' or 'Album Art'"
                )

            if time_val is None:
                raise ValueError(f"Could not parse transfer time from: '{field}'")

    return speeds


def calculate_data_bytes_for_transfer_targets(transfer_speeds, images, pdu_rate, pb, ccc_width=24):
    """
    Calculate the minimum data_bytes needed to achieve target transfer times.

    For each image, calculates the throughput needed, then derives the data_bytes
    value that provides enough aggregate throughput for all images.
    """
    if not transfer_speeds or not images:
        return None

    # Count total active LOT ports (for round-robin sharing)
    active_ports = 0
    for name in transfer_speeds:
        if name in images:
            img = images[name]
            ts = transfer_speeds[name]
            if img.get("station_logo_kb", 0) > 0 and ts.get("station_logo_sec") is not None:
                active_ports += 1
            if img.get("album_art_kb", 0) > 0 and ts.get("album_art_sec") is not None:
                active_ports += 1
    # Add SIG port
    if active_ports > 0:
        active_ports += 1

    if active_ports == 0:
        return None

    # For each image, calculate the raw throughput needed on the data subchannel
    # to achieve the target transfer time.
    # Since ports share via round-robin, the total throughput must be enough
    # that each port's share meets its target.
    # throughput_per_port = total_throughput / active_ports
    # For a given port to transfer file_size in target_time:
    #   lot_total_bytes / (throughput / active_ports) <= target_time
    #   lot_total_bytes * active_ports / throughput <= target_time
    #   throughput >= lot_total_bytes * active_ports / target_time

    max_needed_throughput = 0.0  # bytes/sec

    for name in transfer_speeds:
        if name not in images:
            continue
        img = images[name]
        ts = transfer_speeds[name]

        for img_type, ts_key, size_key in [
            ("station_logo", "station_logo_sec", "station_logo_kb"),
            ("album_art", "album_art_sec", "album_art_kb"),
        ]:
            target_sec = ts.get(ts_key)
            size_kb = img.get(size_key, 0)
            if target_sec is None or target_sec <= 0 or size_kb <= 0:
                continue

            file_size_bytes = int(size_kb * 1024)

            # Calculate total LOT bytes (with all overhead)
            filename_len = 16
            first_header = LOT_FIRST_HEADER_BASE + filename_len
            first_data = max(0, LOT_CHUNK_SIZE - first_header)

            if file_size_bytes <= first_data:
                num_packets = 1
            else:
                remaining = file_size_bytes - first_data
                data_per_subsequent = LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER
                num_packets = 1 + math.ceil(remaining / data_per_subsequent)

            total_lot_bytes = 0
            for i in range(num_packets):
                if i == 0:
                    packet_size = min(LOT_CHUNK_SIZE, first_header + file_size_bytes)
                else:
                    rem = file_size_bytes - first_data - (i - 1) * (LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER)
                    data_in_pkt = min(LOT_CHUNK_SIZE - LOT_SUBSEQUENT_HEADER, rem)
                    packet_size = LOT_SUBSEQUENT_HEADER + data_in_pkt
                aas_packet_size = AAS_HEADER_SIZE + packet_size
                hdlc_size = aas_packet_size + HDLC_OVERHEAD_BYTES + int(aas_packet_size * HDLC_STUFFING_RATIO)
                total_lot_bytes += hdlc_size

            # Throughput needed so this port finishes within target_sec
            # With round-robin: effective_throughput_for_port = total_throughput / active_ports
            # total_lot_bytes / (total_throughput / active_ports) = target_sec
            # total_throughput = total_lot_bytes * active_ports / target_sec
            needed = total_lot_bytes * active_ports / target_sec
            if needed > max_needed_throughput:
                max_needed_throughput = needed

    if max_needed_throughput <= 0:
        return None

    # Reverse the throughput calculation to find data_bytes:
    # effective_bytes_per_sec = usable_bytes_per_frame * pdu_rate * (1 - HDLC_STUFFING_RATIO)
    # usable_bytes_per_frame = data_bytes - data_bytes * BBM_SIZE / BBM_PERIOD
    #                        = data_bytes * (1 - BBM_SIZE / BBM_PERIOD)
    # So: max_needed_throughput = data_bytes * (1 - BBM_SIZE/BBM_PERIOD) * pdu_rate * (1 - HDLC_STUFFING_RATIO)
    # data_bytes = max_needed_throughput / ((1 - BBM_SIZE/BBM_PERIOD) * pdu_rate * (1 - HDLC_STUFFING_RATIO))

    bbm_factor = 1.0 - BBM_SIZE / BBM_PERIOD
    denominator = bbm_factor * pdu_rate * (1.0 - HDLC_STUFFING_RATIO)

    if denominator <= 0:
        return None

    data_bytes = int(math.ceil(max_needed_throughput / denominator))
    data_bytes = max(data_bytes, 1)
    # Cap at 50% of payload to leave room for audio
    data_bytes = min(data_bytes, pb // 2)

    return data_bytes


def allocate_bitrates(total_bitrate_kbps, subchannels, prioritize_name=None, percentages=None):
    """
    Allocate bitrates across subchannels.

    If percentages are given, use those.
    If prioritize_name is given, give it more bandwidth.
    Otherwise, distribute evenly.

    Returns list of bitrate_kbps per subchannel.
    """
    num = len(subchannels)
    if percentages:
        if len(percentages) != num:
            raise ValueError(f"Expected {num} percentages, got {len(percentages)}")
        if abs(sum(percentages) - 100) > 0.5:
            raise ValueError(f"Percentages must sum to 100, got {sum(percentages)}")
        return [total_bitrate_kbps * p / 100.0 for p in percentages]

    if prioritize_name:
        # Give prioritized subchannel 50% of bandwidth, rest split evenly
        bitrates = []
        for sc in subchannels:
            if sc["name"] == prioritize_name.upper():
                bitrates.append(total_bitrate_kbps * 0.5)
            else:
                bitrates.append(total_bitrate_kbps * 0.5 / (num - 1) if num > 1 else total_bitrate_kbps)
        return bitrates

    # Even distribution
    return [total_bitrate_kbps / num] * num


def round_bitrate_down(bitrate_kbps):
    """
    Round bitrate down to nearest HDC-compatible value.

    The HDC encoder uses: bytes_per_frame = bitrate * SAMPLES_PER_FRAME / HDC_SAMPLE_RATE / 8
    This must be an integer, so valid bitrates are multiples of:
    HDC_SAMPLE_RATE * 8 / SAMPLES_PER_FRAME = 44100 * 8 / 2048 = 172.265625 bps ≈ 0.172 kbps
    """
    bps = bitrate_kbps * 1000
    quantum = HDC_SAMPLE_RATE * 8 / SAMPLES_PER_FRAME  # ~172.27 bps
    rounded_bps = int(bps / quantum) * quantum
    return rounded_bps / 1000.0


def format_time(seconds):
    """Format seconds into a human-readable string."""
    if seconds == float('inf'):
        return "never (no data bandwidth)"
    if seconds < 1:
        return f"{seconds*1000:.0f}ms"
    elif seconds < 60:
        return f"{seconds:.1f}s"
    elif seconds < 3600:
        return f"{seconds/60:.1f}min"
    else:
        return f"{seconds/3600:.1f}hr"


def format_size(bytes_val):
    """Format bytes into human-readable string."""
    if bytes_val < 1024:
        return f"{bytes_val:.0f} bytes"
    elif bytes_val < 1024 * 1024:
        return f"{bytes_val/1024:.1f} KB"
    else:
        return f"{bytes_val/(1024*1024):.1f} MB"


# =============================================================================
# Main calculation and output
# =============================================================================

def run_calculation(args):
    """Main calculation logic."""
    # Determine mode tables
    mode_name = args.mode.upper()
    if mode_name.startswith("MP"):
        modes = FM_MODES
        band = "FM"
    elif mode_name.startswith("MA"):
        modes = AM_MODES
        band = "AM"
    else:
        print(f"Error: Unknown mode '{mode_name}'. Use MP1-MP11 (FM) or MA1/MA3 (AM).")
        sys.exit(1)

    if mode_name not in modes:
        print(f"Error: Mode '{mode_name}' is not supported. Available {band} modes: {', '.join(modes.keys())}")
        sys.exit(1)

    # Determine port
    port_name = args.port.upper()
    if port_name not in modes[mode_name]:
        available = ", ".join(modes[mode_name].keys())
        print(f"Error: Port '{port_name}' not available in {mode_name}. Available ports: {available}")
        sys.exit(1)

    frame_size_bits, pdus_per_frame = modes[mode_name][port_name]
    l2_params = get_l2_params(frame_size_bits)
    pb = payload_bytes(frame_size_bits)
    pdu_rate = get_pdu_rate(mode_name, port_name)
    frame_dur = get_frame_duration(mode_name)
    ccc_width = args.ccc_width

    # Parse subchannels
    subchannels = parse_subchannels(args.subchannels)
    num_progs = len(subchannels)

    if num_progs > MAX_PROGRAMS:
        print(f"Error: Maximum {MAX_PROGRAMS} programs supported, got {num_progs}")
        sys.exit(1)

    # Parse images
    images = {}
    if args.images:
        images = parse_images(args.images)

    # Parse PSD config
    psd_config = {}
    if args.psd_data_present:
        psd_config = parse_psd_config(args.psd_data_present)

    # Parse target image transfer speeds
    transfer_speeds = {}
    if args.target_image_transfer_speed:
        transfer_speeds = parse_transfer_speed(args.target_image_transfer_speed)

    # Apply PSD bytes/frame limits
    effective_psd_bytes = l2_params["psd_bytes"]
    psd_bytes_overrides = {}
    for sc in subchannels:
        if sc["name"] in psd_config:
            cfg = psd_config[sc["name"]]
            if not cfg["present"]:
                psd_bytes_overrides[sc["name"]] = 0
            elif cfg["bytes_frame_limit"] is not None:
                psd_bytes_overrides[sc["name"]] = cfg["bytes_frame_limit"]
            else:
                psd_bytes_overrides[sc["name"]] = effective_psd_bytes
        else:
            psd_bytes_overrides[sc["name"]] = effective_psd_bytes

    # Build ordered list of PSD bytes per program (for passing to calculation functions)
    psd_bytes_list = [psd_bytes_overrides[sc["name"]] for sc in subchannels]

    # Calculate total audio bitrate capacity
    # If bitrates are explicitly specified, check if they fit
    # If not specified, calculate maximum
    explicit_bitrates = all(sc["bitrate_kbps"] is not None for sc in subchannels)

    # Estimate data needs from images (used for auto data_bytes calculation)
    total_image_bytes_per_cycle = 0
    target_cycle_time = 60.0  # Target: send all images within 60 seconds
    num_data_ports = 0  # Number of LOT ports sharing the data subchannel

    for sc in subchannels:
        if sc["name"] in images:
            img = images[sc["name"]]
            if img["album_art_kb"] > 0:
                total_image_bytes_per_cycle += img["album_art_kb"] * 1024
                num_data_ports += 1
            if img["station_logo_kb"] > 0:
                total_image_bytes_per_cycle += img["station_logo_kb"] * 1024
                num_data_ports += 1

    for sc in subchannels:
        if sc["name"] in psd_config and psd_config[sc["name"]]["present"]:
            num_data_ports += 1

    if explicit_bitrates:
        target_bitrates = [sc["bitrate_kbps"] for sc in subchannels]
    else:
        target_bitrates = None  # Will be calculated

    # Handle percentages and prioritization
    percentages = None
    if args.prioritize_subchannel_percentage:
        percentages = parse_percentages(args.prioritize_subchannel_percentage)

    # Calculate data_bytes if not specified
    if args.data_bytes is not None:
        data_bytes = args.data_bytes
    elif transfer_speeds and images:
        # Use target transfer speed to determine data_bytes
        speed_data_bytes = calculate_data_bytes_for_transfer_targets(
            transfer_speeds, images, pdu_rate, pb, ccc_width
        )
        if speed_data_bytes is not None:
            data_bytes = speed_data_bytes
        else:
            data_bytes = 0
    else:
        # Estimate reasonable data_bytes based on image needs
        if total_image_bytes_per_cycle > 0:
            # Target: transfer all images within target_cycle_time
            needed_throughput = total_image_bytes_per_cycle / target_cycle_time
            # Account for HDLC, BBM, and LOT overhead (~30% overhead)
            needed_raw_throughput = needed_throughput * 1.3
            # data_bytes needed: raw_throughput / pdu_rate
            data_bytes = int(math.ceil(needed_raw_throughput / pdu_rate))
            data_bytes = max(data_bytes, 64)  # Minimum reasonable data_bytes
            data_bytes = min(data_bytes, pb // 4)  # Don't use more than 25% of payload
        else:
            data_bytes = 0

    tdw = calculate_total_data_width(data_bytes, ccc_width)

    # Calculate data throughput
    data_throughput_bytes, data_throughput_bps = calculate_data_throughput(
        data_bytes, pdu_rate, ccc_width
    )

    # If target bitrates not yet determined, calculate max and allocate
    max_total_bitrate = calculate_max_audio_bitrate(frame_size_bits, num_progs, data_bytes, ccc_width, psd_bytes_list)

    if target_bitrates is None:
        # Cap max total bitrate at the per-program max recommended for this PDU size
        max_recommended = MAX_BITRATE_BY_SIZE.get(frame_size_bits, 0)
        if max_recommended > 0:
            max_total_capped = min(max_total_bitrate, max_recommended * num_progs)
        else:
            max_total_capped = max_total_bitrate
        total_for_allocation = max_total_capped
        target_bitrates = allocate_bitrates(
            total_for_allocation, subchannels,
            args.prioritize_subchannel, percentages
        )
        # Round down to HDC-compatible values and cap per-program at max recommended
        target_bitrates = [round_bitrate_down(br) for br in target_bitrates]
        if max_recommended > 0:
            target_bitrates = [min(br, round_bitrate_down(max_recommended)) for br in target_bitrates]

    # If bitrates were explicit and no data_bytes/transfer targets specified,
    # calculate max data_bytes from remaining space
    if explicit_bitrates and args.data_bytes is None and not transfer_speeds:
        data_bytes = calculate_max_data_bytes(frame_size_bits, num_progs, target_bitrates, ccc_width, psd_bytes_list)
        tdw = calculate_total_data_width(data_bytes, ccc_width)
        data_throughput_bytes, data_throughput_bps = calculate_data_throughput(
            data_bytes, pdu_rate, ccc_width
        )

    # Verify bitrates fit
    total_target = sum(target_bitrates)
    check_bitrates = [br for br in target_bitrates]
    check_max = calculate_max_data_bytes(frame_size_bits, num_progs, check_bitrates, ccc_width, psd_bytes_list)

    # Count active LOT/PSD ports for transfer time calculation
    active_data_ports = 0
    for sc in subchannels:
        if sc["name"] in images:
            img = images[sc["name"]]
            if img["album_art_kb"] > 0:
                active_data_ports += 1
            if img["station_logo_kb"] > 0:
                active_data_ports += 1
    # PSD data also transits through the data subchannel in some configurations,
    # but PSD actually goes through a separate L2 input, not the fixed data subchannel.
    # Only LOT (album art/station logos) and SIG use the fixed data subchannel.
    # Add 1 for SIG port
    if active_data_ports > 0:
        active_data_ports += 1  # SIG port

    # ==========================================================================
    # Output results
    # ==========================================================================
    print()
    print("=" * 78)
    print("  HD Radio Data Bytes & Bitrate Calculator")
    print("=" * 78)
    print()

    # Mode info
    print(f"  Transmission Mode:  {mode_name} ({band})")
    print(f"  L2 Port:            {port_name}")
    print(f"  PDU Size:           {frame_size_bits} bits ({pb} payload bytes)")
    print(f"  PDUs per L1 Frame:  {pdus_per_frame}")
    print(f"  L1 Frame Duration:  {frame_dur*1000:.1f} ms")
    print(f"  PDU Rate:           {pdu_rate:.3f} PDUs/sec")
    print(f"  L2 Mode:            {'Audio' if l2_params['mode_type'] == 'audio' else 'Data'} "
          f"(target_nop={l2_params['target_nop']}, "
          f"psd_bytes={l2_params['psd_bytes']}, "
          f"lc_bits={l2_params['lc_bits']})")
    print()

    # Subchannel info
    print("-" * 78)
    print("  SUBCHANNEL CONFIGURATION")
    print("-" * 78)
    for i, sc in enumerate(subchannels):
        ch_type = "Stereo" if sc["channels"] == 2 else "Mono"
        br_str = f"{target_bitrates[i]:.1f} kbps" if target_bitrates[i] else "Auto"
        psd_str = ""
        if sc["name"] in psd_config:
            cfg = psd_config[sc["name"]]
            psd_str = f", PSD: {'Yes' if cfg['present'] else 'No'}"
            if cfg["bytes_frame_limit"] is not None:
                psd_str += f" ({cfg['bytes_frame_limit']} bytes/frame)"
        print(f"  {sc['name']:6s}  {ch_type:6s}  {br_str:12s}{psd_str}")
    print()

    # Data subchannel
    print("-" * 78)
    print("  DATA SUBCHANNEL ANALYSIS")
    print("-" * 78)
    print(f"  Data Bytes:             {data_bytes}")
    print(f"  CCC Width:              {ccc_width}")
    print(f"  Sync Channel:           1")
    print(f"  Total Data Width:       {tdw} bytes (out of {pb} payload)")
    print(f"  Data Width Percentage:  {tdw/pb*100:.1f}%")
    print()
    print(f"  Raw Data Throughput:    {data_throughput_bps:.0f} bps ({data_throughput_bps/1000:.2f} kbps)")
    print(f"  Usable Data Rate:       {data_throughput_bytes:.1f} bytes/sec")
    print()

    # BBM overhead detail
    if data_bytes > 0:
        bbm_overhead_pct = BBM_SIZE / BBM_PERIOD * 100
        print(f"  BBM Overhead:           {bbm_overhead_pct:.1f}% (4 bytes every {BBM_PERIOD} positions)")
        print(f"  HDLC Stuffing Est.:     {HDLC_STUFFING_RATIO*100:.0f}%")
        print()

    # Image transfer times
    if images:
        print("-" * 78)
        print("  IMAGE TRANSFER TIMES")
        print("-" * 78)
        total_data_per_cycle = 0
        for sc in subchannels:
            if sc["name"] in images:
                img = images[sc["name"]]
                print(f"  {sc['name']}:")
                if img["station_logo_kb"] > 0:
                    logo_bytes = int(img["station_logo_kb"] * 1024)
                    xfer_time = calculate_lot_transfer_time(
                        logo_bytes, data_throughput_bytes,
                        max(1, active_data_ports)
                    )
                    target_str = ""
                    if sc["name"] in transfer_speeds and transfer_speeds[sc["name"]].get("station_logo_sec") is not None:
                        target_sec = transfer_speeds[sc["name"]]["station_logo_sec"]
                        met = "MET" if xfer_time <= target_sec * 1.05 else "MISSED"
                        target_str = f"  [target: {format_time(target_sec)} - {met}]"
                    print(f"    Station Logo ({format_size(logo_bytes)}):  {format_time(xfer_time)}{target_str}")
                    total_data_per_cycle += logo_bytes
                if img["album_art_kb"] > 0:
                    art_bytes = int(img["album_art_kb"] * 1024)
                    xfer_time = calculate_lot_transfer_time(
                        art_bytes, data_throughput_bytes,
                        max(1, active_data_ports)
                    )
                    target_str = ""
                    if sc["name"] in transfer_speeds and transfer_speeds[sc["name"]].get("album_art_sec") is not None:
                        target_sec = transfer_speeds[sc["name"]]["album_art_sec"]
                        met = "MET" if xfer_time <= target_sec * 1.05 else "MISSED"
                        target_str = f"  [target: {format_time(target_sec)} - {met}]"
                    print(f"    Album Art    ({format_size(art_bytes)}):  {format_time(xfer_time)}{target_str}")
                    total_data_per_cycle += art_bytes

        if total_data_per_cycle > 0 and data_throughput_bytes > 0:
            # Total cycle time to send everything once
            total_xfer = calculate_lot_transfer_time(
                total_data_per_cycle, data_throughput_bytes, 1
            )
            print()
            print(f"  Total Data Per Cycle:   {format_size(total_data_per_cycle)}")
            print(f"  Est. Full Cycle Time:   {format_time(total_xfer)} (sequential)")
        print()

    # Bitrate analysis
    print("-" * 78)
    print("  BITRATE ANALYSIS")
    print("-" * 78)

    nop = l2_params["target_nop"]
    space_for_programs = pb - tdw

    print(f"  Payload Bytes:          {pb}")
    print(f"  Data Width:             {tdw}")
    print(f"  Space for Programs:     {space_for_programs}")
    print()

    total_audio_used = 0
    for i, sc in enumerate(subchannels):
        prog_psd = psd_bytes_list[i]
        overhead = calculate_per_program_overhead(l2_params, nop, prog_psd)
        bitrate_bps = target_bitrates[i] * 1000
        bytes_per_hdc = bitrate_bps * SAMPLES_PER_FRAME / HDC_SAMPLE_RATE / 8
        # Each audio frame = hdc_bytes + 1 (CRC8)
        audio_space = int(nop * (bytes_per_hdc + 1))
        total_space = audio_space + overhead
        total_audio_used += total_space

        print(f"  {sc['name']}:")
        print(f"    Bitrate:              {target_bitrates[i]:.1f} kbps")
        print(f"    HDC Bytes/Frame:      {bytes_per_hdc:.1f}")
        print(f"    Audio Space:          {audio_space} bytes ({nop} frames x {bytes_per_hdc + 1:.1f} bytes)")
        print(f"    Per-Program Overhead: {overhead} bytes "
              f"(RS={RS_PARITY_LEN} + CW={CONTROL_WORD_LEN} + "
              f"Loc={len_locators(l2_params['lc_bits'], nop)} + "
              f"HEF={HEF_LEN} + "
              f"PSD={psd_bytes_overrides.get(sc['name'], l2_params['psd_bytes'])})")
        print(f"    Total Space Used:     {total_space} bytes")
        print()

    remaining = space_for_programs - total_audio_used
    print(f"  Total Audio Used:       {total_audio_used} bytes")
    print(f"  Remaining Unused:       {remaining} bytes")
    print(f"  Total Audio Bitrate:    {sum(target_bitrates):.1f} kbps")
    print(f"  Max Audio Bitrate:      {max_total_bitrate:.1f} kbps (for {num_progs} programs)")
    print(f"  Max Recommended:        {MAX_BITRATE_BY_SIZE.get(frame_size_bits, 0)} kbps (from source code)")
    print()

    if remaining < 0:
        print("  *** WARNING: Audio bitrate EXCEEDS available space! ***")
        print(f"  *** Overflowing by {-remaining} bytes. Reduce bitrate or data_bytes. ***")
        print()
    elif sum(target_bitrates) > max_total_bitrate:
        overshoot = sum(target_bitrates) - max_total_bitrate
        print(f"  *** WARNING: Audio bitrate ({sum(target_bitrates):.1f} kbps) exceeds safe maximum ***")
        print(f"  *** ({max_total_bitrate:.1f} kbps) by {overshoot:.1f} kbps. ***")
        print(f"  *** The encoder may intermittently error with 'Audio bitrate is too high'. ***")
        print(f"  *** Reduce audio bitrate by at least {overshoot:.1f} kbps or reduce data_bytes. ***")
        print()

    # Check per-program bitrate against max recommended for this PDU size
    max_recommended = MAX_BITRATE_BY_SIZE.get(frame_size_bits, 0)
    if max_recommended > 0:
        for i, sc in enumerate(subchannels):
            if target_bitrates[i] > max_recommended:
                print(f"  *** WARNING: {sc['name']} bitrate ({target_bitrates[i]:.1f} kbps) exceeds the ***")
                print(f"  *** max recommended {max_recommended} kbps for {frame_size_bits}-bit PDU. ***")
                print(f"  *** The L2 encoder will reject this bitrate. ***")
                print(f"  *** Reduce {sc['name']} bitrate to at most {max_recommended} kbps. ***")
                print()

    # Recommendations
    print("-" * 78)
    print("  RECOMMENDATIONS")
    print("-" * 78)

    if explicit_bitrates and args.data_bytes is None:
        if data_bytes > 0:
            print(f"  Maximum data_bytes for your audio config:  {data_bytes}")
            print(f"  This gives you ~{data_throughput_bps/1000:.2f} kbps data throughput.")
        else:
            print("  No room for data bytes with your audio configuration!")
            print("  Consider reducing audio bitrate to make room.")
    elif not explicit_bitrates:
        print(f"  Calculated audio bitrates:")
        for i, sc in enumerate(subchannels):
            print(f"    {sc['name']}: {target_bitrates[i]:.1f} kbps")
        if data_bytes > 0:
            print(f"  Recommended data_bytes: {data_bytes}")
            print(f"  Data throughput: ~{data_throughput_bps/1000:.2f} kbps")

    print()

    # Quick reference for the user's GRC file
    print("-" * 78)
    print("  GRC PARAMETER SUMMARY")
    print("-" * 78)
    print(f"  L1 Encoder Mode:        {mode_name}")
    print(f"  L2 Encoder Size:        {frame_size_bits}")
    print(f"  L2 Encoder Data Bytes:  {data_bytes}")
    print(f"  L2 Encoder Num Progs:   {num_progs}")
    print(f"  L2 Encoder CCC Width:   {ccc_width}")
    for i, sc in enumerate(subchannels):
        ch_type = "Stereo" if sc["channels"] == 2 else "Mono"
        print(f"  HDC Encoder {sc['name']}:      {target_bitrates[i]:.0f} kbps, {ch_type}")
    print()
    print("=" * 78)
    print()


def main():
    parser = argparse.ArgumentParser(
        description="HD Radio Data Bytes & Bitrate Calculator",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=textwrap.dedent("""\
            Examples:
              # Calculate max data bytes for MP3 mode with explicit bitrates:
              python calculate_bytes.py --mode MP3 --port P1 \\
                --subchannels "HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps" \\
                --psd_data_present "HD1,true,128 - HD2,true,128 - HD3,true,128" \\
                --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb"

              # Auto-calculate bitrates with percentage allocation:
              python calculate_bytes.py --mode MP3 --port P1 \\
                --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \\
                --prioritize_subchannel_percentage 40,30,30 \\
                --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb"

              # Simple FM MP1 single-program calculation:
              python calculate_bytes.py --mode MP1 --port P1 \\
                --subchannels "HD1,Stereo,96kbps"

              # AM MA3 mode calculation:
              python calculate_bytes.py --mode MA3 --port P3 \\
                --subchannels "HD1,Stereo - HD2,Mono" \\
                --data_bytes 500
        """)
    )

    parser.add_argument("--mode", required=True,
                        help="HD Radio transmission mode (FM: MP1, MP2, MP3, MP5, MP6, MP11; AM: MA1, MA3)")
    parser.add_argument("--port", required=True,
                        help="L2 Encoder port on the L1 encoder (e.g., P1, P2, P3, P4)")
    parser.add_argument("--subchannels", required=True,
                        help="Subchannel configuration: 'HD1,Stereo,32kbps - HD2,Stereo,26kbps' "
                             "or 'HD1,Stereo - HD2,Stereo' (bitrate auto-calculated)")
    parser.add_argument("--data_bytes", type=int, default=None,
                        help="Fixed data subchannel size in bytes (auto-calculated if omitted)")
    parser.add_argument("--ccc_width", type=int, default=24,
                        help="Configuration Control Channel width in bytes (default: 24)")
    parser.add_argument("--images", default=None,
                        help="Album art/station logo sizes: "
                             "'HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb'")
    parser.add_argument("--psd_data_present", default=None,
                        help="PSD metadata config: 'HD1,true,128 - HD2,true,128 - HD3,true,128'")
    parser.add_argument("--psd_bytes_frame_limit", type=int, default=None,
                        help="Global PSD bytes/frame limit override for all subchannels")
    parser.add_argument("--prioritize_subchannel", default=None,
                        help="Prioritize a subchannel for bitrate (e.g., HD1)")
    parser.add_argument("--prioritize_subchannel_percentage", default=None,
                        help="Bitrate percentages per subchannel (e.g., '40,30,30')")
    parser.add_argument("--target_image_transfer_speed", default=None,
                        help="Target transfer time per image per subchannel: "
                             "'HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s'. "
                             "Accepts seconds (34s, 34sec, 34), minutes (1.5min, 1min30sec), "
                             "milliseconds (90ms), or colon format (0:34, 1:30, 00:01:30, 00:00:34:00)")

    args = parser.parse_args()
    run_calculation(args)


if __name__ == "__main__":
    main()
