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

If your GNU Radio is installed in `/usr` (rather than `/usr/local`), then
replace the cmake line above with:

    cmake -DCMAKE_INSTALL_PREFIX=/usr ..

## Blocks:

### HDC encoder

This block encodes audio into High-Definition Coding (HDC) frames. The input sample rate must be 44,100 samples per second. ADTS headers are added to the output frames to facilitate synchronization. The encoding is performed by a patched version of fdk-aac: https://github.com/argilo/fdk-aac/tree/hdc-encoder

### PSD encoder

This block encodes Program Service Data PDUs, as described in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1028s.pdf. PSD conveys information (e.g. track title & artist) about the audio that is currently playing.

To control latency, connect the "clock" output of the Layer 1 encoder to the "clock" input of the PSD encoder, and set "Bytes/frame limit" to 128 (if the L2 frame size is 24000 or larger) or 64 (if the L2 frame size is smaller than 24000).

To dynamically update title, artist, and XHDR data, connect a Socket PDU (TCP Server) block to the "set_meta" input, and send any of the following commands via TCP, followed by a carriage return:

* `titleExample Title` — set title to `Example Title`
* `artistExample Artist` — set artist to `Example Artist`
* `lot1337` — display album art contained in LOT file 1337
* `lot-1` — display station logo

### SIS & SIG encoder

This block encodes Station Information Service PDUs, as described in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1020s.pdf, and assembles them into the PIDS and SIDS logical channels. SIS provides information about the station. All message types are implemented, except for Emergency Alerts.

The block can also generate Station Information Guide (SIG) data on its "aas" output, providing the receiver with further information about audio and data services. For each audio program, it indicates that album art and station logo are present. To send SIG data, the "aas" output must be connected to the Layer 2 encoder's "aas" input, and the "ready" output of the Layer 2 encoder must be connected to the "ready" input of the SIS & SIG encoder to tell it when it should produce output.

The SIG data associates the following port numbers with the audio programs:

* Port 0x1000: Album art for HD1 (audio program 0)
* Port 0x1001: Station logo for HD1 (audio program 0)
* Port 0x1002: Album art for HD2 (audio program 1)
* Port 0x1003: Station logo for HD2 (audio program 1)
* Port 0x1004: Album art for HD3 (audio program 2)
* Port 0x1005: Station logo for HD3 (audio program 2)
* etc.

### LOT encoder

This block sends files to the receiver (for instance, containing album art or a station logo) by encoding them as Advanced Application Services (AAS) PDUs, according to the Large Object Transfer (LOT) protocol. The "aas" output must be connected to the Layer 2 encoder's "aas" input, and the "ready" output of the Layer 2 encoder must be connected to the "ready" input of the LOT encoder to tell it when it should produce output.

To allow new files to be sent at runtime, connect a Socket PDU (TCP Server) block to the "file" input. To read a new file from disk, send the following command, followed by a carriage return:

```
file|<lot_id>|<filename>
```

To stream in a file over the network connection, send the following command, followed by a carriage return:

```
streamfile|<lot_id>|<size>|<filename>
```

Then send the file itself over the same network connection.

The `apps/send_album_art.py` script demonstrates how to stream an album art file and request for it to be displayed by the receiver.

Note: Station logo and album art files must use PNG or JPEG format, and be 200x200 pixels in size.

### Layer 2 encoder

This block assembles HDC audio frames and PSD PDUs into the audio transport, producing layer 2 PDUs (as defined in https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1014s.pdf and https://www.nrscstandards.org/standards-and-guidelines/documents/standards/nrsc-5-d/reference-docs/1017s.pdf).

The "Data bytes" setting controls how many bytes of each layer 2 PDU are set aside for Advanced Application Services (AAS) data.

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
