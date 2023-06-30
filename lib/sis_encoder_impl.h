/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_SIS_ENCODER_IMPL_H
#define INCLUDED_NRSC5_SIS_ENCODER_IMPL_H

#include <nrsc5/sis_encoder.h>

namespace gr {
namespace nrsc5 {

class sis_encoder_impl : public sis_encoder
{
private:
    unsigned int alfn;
    std::string country_code;
    unsigned int fcc_facility_id;
    std::string short_name;
    std::string slogan;
    std::string message;
    float latitude;
    float longitude;
    float altitude;

    unsigned char* bit;

    unsigned int long_name_current_frame;
    unsigned int long_name_seq;

    unsigned int slogan_current_frame;

    unsigned int message_current_frame;
    unsigned int message_seq;

    std::vector<unsigned int> program_types;
    unsigned int current_program;

    // vars for service parameter message
    // counter
    unsigned int sis7idx;
    float UTCoffset;
    unsigned int DSTSchedule;
    bool DSTLocal;
    bool DSTReg;

    int crc12(unsigned char* sis);
    void write_bit(int b);
    void write_int(int n, int len);
    void write_char5(char c);
    void write_station_id();
    void write_station_name_short();
    void write_station_name_long();
    void write_station_location(bool high);
    void write_station_message();
    void write_service_information_message();
    void write_sis_parameter_message();
    void write_station_slogan();

public:
    sis_encoder_impl(const std::string& short_name = "ABCD");
    ~sis_encoder_impl();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_SIS_ENCODER_IMPL_H */
