/* -*- c++ -*- */
/*
 * Copyright 2017, 2023 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hdlc.h"
#include "l2_encoder_impl.h"
#include <gnuradio/io_signature.h>

extern "C" {
#include <gnuradio/fec/rs.h>
}

#include <stdio.h>

namespace gr {
namespace nrsc5 {

l2_encoder::sptr l2_encoder::make(const int num_progs,
                                  const int first_prog,
                                  const int size,
                                  const int data_bytes,
                                  const blend blend_control)
{
    return gnuradio::get_initial_sptr(
        new l2_encoder_impl(num_progs, first_prog, size, data_bytes, blend_control));
}


/*
 * The private constructor
 */
l2_encoder_impl::l2_encoder_impl(const int num_progs,
                                 const int first_prog,
                                 const int size,
                                 const int data_bytes,
                                 const blend blend_control)
    : gr::block("l2_encoder",
                gr::io_signature::make(2, 16, sizeof(unsigned char)),
                gr::io_signature::make(1, 1, sizeof(unsigned char) * size))
{
    message_port_register_in(pmt::intern("aas"));
    set_msg_handler(pmt::intern("aas"),
                    [this](pmt::pmt_t msg) { this->handle_aas_pdu(msg); });

    message_port_register_out(pmt::intern("ready"));

    this->num_progs = num_progs;
    this->first_prog = first_prog;
    memset(program_type, 0, sizeof(program_type));
    this->size = size;
    this->data_bytes = data_bytes;
    this->blend_control = blend_control;
    payload_bytes = (size - 22) / 8;
    out_buf = (unsigned char*)malloc(payload_bytes);
    rs_enc = init_rs_char(8, 0x11d, 1, 1, 8);
    memset(rs_buf, 0, 255);
    pdu_seq_no = 0;
    memset(start_seq_no, 0, sizeof(start_seq_no));
    target_seq_no = 0;
    memset(partial_bytes, 0, sizeof(partial_bytes));
    ccc_width = 24;
    ccc_count = 0;
    ccc = hdlc_encode({ 0x00,
                        0x00,
                        0x00,
                        (unsigned char)(this->data_bytes & 0xff),
                        (unsigned char)(this->data_bytes >> 8) });
    ccc_offset = ccc.size() - 1;
    total_data_width = (this->data_bytes > 0) ? (this->data_bytes + ccc_width + 1) : 0;
    aas_current_port = -1;
    aas_block_offset = 0;

    switch (size) {
    case 146176:
    case 109312:
    case 72448:
    case 30000:
    case 24000:
        target_nop = 32;
        lc_bits = 16;
        psd_bytes = 128;
        pdu_seq_len = 2;
        codec_mode = 0;
        break;
    case 9216:
    case 4608:
    case 3750:
    case 2304:
        set_min_output_buffer(0, 16);
        target_nop = 4;
        lc_bits = 12;
        psd_bytes = 8;
        pdu_seq_len = 8;
        codec_mode = 13;
        break;
    }
}

/*
 * Our virtual destructor.
 */
l2_encoder_impl::~l2_encoder_impl()
{
    free(out_buf);
    free_rs_char(rs_enc);
}

void l2_encoder_impl::forecast(int noutput_items, gr_vector_int& ninput_items_required)
{
    for (int p = 0; p < num_progs; p++) {
        ninput_items_required[p] = noutput_items * size / 8;
        ninput_items_required[num_progs + p] = noutput_items * psd_bytes;
    }
}

int l2_encoder_impl::general_work(int noutput_items,
                                  gr_vector_int& ninput_items,
                                  gr_vector_const_void_star& input_items,
                                  gr_vector_void_star& output_items)
{
    const unsigned char** hdc = (const unsigned char**)&input_items[0];
    const unsigned char** psd = (const unsigned char**)&input_items[num_progs];
    unsigned char* out = (unsigned char*)output_items[0];

    int hdc_off[MAX_PROGRAMS] = { 0 };
    int psd_off[MAX_PROGRAMS] = { 0 };

    for (int out_off = 0; out_off < noutput_items * size; out_off += size) {
        memset(out_buf, 0, payload_bytes);

        unsigned char* out_program = out_buf;
        target_seq_no += target_nop;
        for (int p = 0; p < num_progs; p++) {
            int program_number = first_prog + p;
            int bytes_left = (out_buf + payload_bytes - total_data_width) - out_program;
            int nop = 0;
            int off = hdc_off[p];
            int audio_length = 0;
            int begin_bytes = 0;
            int end_bytes = 0;
            if (partial_bytes[p]) {
                nop++;
                audio_length = partial_bytes[p] + 1;
                off += partial_bytes[p];
            }
            while (nop < target_seq_no - start_seq_no[p]) {
                if (off + 7 > ninput_items[p])
                    break;
                int length = adts_length(hdc[p] + off);
                off += 7;

                if (off + length > ninput_items[p])
                    break;
                off += length;

                if (RS_PARITY_LEN + CONTROL_WORD_LEN + len_locators(nop + 1) + HEF_LEN +
                        psd_bytes + audio_length + 2 >
                    bytes_left)
                    break;
                if (RS_PARITY_LEN + CONTROL_WORD_LEN + len_locators(nop + 1) + HEF_LEN +
                        psd_bytes + audio_length + length + 1 >
                    bytes_left) {
                    begin_bytes = bytes_left - (RS_PARITY_LEN + CONTROL_WORD_LEN +
                                                len_locators(nop + 1) + HEF_LEN +
                                                psd_bytes + audio_length + 1);
                    end_bytes = length - begin_bytes;
                    nop++;
                    break;
                }

                nop++;
                audio_length += length + 1;
            }

            int la_loc = RS_PARITY_LEN + CONTROL_WORD_LEN + len_locators(nop) + HEF_LEN +
                         psd_bytes - 1;

            write_control_word(out_program + RS_PARITY_LEN,
                               codec_mode,
                               /*stream_id*/ 0,
                               pdu_seq_no,
                               program_number == 0 ? static_cast<int>(blend_control) : 0,
                               /*digital_gain_or_per_stream_delay*/ 0,
                               /*common_delay*/ program_number == 0 ? 24 : 0,
                               /*latency*/ 4,
                               partial_bytes[p] ? 1 : 0,
                               begin_bytes ? 1 : 0,
                               start_seq_no[p],
                               nop,
                               /*hef*/ 1,
                               la_loc);

            int end = la_loc;
            for (int i = 0; i < nop; i++) {
                int length;
                if ((i == 0) && partial_bytes[p]) {
                    length = partial_bytes[p];
                } else if ((i == nop - 1) && begin_bytes) {
                    length = begin_bytes;
                    hdc_off[p] += 7;
                    start_seq_no[p]++;
                } else {
                    length = adts_length(hdc[p] + hdc_off[p]);
                    hdc_off[p] += 7;
                    start_seq_no[p]++;
                }

                unsigned char crc_reg = 0xff;
                for (int j = 0; j < length; j++) {
                    crc_reg = CRC8_TABLE[crc_reg ^ hdc[p][hdc_off[p]]];
                    out_program[++end] = hdc[p][hdc_off[p]++];
                }
                out_program[++end] = crc_reg;
                write_locator(out_program + RS_PARITY_LEN + CONTROL_WORD_LEN, i, end);
            }
            partial_bytes[p] = end_bytes;

            write_hef(out_program + RS_PARITY_LEN + CONTROL_WORD_LEN + len_locators(nop),
                      program_number,
                      /*access*/ 0,
                      program_type[program_number]);

            memcpy(out_program +
                       (RS_PARITY_LEN + CONTROL_WORD_LEN + len_locators(nop) + HEF_LEN),
                   psd[p] + psd_off[p],
                   psd_bytes);
            psd_off[p] += psd_bytes;

            // Reed-Solomon encoding
            for (int i = RS_CODEWORD_LEN - 1; i >= RS_PARITY_LEN; i--) {
                rs_buf[255 - i - 1] = out_program[i];
            }
            encode_rs_char(rs_enc, rs_buf, rs_buf + 255 - RS_PARITY_LEN);
            for (int i = RS_PARITY_LEN - 1; i >= 0; i--) {
                out_program[i] = rs_buf[255 - i - 1];
            }

            out_program += (end + 1);

            if (target_seq_no - start_seq_no[p] > 8) {
                fprintf(stderr, "Audio bitrate it too high\n");
            }
        }

        if (data_bytes > 0) {
            // Synchronization channel
            if ((ccc_count & 0x03) == 0) {
                out_buf[payload_bytes - 1] = ccc_count;
            } else {
                if (ccc_width == 1) {
                    out_buf[payload_bytes - 1] = 0x00;
                } else {
                    out_buf[payload_bytes - 1] = ((ccc_width / 2) << 4) | (ccc_width / 2);
                }
            }
            ccc_count++;

            // Configuration control channel
            for (int i = payload_bytes - 1 - ccc_width; i < payload_bytes - 1; i++) {
                out_buf[i] = ccc[ccc_offset];
                ccc_offset = (ccc_offset + 1) % ccc.size();
            }

            // Fixed data subchannel
            std::vector<int> aas_ports;
            int aas_queue_bytes = 0;
            for (auto queue : aas_queues) {
                aas_ports.push_back(queue.first);
                aas_queue_bytes += queue.second.size();
            }

            if (aas_current_port == -1 && !aas_ports.empty()) {
                aas_current_port = aas_ports[0];
            }

            int aas_current_port_index = -1; // Index for round robin
            for (int i = 0; i < aas_ports.size(); i++) {
                if (aas_ports[i] == aas_current_port) {
                    aas_current_port_index = i;
                    break;
                }
            }

            for (int i = payload_bytes - 1 - ccc_width - data_bytes;
                 i < payload_bytes - 1 - ccc_width;
                 i++) {
                if (aas_block_offset < 4) {
                    out_buf[i] = BBM[aas_block_offset];
                } else {
                    if (aas_queue_bytes == 0) { // all queues are empty
                        out_buf[i] = 0x7e;
                    } else { // at least one queue still has data
                        // advance to the next non-empty queue if necessary
                        while (aas_queues[aas_current_port].empty()) {
                            aas_current_port_index =
                                (aas_current_port_index + 1) % aas_ports.size();
                            aas_current_port = aas_ports[aas_current_port_index];
                        }

                        // send out a byte
                        out_buf[i] = aas_queues[aas_current_port].front();
                        aas_queues[aas_current_port].pop();
                        aas_queue_bytes--;

                        // move to the next port at the end of a PDU
                        if (out_buf[i] == 0x7e) {
                            // if we emptied the queue, ask for more
                            if (aas_queues[aas_current_port].empty()) {
                                message_port_pub(pmt::intern("ready"),
                                                 pmt::from_long(aas_current_port));
                            }

                            aas_current_port_index =
                                (aas_current_port_index + 1) % aas_ports.size();
                            aas_current_port = aas_ports[aas_current_port_index];
                        }
                    }
                }
                aas_block_offset = (aas_block_offset + 1) % (255 + 4);
            }
        }

        header_spread(
            out_buf, out + out_off, (data_bytes > 0) ? CW2_AUDIO_FIXED : CW0_AUDIO);

        pdu_seq_no = (pdu_seq_no + 1) % pdu_seq_len;
    }

    for (int p = 0; p < num_progs; p++) {
        consume(p, hdc_off[p]);
        consume(num_progs + p, psd_off[p]);
    }
    return noutput_items;
}

/* 1017s.pdf figure 5-2 */
void l2_encoder_impl::write_control_word(unsigned char* out,
                                         int codec_mode,
                                         int stream_id,
                                         int pdu_seq_no,
                                         int blend_control,
                                         int digital_gain_or_per_stream_delay,
                                         int common_delay,
                                         int latency,
                                         int p_first,
                                         int p_last,
                                         int start_seq_no,
                                         int nop,
                                         int hef,
                                         int la_loc)
{
    out[0] = ((pdu_seq_no & 0x3) << 6) | (stream_id << 4) | codec_mode;
    out[1] = (digital_gain_or_per_stream_delay << 3) | (blend_control << 1) |
             (pdu_seq_no >> 2);
    out[2] = ((latency & 0x3) << 6) | common_delay;
    out[3] =
        ((start_seq_no & 0x1f) << 3) | (p_last << 2) | (p_first << 1) | (latency >> 2);
    out[4] = (hef << 7) | (nop << 1) | ((start_seq_no & 0x20) >> 5);
    out[5] = la_loc;
}

/* 1017s.pdf section 5.2.1.6 */
void l2_encoder_impl::write_hef(unsigned char* out,
                                int program_number,
                                int access,
                                int program_type)
{
    out[0] = 0x90 | (program_number << 1);
    out[1] = 0xA0 | (access << 3) | (program_type >> 7);
    out[2] = program_type & 0x7f;
}

void l2_encoder_impl::write_locator(unsigned char* out, int i, int locator)
{
    if (lc_bits == 16) {
        out[i * 2] = (locator & 0xff);
        out[i * 2 + 1] = (locator >> 8);
    } else {
        if (i % 2 == 0) {
            out[i / 2 * 3] = (locator & 0xff);
            out[i / 2 * 3 + 1] = (locator >> 8);
        } else {
            out[i / 2 * 3 + 1] |= ((locator & 0xf) << 4);
            out[i / 2 * 3 + 2] = (locator >> 4);
        }
    }
}

/* 1014s.pdf figure 5-2 */
void l2_encoder_impl::header_spread(const unsigned char* in,
                                    unsigned char* out,
                                    const unsigned char* pci)
{
    int n_start, n_offset, header_bits;

    /* 1014s.pdf table 5-4 */
    if (size >= 72000) {
        n_start = 8 * ((size - 30000 + 7) / 8);

        switch (size % 8) {
        case 0:
            n_offset = 1247;
            header_bits = 24;
            break;
        case 7:
            n_offset = 1303;
            header_bits = 23;
            break;
        default:
            n_offset = 1359;
            header_bits = 22;
        }
    } else {
        n_start = 120;

        switch (size % 8) {
        case 0:
            header_bits = 24;
            break;
        case 7:
            header_bits = 23;
            break;
        default:
            header_bits = 22;
        }

        n_offset = 8 * (((size - 120 + 7) / 8) / header_bits) - 1;
    }

    int out_off = 0;
    int pci_off = 0;
    for (int i = 0; i < payload_bytes; i++) {
        for (int j = 0; j < 8; j++) {
            if ((out_off >= n_start) && (pci_off < header_bits) &&
                ((out_off - n_start) % (n_offset + 1) == 0)) {
                out[out_off++] = pci[pci_off++];
            }
            out[out_off++] = (in[i] >> (7 - j)) & 1;
        }
    }
}

int l2_encoder_impl::adts_length(const unsigned char* header)
{
    return ((header[3] & 0x03) << 11 | (header[4] << 3) | (header[5] >> 5)) - 7;
}

int l2_encoder_impl::len_locators(int nop) { return ((lc_bits * nop) + 4) / 8; }

void l2_encoder_impl::handle_aas_pdu(pmt::pmt_t msg)
{
    std::vector<unsigned char> pdu_bytes = pmt::u8vector_elements(pmt::cdr(msg));
    int port = (pdu_bytes[2] << 8) | pdu_bytes[1];

    if ((pdu_bytes[0] == AAS_PACKET_FORMAT) && (port == SIG_PORT)) {
        decode_sig(pdu_bytes);
    }

    std::vector<unsigned char> pdu_encoded = hdlc_encode(pdu_bytes);

    for (auto c : pdu_encoded) {
        aas_queues[port].push(c);
    }
}

void l2_encoder_impl::decode_sig(std::vector<unsigned char>& pdu_bytes)
{
    int offset = 5;
    while (offset < pdu_bytes.size()) {
        unsigned char type = pdu_bytes[offset++];
        switch (type & 0xf0) {
        case 0x40:
            offset += 3;
            break;
        case 0x60:
            unsigned char length = pdu_bytes[offset++];
            if (type == 0x66) {
                unsigned char port = pdu_bytes[offset + 1];
                unsigned char type = pdu_bytes[offset + 2];
                if (port < MAX_PROGRAMS) {
                    program_type[port] = type;
                }
            }
            offset += length - 1;
        }
    }
}

} /* namespace nrsc5 */
} /* namespace gr */
