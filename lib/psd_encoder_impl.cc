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
                                    const std::string& album,
                                    const std::string& genre,
                                    const std::string& comment_language,
                                    const std::string& comment_short_desc,
                                    const std::string& comment_text,
                                    const int bytes_per_frame)
{
    return gnuradio::get_initial_sptr(
        new psd_encoder_impl(prog_num, title, artist, album, genre,
                           comment_language, comment_short_desc, comment_text, bytes_per_frame));
}


/*
 * The private constructor
 */
psd_encoder_impl::psd_encoder_impl(const int prog_num,
                                   const std::string& title,
                                   const std::string& artist,
                                   const std::string& album,
                                   const std::string& genre,
                                   const std::string& comment_language,
                                   const std::string& comment_short_desc,
                                   const std::string& comment_text,
                                   const int bytes_per_frame)
    : gr::sync_block("psd_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char)))
{
    this->prog_num = prog_num;
    this->title = title;
    this->artist = artist;
    this->album = album;
    this->genre = genre;
    this->comment_language = comment_language;
    this->comment_short_desc = comment_short_desc;
    this->comment_text = comment_text;
    this->commercial_price = "";
    this->commercial_valid_until = "";
    this->commercial_contact_url = "";
    this->commercial_received_as = "";
    this->commercial_seller = "";
    this->commercial_description = "";
    this->ufid_owner = "";
    this->ufid_identifier = "";
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
                encode_psd_packet(AAS_PACKET_FORMAT, PORT[prog_num], seq_num++);
            std::vector<unsigned char> packet_vect(packet_str.begin(), packet_str.end());
            packet = hdlc_encode(packet_vect);

            packet_off = 0;
        }
        out[off] = packet[packet_off++];
    }

    if (bytes_per_frame > 0) {
        bytes_allowed -= noutput_items_reduced;
    }
    return noutput_items_reduced;
}

std::string psd_encoder_impl::encode_psd_packet(uint8_t dtpf, uint16_t port, uint16_t seq)
{
    std::stringstream out;

    out << dtpf;
    out << (char)(port & 0xff);
    out << (char)(port >> 8);
    out << (char)(seq & 0xff);
    out << (char)(seq >> 8);
    out << encode_id3();
    out << "UF";

    return out.str();
}

std::string psd_encoder_impl::encode_id3()
{
    std::stringstream out;

    std::string payload = "";

    // PSD Type 1: Title (TIT2) - Required
    if (!title.empty()) {
        payload += encode_text_frame("TIT2", title);
    }

    // PSD Type 2: Artist (TPE1) - Required
    if (!artist.empty()) {
        payload += encode_text_frame("TPE1", artist);
    }

    // PSD Type 3: Album (TALB)
    if (!album.empty()) {
        payload += encode_text_frame("TALB", album);
    }

    // PSD Type 4: Genre (TCON)
    if (!genre.empty()) {
        payload += encode_text_frame("TCON", genre);
    }

    // PSD Type 5: Comment (COMM)
    if (!comment_text.empty() || !comment_short_desc.empty()) {
        payload += encode_comment_frame(comment_language, comment_short_desc, comment_text);
    }

    // PSD Type 6: Commercial (COMR)
    if (!commercial_description.empty() || !commercial_contact_url.empty()) {
        payload += encode_commercial_frame(commercial_price, commercial_valid_until,
                                          commercial_contact_url, commercial_received_as,
                                          commercial_seller, commercial_description);
    }

    // PSD Type 7: Reference Identifier (UFID)
    if (!ufid_owner.empty() && !ufid_identifier.empty()) {
        payload += encode_ufid_frame(ufid_owner, ufid_identifier);
    }

    // XHDR frame for album art
    payload += encode_xhdr_frame();

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

std::string psd_encoder_impl::encode_comment_frame(const std::string& language,
                                                    const std::string& short_desc,
                                                    const std::string& text)
{
    std::stringstream out;

    // COMM frame structure:
    // Text encoding: 1 byte (0x00 for ISO-8859-1, 0x01 for Unicode)
    // Language: 3 bytes (ISO 639-2)
    // Short description: null-terminated string
    // Content: text string (not null-terminated)

    std::string lang = language.empty() ? "eng" : language;
    if (lang.length() > 3) {
        lang = lang.substr(0, 3);
    }
    while (lang.length() < 3) {
        lang += " ";
    }

    int len = 1 + 3 + short_desc.length() + 1 + text.length();

    out << "COMM";
    out << (char)((len >> 24) & 0xff);
    out << (char)((len >> 16) & 0xff);
    out << (char)((len >> 8) & 0xff);
    out << (char)(len & 0xff);
    out << (char)0;  // Flags byte 1
    out << (char)0;  // Flags byte 2
    out << (char)0;  // Text encoding (ISO-8859-1)
    out << lang;     // 3-byte language code
    out << short_desc;
    out << (char)0;  // Null terminator for short description
    out << text;

    return out.str();
}

std::string psd_encoder_impl::encode_commercial_frame(const std::string& price,
                                                       const std::string& valid_until,
                                                       const std::string& contact_url,
                                                       const std::string& received_as,
                                                       const std::string& seller,
                                                       const std::string& description)
{
    std::stringstream out;

    // COMR frame structure:
    // Text encoding: 1 byte (0x00 for ISO-8859-1)
    // Price: null-terminated string
    // Valid until: 8 bytes (YYYYMMDD format as string)
    // Contact URL: null-terminated string
    // Received as: 1 byte
    // Seller name: null-terminated string
    // Description: null-terminated string
    // Picture MIME type: null-terminated string (empty for PSD - binary pictures not supported)
    // Seller logo: null-terminated string (empty for PSD - binary pictures not supported)

    int len = 1 + price.length() + 1 + 8 + contact_url.length() + 1 + 1 +
              seller.length() + 1 + description.length() + 1 + 1 + 1;

    out << "COMR";
    out << (char)((len >> 24) & 0xff);
    out << (char)((len >> 16) & 0xff);
    out << (char)((len >> 8) & 0xff);
    out << (char)(len & 0xff);
    out << (char)0;  // Flags byte 1
    out << (char)0;  // Flags byte 2
    out << (char)0;  // Text encoding (ISO-8859-1)
    out << price;
    out << (char)0;  // Null terminator for price

    // Valid until date (8 bytes, pad with spaces if needed)
    std::string valid = valid_until;
    if (valid.length() > 8) {
        valid = valid.substr(0, 8);
    }
    while (valid.length() < 8) {
        valid += " ";
    }
    out << valid;

    out << contact_url;
    out << (char)0;  // Null terminator for contact URL

    // Received as (1 byte - typically 0x00)
    out << (char)(received_as.empty() ? 0 : received_as[0]);

    out << seller;
    out << (char)0;  // Null terminator for seller name
    out << description;
    out << (char)0;  // Null terminator for description
    out << (char)0;  // Empty picture MIME type (not supported)
    out << (char)0;  // Empty seller logo (not supported)

    return out.str();
}

std::string psd_encoder_impl::encode_ufid_frame(const std::string& owner,
                                                 const std::string& identifier)
{
    std::stringstream out;

    // UFID frame structure:
    // Owner identifier: null-terminated string (typically a URL)
    // Identifier: binary or text data (up to 64 bytes)

    int len = owner.length() + 1 + identifier.length();

    out << "UFID";
    out << (char)((len >> 24) & 0xff);
    out << (char)((len >> 16) & 0xff);
    out << (char)((len >> 8) & 0xff);
    out << (char)(len & 0xff);
    out << (char)0;  // Flags byte 1
    out << (char)0;  // Flags byte 2
    out << owner;
    out << (char)0;  // Null terminator for owner
    out << identifier;

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

            // PSD Type 1: Title (TIT2)
            if (line.rfind("title", 0) == 0) {
                title = line.substr(5);
            }
            // PSD Type 2: Artist (TPE1)
            else if (line.rfind("artist", 0) == 0) {
                artist = line.substr(6);
            }
            // PSD Type 3: Album (TALB)
            else if (line.rfind("album", 0) == 0) {
                album = line.substr(5);
            }
            // PSD Type 4: Genre (TCON)
            else if (line.rfind("genre", 0) == 0) {
                genre = line.substr(5);
            }
            // PSD Type 5: Comment (COMM) - Language
            else if (line.rfind("comment_language", 0) == 0) {
                comment_language = line.substr(16);
            }
            // PSD Type 5: Comment (COMM) - Short description
            else if (line.rfind("comment_short", 0) == 0) {
                comment_short_desc = line.substr(13);
            }
            // PSD Type 5: Comment (COMM) - Content/text
            else if (line.rfind("comment", 0) == 0) {
                comment_text = line.substr(7);
            }
            // PSD Type 6: Commercial (COMR) - Price
            else if (line.rfind("commercial_price", 0) == 0) {
                commercial_price = line.substr(16);
            }
            // PSD Type 6: Commercial (COMR) - Valid until
            else if (line.rfind("commercial_valid", 0) == 0) {
                commercial_valid_until = line.substr(16);
            }
            // PSD Type 6: Commercial (COMR) - Contact URL
            else if (line.rfind("commercial_url", 0) == 0) {
                commercial_contact_url = line.substr(14);
            }
            // PSD Type 6: Commercial (COMR) - Received as
            else if (line.rfind("commercial_received", 0) == 0) {
                commercial_received_as = line.substr(19);
            }
            // PSD Type 6: Commercial (COMR) - Seller
            else if (line.rfind("commercial_seller", 0) == 0) {
                commercial_seller = line.substr(17);
            }
            // PSD Type 6: Commercial (COMR) - Description
            else if (line.rfind("commercial_desc", 0) == 0) {
                commercial_description = line.substr(15);
            }
            // PSD Type 7: Reference Identifier (UFID) - Owner
            else if (line.rfind("ufid_owner", 0) == 0) {
                ufid_owner = line.substr(10);
            }
            // PSD Type 7: Reference Identifier (UFID) - Identifier
            else if (line.rfind("ufid_id", 0) == 0) {
                ufid_identifier = line.substr(7);
            }
            // Album art LOT
            else if (line.rfind("lot", 0) == 0) {
                try {
                    lot = std::stoi(line.substr(3));
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
