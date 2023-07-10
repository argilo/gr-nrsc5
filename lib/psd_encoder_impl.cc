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

psd_encoder::sptr
psd_encoder::make(const int prog_num, const std::string& title, const std::string& artist)
{
    return gnuradio::get_initial_sptr(new psd_encoder_impl(prog_num, title, artist));
}


/*
 * The private constructor
 */
psd_encoder_impl::psd_encoder_impl(const int prog_num,
                                   const std::string& title,
                                   const std::string& artist)
    : gr::sync_block("psd_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char)))
{
    this->prog_num = prog_num;
    this->title = title;
    this->artist = artist;
    seq_num = 0;
    packet_off = 0;
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

    for (int off = 0; off < noutput_items; off++) {
        if (packet_off == packet.size()) {
            std::string packet_str =
                encode_psd_packet(BASIC_PACKET_FORMAT, PORT[prog_num], seq_num++);
            std::vector<unsigned char> packet_vect(packet_str.begin(), packet_str.end());
            packet = hdlc_encode(packet_vect);

            packet_off = 0;
        }
        out[off] = packet[packet_off++];
    }

    return noutput_items;
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
                          encode_text_frame("TPE1", artist) +
                          encode_xhdr_frame(mime_hash::PRIMARY_IMAGE, -1);
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

std::string psd_encoder_impl::encode_xhdr_frame(mime_hash mime, int lot)
{
    std::stringstream out;

    int param = (lot >= 0) ? 0 : 1;
    int extlen = (lot >= 0) ? 2 : 0;
    int len = 6 + extlen;
    uint32_t mime_int = static_cast<int>(mime);

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

} /* namespace nrsc5 */
} /* namespace gr */
