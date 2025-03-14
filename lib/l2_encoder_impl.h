/* -*- c++ -*- */
/*
 * Copyright 2017, 2023 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_L2_ENCODER_IMPL_H
#define INCLUDED_NRSC5_L2_ENCODER_IMPL_H

#include <nrsc5/l2_encoder.h>
#include <mutex>
#include <queue>

namespace gr {
namespace nrsc5 {

constexpr unsigned char CW0_AUDIO[] = { 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1,
                                        1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1 };
constexpr unsigned char CW1_AUDIO_OPP[] = { 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1,
                                            0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0 };
constexpr unsigned char CW2_AUDIO_FIXED[] = { 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0,
                                              0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0 };
constexpr unsigned char CW3_AUDIO_FIXED_OPP[] = { 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0,
                                                  1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1 };
constexpr unsigned char CW4_FIXED[] = { 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1,
                                        0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0 };

constexpr unsigned char CRC8_TABLE[] = {
    0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97, 0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c,
    0x1f, 0x2e, 0x43, 0x72, 0x21, 0x10, 0x87, 0xb6, 0xe5, 0xd4, 0xfa, 0xcb, 0x98, 0xa9,
    0x3e, 0x0f, 0x5c, 0x6d, 0x86, 0xb7, 0xe4, 0xd5, 0x42, 0x73, 0x20, 0x11, 0x3f, 0x0e,
    0x5d, 0x6c, 0xfb, 0xca, 0x99, 0xa8, 0xc5, 0xf4, 0xa7, 0x96, 0x01, 0x30, 0x63, 0x52,
    0x7c, 0x4d, 0x1e, 0x2f, 0xb8, 0x89, 0xda, 0xeb, 0x3d, 0x0c, 0x5f, 0x6e, 0xf9, 0xc8,
    0x9b, 0xaa, 0x84, 0xb5, 0xe6, 0xd7, 0x40, 0x71, 0x22, 0x13, 0x7e, 0x4f, 0x1c, 0x2d,
    0xba, 0x8b, 0xd8, 0xe9, 0xc7, 0xf6, 0xa5, 0x94, 0x03, 0x32, 0x61, 0x50, 0xbb, 0x8a,
    0xd9, 0xe8, 0x7f, 0x4e, 0x1d, 0x2c, 0x02, 0x33, 0x60, 0x51, 0xc6, 0xf7, 0xa4, 0x95,
    0xf8, 0xc9, 0x9a, 0xab, 0x3c, 0x0d, 0x5e, 0x6f, 0x41, 0x70, 0x23, 0x12, 0x85, 0xb4,
    0xe7, 0xd6, 0x7a, 0x4b, 0x18, 0x29, 0xbe, 0x8f, 0xdc, 0xed, 0xc3, 0xf2, 0xa1, 0x90,
    0x07, 0x36, 0x65, 0x54, 0x39, 0x08, 0x5b, 0x6a, 0xfd, 0xcc, 0x9f, 0xae, 0x80, 0xb1,
    0xe2, 0xd3, 0x44, 0x75, 0x26, 0x17, 0xfc, 0xcd, 0x9e, 0xaf, 0x38, 0x09, 0x5a, 0x6b,
    0x45, 0x74, 0x27, 0x16, 0x81, 0xb0, 0xe3, 0xd2, 0xbf, 0x8e, 0xdd, 0xec, 0x7b, 0x4a,
    0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xc2, 0xf3, 0xa0, 0x91, 0x47, 0x76, 0x25, 0x14,
    0x83, 0xb2, 0xe1, 0xd0, 0xfe, 0xcf, 0x9c, 0xad, 0x3a, 0x0b, 0x58, 0x69, 0x04, 0x35,
    0x66, 0x57, 0xc0, 0xf1, 0xa2, 0x93, 0xbd, 0x8c, 0xdf, 0xee, 0x79, 0x48, 0x1b, 0x2a,
    0xc1, 0xf0, 0xa3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1a, 0x2b, 0xbc, 0x8d,
    0xde, 0xef, 0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24, 0x15, 0x3b, 0x0a, 0x59, 0x68,
    0xff, 0xce, 0x9d, 0xac
};

constexpr unsigned char BBM[] = { 0x7d, 0x3a, 0xe2, 0x42 };

constexpr int MAX_PROGRAMS = 8;
constexpr uint8_t AAS_PACKET_FORMAT = 0x21;
constexpr uint16_t SIG_PORT = 0x20;

class l2_encoder_impl : public l2_encoder
{
private:
    int num_progs;
    int first_prog;
    int program_type[MAX_PROGRAMS];
    int size;
    int data_bytes;
    blend blend_control;
    int payload_bytes;
    unsigned char rs_buf[255];
    void* rs_enc;
    int target_nop;
    int lc_bits;
    int psd_bytes;
    int pdu_seq_no;
    int pdu_seq_len;
    int codec_mode;
    int start_seq_no[MAX_PROGRAMS];
    int target_seq_no;
    int partial_bytes[MAX_PROGRAMS];
    int ccc_width;
    unsigned char ccc_count;
    std::vector<unsigned char> ccc;
    int ccc_offset;
    int total_data_width;
    std::map<int, std::queue<unsigned char>> aas_queues;
    int aas_current_port;
    int aas_block_offset;

    unsigned char* out_buf;

    void write_control_word(unsigned char* out,
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
                            int la_loc);
    void write_hef(unsigned char* out, int program_number, int access, int program_type);
    void write_locator(unsigned char* out, int i, int locator);
    void
    header_spread(const unsigned char* in, unsigned char* out, const unsigned char* pci);
    int adts_length(const unsigned char* header);
    int len_locators(int nop);
    void handle_aas_pdu(pmt::pmt_t msg);
    void decode_sig(std::vector<unsigned char>& pdu_bytes);

public:
    l2_encoder_impl(const int num_progs,
                    const int first_prog,
                    const int size,
                    const int data_bytes = 0,
                    const blend blend_control = blend::ENABLE);
    ~l2_encoder_impl();

    // Where all the action really happens
    void forecast(int noutput_items, gr_vector_int& ninput_items_required);

    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_L2_ENCODER_IMPL_H */
