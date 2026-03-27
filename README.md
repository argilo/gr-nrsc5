gr-nrsc5
========

This project implements an HD Radio transmitter in GNU Radio.
HD Radio is standardized in NRSC-5. The latest version of the
standard is NRSC-5-E, which can be found at
https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-e/nrsc-5-e.asp.

If you're interested in receiving HD Radio, a stand-alone receiver for RTL-SDR
is available here: https://github.com/theori-io/nrsc5/

## Installation

If you installed GNU Radio using PyBOMBS, simply run `pybombs install gr-nrsc5`.

Otherwise, run the following commands:

    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    sudo ldconfig

## Blocks:

### HDC encoder

This block encodes audio into High-Definition Coding (HDC) frames. The input sample rate must be 44,100 samples per second. ADTS headers are added to the output frames to facilitate synchronization. The encoding is performed by a patched version of fdk-aac: https://github.com/argilo/fdk-aac/tree/hdc-encoder

### PSD encoder

This block encodes Program Service Data PDUs, as described in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1028s.pdf. PSD conveys information (e.g. track title & artist) about the audio that is currently playing. The encoder supports all ID3 frames defined in NRSC-5 Table 5-1.

To control latency, connect the "clock" output of the Layer 1 encoder to the "clock" input of the PSD encoder, and set "Bytes/frame limit" to 128 (if the L2 frame size is 24000 or larger) or 64 (if the L2 frame size is smaller than 24000).

To dynamically update metadata, connect a Socket PDU (TCP Server) block to the "set_meta" input, and send any of the following commands via TCP, followed by a carriage return:

**Basic Metadata (PSD Types 1-4):**
* `titleExample Title` — set title to `Example Title` (TIT2)
* `artistExample Artist` — set artist to `Example Artist` (TPE1)
* `albumExample Album` — set album to `Example Album` (TALB)
* `genreRock` — set genre to `Rock` (TCON)

**Comment (PSD Type 5 - COMM):**
* `commentRecorded live at Madison Square Garden` — set comment text
* `comment_shortInfo` — set comment short description
* `comment_languageeng` — set comment language (3-byte ISO 639-2 code, default: eng)

**Commercial Frame (PSD Type 6 - COMR):**
* `commercial_price$9.99` — set price string
* `commercial_valid20261231` — set expiration date (YYYYMMDD format)
* `commercial_urlhttps://example.com/buy` — set contact URL for purchase
* `commercial_received0` — set received-as byte (how merchandise is received)
* `commercial_sellerExample Store` — set seller name
* `commercial_descBuy now and save 20%!` — set advertisement description

**Reference Identifier (PSD Type 7 - UFID):**
* `ufid_ownerhttp://musicbrainz.org` — set owner identifier URL
* `ufid_idabc123def456` — set unique identifier (up to 64 bytes)

**Album Art (XHDR):**
* `lot1337` — display album art contained in LOT file 1337
* `lot-1` — display station logo

**Notes:**
* All metadata fields are optional except Title and Artist (per NRSC-5 requirements)
* Maximum PSD message size is 1,018 bytes (excluding 6-byte overhead)
* Title, Artist, Album, and Genre frames should be limited to less than 128 characters
* Binary pictures are not supported in PSD; use LOT protocol for album art transmission

### SIS & SIG encoder

This block encodes Station Information Service PDUs, as described in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1020s.pdf, and assembles them into the PIDS and SIDS logical channels. SIS provides information about the station.

The block can also generate Station Information Guide (SIG) data on its "aas" output, providing the receiver with further information about audio and data services. For each audio program, it indicates that album art and station logo are present. To send SIG data, the "aas" output must be connected to the Layer 2 encoder's "aas" input, and the "ready" output of the Layer 2 encoder must be connected to the "ready" input of the SIS & SIG encoder to tell it when it should produce output.

The SIG data associates the following port numbers with the audio programs:

* Port 0x1000: Album art for HD1 (audio program 0)
* Port 0x1001: Station logo for HD1 (audio program 0)
* Port 0x1002: Album art for HD2 (audio program 1)
* Port 0x1003: Station logo for HD2 (audio program 1)
* Port 0x1004: Album art for HD3 (audio program 2)
* Port 0x1005: Station logo for HD3 (audio program 2)
* etc.

This block can also send emergency alerts. To enable this feature, connect a Socket PDU (TCP Server) block to the "command" input. To send an alert, connect to the TCP port and send the following command, followed by a carriage return:

```
set_alert|<control_data>|<alert text>
```

Control data is written in hexidecimal. For instance:

```
set_alert|00679c247c5b0438c70000|Example alert
```

To stop sending an alert, send the following command:

```
clear_alert
```

The `apps/send_alert.py` script can be used to generate control data and send a `set_alert` command to the TCP port. For example:

```
./send_alert.py 3 "Example alert" --same 51000
```

To indicate that the station provides emergency alerts, set the "Emergency alerts" parameter of the SIS & SIG encoder to "On".

The "clock" output of the Layer 1 encoder must be connected to the "clock" input of the SIS & SIG encoder. This connection is used to control latency.

### LOT encoder

This block sends files to the receiver (for instance, containing album art or a station logo) by encoding them as Advanced Application Services (AAS) PDUs, according to the Large Object Transfer (LOT) protocol. The "aas" output must be connected to the Layer 2 encoder's "aas" input, and the "ready" output of the Layer 2 encoder must be connected to the "ready" input of the LOT encoder to tell it when it should produce output.

#### File Expiry

Files transmitted via LOT include an expiry date/time. The expiry can be set in the GRC block's "Expiry" parameter (default: "45 minutes") or specified per-file via TCP commands. The expiry parameter supports two formats:

**Relative Time Format** (recommended):
* `15 milliseconds`, `15ms`, `15mil`
* `120 seconds`, `120s`, `120sec`
* `15 minutes`, `15m`, `15min`
* `2 hours`, `2h`, `2hr`
* `7 days`, `7d`
* `2 weeks`, `2w`, `2wk`
* `6 months`, `6mn`, `6month`
* `1 year`, `1y`, `1yr`

**ISO 8601 / TZ Format:**
* `2026-12-31T23:59:59+00:00` (with timezone offset)
* `2026-12-31T23:59:59Z` (UTC with 'Z' suffix)
* `2026-12-31 23:59:59+00:00` (space separator also supported)

#### TCP Commands

To allow new files to be sent at runtime, connect a Socket PDU (TCP Server) block to the "file" input. To read a new file from disk, send the following command, followed by a carriage return:

```
file|<lot_id>|<filename>|<expiry>
```

Example:
```
file|1337|album_art.png|15 minutes
```

To stream in a file over the network connection, send the following command, followed by a carriage return:

```
streamfile|<lot_id>|<size>|<filename>|<expiry>
```

Example:
```
streamfile|1337|4096|album_art.png|2 hours
```

Then send the file itself over the same network connection.

The `apps/send_album_art.py` script demonstrates how to stream an album art file and request for it to be displayed by the receiver.

**Notes:**
* The expiry parameter is optional in TCP commands; if omitted, the default expiry from the GRC block is used
* Station logo and album art files must use PNG or JPEG format, and be 200x200 pixels in size
* Expiry dates are encoded in the LOT header and transmitted to the receiver

### Layer 2 encoder

This block assembles HDC audio frames and PSD PDUs into the audio transport, producing layer 2 PDUs (as defined in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1014s.pdf and https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1017s.pdf).

The "Data bytes" setting controls how many bytes of each layer 2 PDU are set aside for Advanced Application Services (AAS) data.

The Layer 2 encoder gets program type information from the SIS & SIG encoder via the "aas" message port, so this port should be connected even when "Data bytes" is set to zero.

When sending data only (i.e. 0 audio programs), connect the "clock" output of the Layer 1 encoder to the "clock" input of the Layer 2 encoder to control latency.

### Layer 1 FM encoder

This block implements Layer 1 FM (as defined in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1011s.pdf). It takes PIDS and Layer 2 PDUs as input, and produces OFDM symbols as output. Only the Hybrid and Extended Hybrid modes have been implemented and tested so far. The All Digital modes are currently under development.

### Layer 1 AM encoder

This block implements Layer 1 AM (as defined in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1012s.pdf). It takes PIDS and Layer 2 PDUs as input, and produces OFDM symbols as output. Both Hybrid (MA1) mode and All Digital (MA3) mode are implemented.

## Flowgraphs:

Several sample flowgraphs are available in the apps folder:

### FM

* hd_tx_usrp.grc, hd_tx_usrp.py: tested on a USRP B200
* hd_tx_hackrf.grc, hd_tx_hackrf.py: tested on a HackRF One
* hd_tx_rtl_file.grc, hd_tx_rtl_file.py: produces an output file in the format used by https://github.com/theori-io/nrsc5/

### AM

* hd_tx_am_hackrf.grc, hd_tx_am_hackrf.py: Hybrid mode, tested on a HackRF One
* hd_tx_am_ma3_hackrf.grc, hd_tx_am_ma3_hackrf.py: All Digital mode, tested on a HackRF One
* hd_tx_am_soundcard.grc, hd_tx_am_soundcard.py: outputs I/Q samples as stereo audio, suitable for use with a soundcard-based direct conversion transmitter

These flowgraphs read a WAV file named sample.wav, which must be encoded at 44,100 samples per second. The license for the supplied sample.wav file is as follows:

> Copyright 2013, Canonical Ltd.
> This work is licensed under the Creative Commons Attribution-ShareAlike 3.0
> Unported License. To view a copy of this license, visit
> http://creativecommons.org/licenses/by-sa/3.0/ or send a letter to Creative
> Commons, 444 Castro Street, Suite 900, Mountain View, California, 94041, USA.
