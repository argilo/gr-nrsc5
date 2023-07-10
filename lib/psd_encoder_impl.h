/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_PSD_ENCODER_IMPL_H
#define INCLUDED_NRSC5_PSD_ENCODER_IMPL_H

#include <nrsc5/psd_encoder.h>

namespace gr {
namespace nrsc5 {
unsigned short PORT[] = {
    0x5100, 0x5201, 0x5202, 0x5203, 0x5204, 0x5205, 0x5206, 0x5207
};

class psd_encoder_impl : public psd_encoder
{
private:
    int prog_num;
    std::string title;
    std::string artist;
    int seq_num;
    std::vector<unsigned char> packet;
    int packet_off;

    std::string encode_psd_packet(int dtpf, int port, int seq);
    std::string encode_id3();
    std::string encode_text_frame(const std::string& id, const std::string& data);
    std::string encode_xhdr_frame(mime_hash mime, int lot);

public:
    psd_encoder_impl(const int prog_num,
                     const std::string& title,
                     const std::string& artist);
    ~psd_encoder_impl();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_PSD_ENCODER_IMPL_H */
