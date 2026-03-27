# HD Radio Data Bytes & Bitrate Calculator

A Python command-line tool for calculating optimal data bytes, audio bitrate allocations, and data throughput for HD Radio transmissions using the gr-nrsc5 encoder.

## Why This Tool Exists

Configuring the `data_bytes` parameter in the L2 Encoder is tricky. Set it too high and audio will stutter or the encoder will error out. Set it too low and album art / station logos take forever to transfer. This tool does all the math for you based on the actual gr-nrsc5 source code calculations.

## Quick Start

```bash
# What's the max data_bytes I can use with my current audio config?
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps"

# Auto-calculate everything with images and PSD:
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \
  --prioritize_subchannel_percentage 40,30,30 \
  --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb" \
  --psd_data_present "HD1,true,128 - HD2,true,128 - HD3,true,128"

# Target specific image transfer times (album art in 30s, logos in 24s):
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \
  --prioritize_subchannel_percentage 40,30,30 \
  --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb" \
  --target_image_transfer_speed "HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s - HD3,Station Logo 24s,Album Art 30s"
```

## Requirements

- Python 3.6+
- No external dependencies required

## Command-Line Arguments

### Required Arguments

| Argument | Description |
|----------|-------------|
| `--mode` | HD Radio transmission mode. FM: `MP1`, `MP2`, `MP3`, `MP5`, `MP6`, `MP11`. AM: `MA1`, `MA3` |
| `--port` | L2 Encoder port on the L1 encoder (e.g., `P1`, `P2`, `P3`, `P4`). Depends on mode. |
| `--subchannels` | Subchannel configuration string (see format below) |

### Optional Arguments

| Argument | Default | Description |
|----------|---------|-------------|
| `--data_bytes` | Auto | Fixed data subchannel size in bytes. If omitted, calculated automatically. |
| `--ccc_width` | 24 | Configuration Control Channel width in bytes (1-30) |
| `--images` | None | Album art and station logo sizes per subchannel (see format below) |
| `--psd_data_present` | None | PSD metadata configuration per subchannel (see format below) |
| `--psd_bytes_frame_limit` | None | Global PSD bytes/frame limit override for all subchannels |
| `--prioritize_subchannel` | None | Give priority bandwidth to a specific subchannel (e.g., `HD1`) |
| `--prioritize_subchannel_percentage` | None | Explicit bandwidth percentages per subchannel (e.g., `40,30,30`) |
| `--target_image_transfer_speed` | None | Target transfer time per image per subchannel (see format below) |

## Argument Format Details

### --subchannels

Format: `"HDx,Mode[,Bitrate] - HDy,Mode[,Bitrate] - ..."`

- Subchannels are separated by ` - ` (space-dash-space)
- Each subchannel has: name, audio mode, and optional bitrate
- Audio mode: `Stereo` or `Mono`
- Bitrate: optional, in kbps (e.g., `32kbps`, `26k`, or just `26`)

**With explicit bitrates** (script calculates max data_bytes):
```
"HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps"
```

**Without bitrates** (script auto-calculates optimal bitrates):
```
"HD1,Stereo - HD2,Stereo - HD3,Stereo"
```

### --images

Format: `"HDx,Station Logo Xkb,Album Art Ykb - HDy,Station Logo Xkb,Album Art Ykb - ..."`

- Specifies album art and station logo file sizes for each subchannel
- Sizes in KB (e.g., `3kb`, `8kb`, `16kb`)
- Used to estimate data throughput requirements and transfer times

```
"HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb"
```

### --psd_data_present

Format: `"HDx,true/false[,bytes_per_frame] - HDy,true/false[,bytes_per_frame] - ..."`

- Configures PSD (Program Service Data) metadata per subchannel
- PSD includes: title, artist, album, genre, etc.
- `bytes_per_frame`: optional override (default: 128 for audio modes, 8 for data modes)

```
"HD1,true,128 - HD2,true,128 - HD3,true,128"
```

### --prioritize_subchannel_percentage

Format: `"pct1,pct2,pct3,..."`

- Comma-separated percentages, one per subchannel
- Must sum to 100
- Only used when bitrates are NOT explicitly specified in `--subchannels`

```
--prioritize_subchannel_percentage 40,30,30
```

### --target_image_transfer_speed

Format: `"HDx,Station Logo <time>,Album Art <time> - HDy,Station Logo <time>,Album Art <time> - ..."`

Sets a target transfer time for each image on each subchannel. The script will calculate the `data_bytes` value needed to achieve those transfer times. When combined with auto-calculated bitrates (no explicit bitrates in `--subchannels`), the audio bitrates will be reduced to make room for the required data throughput.

**Supported time formats:**

| Format | Examples | Notes |
|--------|----------|-------|
| Seconds | `34s`, `34`, `34sec`, `34seconds`, `34 seconds`, `34 sec`, `34 s`, `0.09s`, `0.09` | Bare numbers are treated as seconds |
| Minutes | `1.5min`, `1min30sec`, `1.5 minutes`, `1.5m`, `1.5 min` | Compound format like `1min30sec` supported |
| Milliseconds | `90ms`, `90 miliseconds`, `90 ms`, `90 mil`, `90 mili` | |
| Colon (m:ss) | `0:34`, `00:34`, `1:30`, `01:30` | Two parts = minutes:seconds |
| Colon (h:mm:ss) | `00:00:34`, `00:01:30`, `0:34:00`, `01:30:00`, `1:30:00` | Three parts = hours:minutes:seconds |
| Colon (h:mm:ss:ms) | `00:00:34:00`, `00:01:30:00` | Four parts = hours:minutes:seconds:milliseconds |

**Example:**
```
--target_image_transfer_speed "HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s - HD3,Station Logo 24s,Album Art 30s"
```

**Example with mixed time formats:**
```
--target_image_transfer_speed "HD1,Station Logo 0:24,Album Art 1.5min - HD2,Station Logo 24sec,Album Art 90s"
```

When used with `--images`, the script:
1. Calculates the data throughput needed for the slowest-to-meet target
2. Derives the minimum `data_bytes` to achieve that throughput
3. If bitrates are auto-calculated, reduces them to fit
4. Shows `[target: Xs - MET]` or `[target: Xs - MISSED]` next to each transfer time

If you specify explicit bitrates AND transfer speed targets, the script will show a WARNING if the audio bitrates + required data_bytes don't fit in the payload.

## Available Modes and Ports

### FM Modes

| Mode | Available Ports | Description |
|------|----------------|-------------|
| MP1 | P1 | Single program, no extended digital sidebands |
| MP2 | P1, P3 | Stereo main + small data channel |
| MP3 | P1, P3 | Stereo main + medium data channel |
| MP5 | P1, P2, P3 | Multi-program hybrid |
| MP6 | P1, P2 | Multi-program all-digital |
| MP11 | P1, P3, P4 | Multi-program stereo with dual data |

### AM Modes

| Mode | Available Ports | Description |
|------|----------------|-------------|
| MA1 | P1, P3 | Standard AM hybrid |
| MA3 | P1, P3 | Enhanced AM hybrid |

### Port Frame Sizes

| Port | Mode(s) | Frame Size (bits) | PDUs/Frame | L2 Type |
|------|---------|-------------------|-----------|---------|
| P1 | MP1, MP2, MP3, MP11 | 146,176 | 1 | Audio |
| P1 | MP5 | 4,608 | 8 | Data |
| P1 | MP6 | 9,216 | 8 | Data |
| P2 | MP5 | 109,312 | 1 | Audio |
| P2 | MP6 | 72,448 | 1 | Audio |
| P3 | MP2 | 2,304 | 8 | Data |
| P3 | MP3, MP5, MP11 | 4,608 | 8 | Data |
| P4 | MP11 | 4,608 | 8 | Data |
| P1 | MA1, MA3 | 3,750 | 1 | Audio |
| P3 | MA1 | 24,000 | 1 | Audio |
| P3 | MA3 | 30,000 | 1 | Audio |

### Maximum Recommended Bitrates (from source code)

| PDU Size | Max Bitrate | Used By |
|----------|------------|---------|
| 146,176 bits | 96 kbps | MP1/MP2/MP3/MP11 P1 |
| 109,312 bits | 72 kbps | MP5 P2 |
| 72,448 bits | 48 kbps | MP6 P2 |
| 30,000 bits | 32 kbps | MA3 P3 |
| 24,000 bits | 32 kbps | MA1 P3 |
| 9,216 bits | 24 kbps | MP6 P1 |
| 4,608 bits | 12 kbps | MP3/MP5/MP11 P3/P4 |
| 3,750 bits | 10 kbps | MA1/MA3 P1 |
| 2,304 bits | 6 kbps | MP2 P3 |

## How the Calculations Work

### PDU Payload Size

From `l2_encoder_impl.cc`:
```
payload_bytes = (size_bits - 22) / 8
```

### Total Data Width

When `data_bytes > 0`:
```
total_data_width = data_bytes + ccc_width + 1
                              ^            ^
                              |            └─ Sync channel (1 byte)
                              └─ Config Control Channel (default 24 bytes)
```

### Per-Program Overhead

Each audio program in a PDU has fixed overhead:
```
overhead = RS_PARITY (8) + CONTROL_WORD (6) + LOCATORS (variable) + HEF (3) + PSD (128 or 8)

LOCATORS size = ((lc_bits * target_nop) + 4) / 8
  Audio modes: lc_bits=16, target_nop=32 → 64 bytes
  Data modes:  lc_bits=12, target_nop=4  → 6 bytes
```

For audio modes: overhead = 8 + 6 + 64 + 3 + 128 = **209 bytes per program**
For data modes: overhead = 8 + 6 + 6 + 3 + 8 = **31 bytes per program**

### Audio Bytes per Program

Each HDC audio frame occupies `bytes_per_hdc_frame + 1` bytes in the PDU (the +1 is for a CRC8 byte):
```
bytes_per_hdc_frame = bitrate_bps * 2048 / 44100 / 8
audio_space = target_nop * (bytes_per_hdc_frame + 1)
```

### Space Available for Audio

```
space_for_programs = payload_bytes - total_data_width
safety_margin = 48 * num_progs   (headroom for HDC frame size variance)
effective_space = space_for_programs - safety_margin
audio_per_program = (effective_space / num_progs) - overhead_per_program
max_bitrate_per_prog = ((audio_per_program / target_nop) - 1) * 44100 * 8 / 2048
```

**Why the safety margin?** The L2 encoder packs programs sequentially into the PDU buffer. fdk-aac's CBR mode targets a specific bitrate but individual frames can be 1-2 bytes larger or smaller than expected. Over 32 frames per PDU, this variance accumulates. Without headroom, the last program in the PDU can run out of space and trigger `ERROR: Audio bitrate is too high`. The 48-byte-per-program margin prevents this.

### Data Throughput

The L1 frame duration is ~1.409 seconds (both FM and AM). PDU rate depends on how many PDUs the L1 encoder consumes per frame:
```
pdu_rate = pdus_per_l1_frame / 1.409

data_throughput = data_bytes * pdu_rate  (raw, before overhead)
```

Overhead deductions:
- **BBM (Block Boundary Marker)**: 4 bytes every 259 positions (~1.5%)
- **HDLC framing**: CRC16 (2 bytes) + flag (1 byte) + ~3% byte stuffing per packet

### Image Transfer Times

Album art and station logos are sent as LOT (Large Object Transfer) packets via the fixed data subchannel. Each LOT packet contains up to 256 bytes of data with headers:
- First packet: ~24 byte header + filename
- Subsequent packets: 8 byte header
- Each wrapped in AAS packet (5 byte header) + HDLC framing

Transfer time depends on data throughput and how many ports share the subchannel (round-robin).

## Examples

### Example 1: MP3 Mode - Calculate max data_bytes for known bitrates

You have 3 HD channels on P1 and want to know the maximum data_bytes:

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps" \
  --psd_data_present "HD1,true,128 - HD2,true,128 - HD3,true,128"
```

This will tell you the maximum `data_bytes` you can set in the L2 Encoder block without exceeding the payload capacity.

### Example 2: MP3 Mode - Auto-calculate everything with images

Let the script figure out optimal bitrates and data_bytes:

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \
  --prioritize_subchannel_percentage 40,30,30 \
  --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb" \
  --psd_data_present "HD1,true,128 - HD2,true,128 - HD3,true,128"
```

### Example 3: MP1 Mode - Simple single-program FM

```bash
python calculate_bytes.py --mode MP1 --port P1 \
  --subchannels "HD1,Stereo,96kbps"
```

### Example 4: MP5 Hybrid Mode - Two programs on P2

```bash
python calculate_bytes.py --mode MP5 --port P2 \
  --subchannels "HD1,Stereo,48kbps - HD2,Stereo,20kbps" \
  --data_bytes 200
```

### Example 5: MP3 Mode - P3 data port

```bash
python calculate_bytes.py --mode MP3 --port P3 \
  --subchannels "HD1,Stereo,10kbps" \
  --data_bytes 100
```

### Example 6: AM MA3 Mode

```bash
python calculate_bytes.py --mode MA3 --port P3 \
  --subchannels "HD1,Stereo - HD2,Mono" \
  --data_bytes 500
```

### Example 7: Prioritize HD1 bandwidth

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \
  --prioritize_subchannel HD1 \
  --images "HD1,Station Logo 3kb,Album Art 16kb - HD2,Station Logo 3kb,Album Art 8kb"
```

### Example 8: MP6 All-Digital Mode

```bash
python calculate_bytes.py --mode MP6 --port P2 \
  --subchannels "HD1,Stereo,32kbps - HD2,Stereo,16kbps" \
  --data_bytes 300 \
  --psd_data_present "HD1,true,128 - HD2,true,128"
```

### Example 9: MP11 Mode with P3 and P4

```bash
# Main audio on P1
python calculate_bytes.py --mode MP11 --port P1 \
  --subchannels "HD1,Stereo,64kbps - HD2,Stereo,24kbps" \
  --data_bytes 500

# Additional data on P3
python calculate_bytes.py --mode MP11 --port P3 \
  --subchannels "HD3,Stereo,10kbps" \
  --data_bytes 50
```

### Example 10: Target image transfer speed with auto bitrates

Want album art delivered in 30 seconds and station logos in 24 seconds? Let the script figure out the data_bytes and bitrates:

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo - HD3,Stereo" \
  --prioritize_subchannel_percentage 40,30,30 \
  --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb" \
  --psd_data_present "HD1,true,128 - HD2,true,128 - HD3,true,128" \
  --target_image_transfer_speed "HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s - HD3,Station Logo 24s,Album Art 30s"
```

The output will show whether each target was MET or MISSED. Audio bitrates will be automatically reduced to make room for the required data throughput.

### Example 11: Target transfer speed with explicit bitrates (check feasibility)

Use this to check if your desired bitrates are compatible with your transfer speed targets:

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo,32kbps - HD2,Stereo,26kbps - HD3,Stereo,26kbps" \
  --images "HD1,Station Logo 3kb,Album Art 8kb - HD2,Station Logo 3kb,Album Art 7kb - HD3,Station Logo 4kb,Album Art 8kb" \
  --target_image_transfer_speed "HD1,Station Logo 24s,Album Art 30s - HD2,Station Logo 24s,Album Art 30s - HD3,Station Logo 24s,Album Art 30s"
```

If the output shows a WARNING, you'll need to either reduce bitrates or relax the transfer speed targets.

### Example 12: Mixed time formats

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo - HD2,Stereo" \
  --images "HD1,Station Logo 4kb,Album Art 12kb - HD2,Station Logo 3kb,Album Art 8kb" \
  --target_image_transfer_speed "HD1,Station Logo 0:30,Album Art 1.5min - HD2,Station Logo 30sec,Album Art 1min30sec"
```

### Example 13: Check if your current config fits

If you're getting "Audio bitrate is too high" errors, check your config:

```bash
python calculate_bytes.py --mode MP3 --port P1 \
  --subchannels "HD1,Stereo,48kbps - HD2,Stereo,32kbps - HD3,Stereo,32kbps" \
  --data_bytes 2000
```

If the output shows a WARNING about exceeding available space, reduce either the bitrates or the `data_bytes` value.

## Understanding the Output

### Key Sections

1. **Transmission Mode Info** - Shows the mode, PDU size, frame rate, and L2 parameters
2. **Subchannel Configuration** - Lists your configured audio channels
3. **Data Subchannel Analysis** - Shows data_bytes allocation and throughput
4. **Image Transfer Times** - How long album art/logos take to send (if `--images` provided)
5. **Bitrate Analysis** - Detailed byte-level breakdown of how the PDU payload is used
6. **Recommendations** - Suggested values for your GRC file
7. **GRC Parameter Summary** - Copy-paste values for your GRC flowgraph

### Warning Signs

- **"WARNING: Audio bitrate EXCEEDS available space!"** - Your audio bitrates + data_bytes don't fit. Reduce one or both.
- **"Remaining Unused" is very small** (< 10 bytes) - You're running at the edge. Consider a small safety margin.
- **Image transfer times > 2 minutes** - Consider increasing data_bytes or reducing audio bitrate.
- **Max Audio Bitrate < Total Audio Bitrate** - Your config will cause encoder errors at runtime.

## Common Pitfalls

1. **data_bytes = 0 means no album art/station logos** - You need data_bytes > 0 to send images via LOT.

2. **CCC width matters** - The default of 24 bytes is standard, but it eats into your data budget. With `total_data_width = data_bytes + ccc_width + 1`, those 25 extra bytes per frame add up.

3. **PSD bytes are per-program** - With 3 programs at 128 bytes PSD each, that's 384 bytes of overhead per PDU just for metadata.

4. **The "Max Recommended" bitrate assumes no data_bytes** - If you're running data_bytes > 0, your actual max audio bitrate is lower.

5. **Data modes (P3/P4) have much less capacity** - A 4608-bit P3 PDU only has 573 payload bytes vs 18269 for a 146176-bit P1 PDU.

6. **BBM overhead is unavoidable** - The Block Boundary Marker consumes 4 bytes every 259 positions regardless of what data you're sending.

7. **Round-robin port sharing** - If you have multiple LOT ports (album art + station logo for each subchannel), they share the data subchannel bandwidth via round-robin. More ports = slower individual transfers.

## Technical Reference

All calculations in this tool are derived from the gr-nrsc5 source code:

- `lib/l1_fm_encoder_impl.cc` - FM L1 encoder, mode/port/frame size definitions
- `lib/l1_am_encoder_impl.cc` - AM L1 encoder, mode/port/frame size definitions
- `lib/l2_encoder_impl.cc` - L2 encoder, payload structure, data subchannel, audio packing
- `lib/l2_encoder_impl.h` - L2 constants (RS_PARITY_LEN, CONTROL_WORD_LEN, etc.)
- `lib/hdc_encoder_impl.cc` - HDC audio encoder, bytes_per_frame calculation
- `lib/hdc_encoder_impl.h` - HDC constants (HDC_SAMPLE_RATE, SAMPLES_PER_FRAME)
- `lib/psd_encoder_impl.cc` - PSD metadata encoder
- `python/lot_encoder.py` - LOT file transfer encoder
- `lib/hdlc.cc` - HDLC framing (CRC, byte stuffing)
