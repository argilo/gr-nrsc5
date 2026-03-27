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

constexpr uint16_t PORT[] = { 0x5100, 0x5201, 0x5202, 0x5203,
                              0x5204, 0x5205, 0x5206, 0x5207 };

constexpr uint8_t AAS_PACKET_FORMAT = 0x21;

class psd_encoder_impl : public psd_encoder
{
private:
    int prog_num;
    std::string title;
    std::string artist;
    std::string album;
    std::string genre;
    std::string comment_language;
    std::string comment_short_desc;
    std::string comment_text;
    std::string commercial_price;
    std::string commercial_valid_until;
    std::string commercial_contact_url;
    std::string commercial_received_as;
    std::string commercial_seller;
    std::string commercial_description;
    std::string ufid_owner;
    std::string ufid_identifier;
    int lot;
    int bytes_per_frame;
    uint16_t seq_num;
    std::vector<unsigned char> packet;
    int packet_off;
    int bytes_allowed;
    std::ostringstream meta_buffer;

    std::string encode_psd_packet(uint8_t dtpf, uint16_t port, uint16_t seq);
    std::string encode_id3();
    std::string encode_text_frame(const std::string& id, const std::string& data);
    std::string encode_comment_frame(const std::string& language, const std::string& short_desc, const std::string& text);
    std::string encode_commercial_frame(const std::string& price, const std::string& valid_until, const std::string& contact_url, const std::string& received_as, const std::string& seller, const std::string& description);
    std::string encode_ufid_frame(const std::string& owner, const std::string& identifier);
    std::string encode_xhdr_frame();

    void handle_clock(pmt::pmt_t msg);
    void set_meta(const pmt::pmt_t& msg);

public:
    psd_encoder_impl(const int prog_num,
                     const std::string& title,
                     const std::string& artist,
                     const std::string& album,
                     const std::string& genre,
                     const std::string& comment_language,
                     const std::string& comment_short_desc,
                     const std::string& comment_text,
                     const int bytes_per_frame = 0);
    ~psd_encoder_impl();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_PSD_ENCODER_IMPL_H */
