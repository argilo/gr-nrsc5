/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "hdlc.h"
#include "psd_encoder_impl.h"
#include <gnuradio/io_signature.h>
#include <sstream>

namespace gr {
namespace nrsc5 {

psd_encoder::sptr psd_encoder::make(const int prog_num,
                                    const std::string& title,
                                    const std::string& artist,
                                    const int bytes_per_frame)
{
    return gnuradio::get_initial_sptr(
        new psd_encoder_impl(prog_num, title, artist, bytes_per_frame));
}


/*
 * The private constructor
 */
psd_encoder_impl::psd_encoder_impl(const int prog_num,
                                   const std::string& title,
                                   const std::string& artist,
                                   const int bytes_per_frame)
    : gr::sync_block("psd_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char)))
{
    this->prog_num = prog_num;
    this->title = title;
    this->artist = artist;
    this->bytes_per_frame = bytes_per_frame;
    lot = -1;
    seq_num = 0;
    packet_off = 0;

    set_max_output_buffer(0, 4096);

    if (this->bytes_per_frame > 0) {
        bytes_allowed = 2 * this->bytes_per_frame;
    } else {
        bytes_allowed = INT_MAX;
    }

    message_port_register_in(pmt::intern("clock"));
    set_msg_handler(pmt::intern("clock"),
                    [this](pmt::pmt_t msg) { this->handle_clock(msg); });

    message_port_register_in(pmt::mp("set_meta"));
    set_msg_handler(pmt::mp("set_meta"),
                    [this](const pmt::pmt_t& msg) { this->set_meta(msg); });
}

/*
 * Our virtual destructor.
 */
psd_encoder_impl::~psd_encoder_impl() {}

int psd_encoder_impl::work(int noutput_items,
                           gr_vector_const_void_star& input_items,
                           gr_vector_void_star& output_items)
{
    unsigned char* out = (unsigned char*)output_items[0];

    int noutput_items_reduced = std::min(noutput_items, bytes_allowed);

    for (int off = 0; off < noutput_items_reduced; off++) {
        if (packet_off == packet.size()) {
            std::string packet_str =
                encode_psd_packet(BASIC_PACKET_FORMAT, PORT[prog_num], seq_num++);
            std::vector<unsigned char> packet_vect(packet_str.begin(), packet_str.end());
            packet = hdlc_encode(packet_vect);

            packet_off = 0;
        }
        out[off] = packet[packet_off++];
    }

    bytes_allowed -= noutput_items_reduced;
    return noutput_items_reduced;
}

std::string psd_encoder_impl::encode_psd_packet(int dtpf, int port, int seq)
{
    std::stringstream out;

    out << (char)(dtpf & 0xff);
    out << (char)(port & 0xff);
    out << (char)((port >> 8) & 0xff);
    out << (char)(seq & 0xff);
    out << (char)((seq >> 8) & 0xff);
    out << encode_id3();
    out << "UF";

    return out.str();
}

std::string psd_encoder_impl::encode_id3()
{
    std::stringstream out;

    std::string payload = encode_text_frame("TIT2", title) +
                          encode_text_frame("TPE1", artist) + encode_xhdr_frame();
    int len = payload.length();

    out << "ID3";
    out << (char)3;
    out << (char)0;
    out << (char)0;
    out << (char)((len >> 21) & 0x7f);
    out << (char)((len >> 14) & 0x7f);
    out << (char)((len >> 7) & 0x7f);
    out << (char)(len & 0x7f);
    out << payload;

    return out.str();
}

std::string psd_encoder_impl::encode_text_frame(const std::string& id,
                                                const std::string& data)
{
    std::stringstream out;

    int len = data.length() + 1;

    out << id;
    out << (char)((len >> 24) & 0xff);
    out << (char)((len >> 16) & 0xff);
    out << (char)((len >> 8) & 0xff);
    out << (char)(len & 0xff);
    out << (char)0;
    out << (char)0;
    out << (char)0;
    out << data;

    return out.str();
}

std::string psd_encoder_impl::encode_xhdr_frame()
{
    std::stringstream out;

    int param = (lot >= 0) ? 0 : 1;
    int extlen = (lot >= 0) ? 2 : 0;
    int len = 6 + extlen;
    uint32_t mime_int = static_cast<int>(mime_hash::PRIMARY_IMAGE);

    out << "XHDR";
    out << (char)((len >> 24) & 0xff);
    out << (char)((len >> 16) & 0xff);
    out << (char)((len >> 8) & 0xff);
    out << (char)(len & 0xff);
    out << (char)0;
    out << (char)0;
    out << (char)(mime_int & 0xff);
    out << (char)((mime_int >> 8) & 0xff);
    out << (char)((mime_int >> 16) & 0xff);
    out << (char)((mime_int >> 24) & 0xff);
    out << (char)param;
    out << (char)extlen;

    if (lot >= 0) {
        out << (char)(lot & 0xff);
        out << (char)((lot >> 8) & 0xff);
    }

    return out.str();
}

void psd_encoder_impl::handle_clock(pmt::pmt_t msg)
{
    if (bytes_per_frame > 0) {
        bytes_allowed += bytes_per_frame;
    }
}

void psd_encoder_impl::set_meta(const pmt::pmt_t& msg)
{
    int msg_len = pmt::blob_length(pmt::cdr(msg));
    char* msg_data = (char*)pmt::blob_data(pmt::cdr(msg));

    for (int i = 0; i < msg_len; i++) {
        if (msg_data[i] == '\n') {
            std::string line = meta_buffer.str();
            meta_buffer.str("");

            if (line.rfind("title", 0) == 0) {
                title = line.substr(5, msg_len - 5);
            } else if (line.rfind("artist", 0) == 0) {
                artist = line.substr(6, msg_len - 6);
            } else if (line.rfind("lot", 0) == 0) {
                try {
                    lot = std::stoi(line.substr(3, msg_len - 3));
                } catch (std::invalid_argument& err) {
                    // ignore
                }
            }
        } else {
            meta_buffer << msg_data[i];
        }
    }
}

} /* namespace nrsc5 */
} /* namespace gr */
