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

constexpr size_t SIS_BITS = 80;
constexpr int BLOCKS_PER_FRAME = 16;

enum class pdu_type { PIDS_FORMATTED, LOW_LATENCY_DATA_SERVICE };

enum class extension { NO_EXTENSION, EXTENDED_FORMAT };

enum class msg_id {
    STATION_ID_NUMBER,
    STATION_NAME_SHORT,
    STATION_NAME_LONG,
    ALFN,
    STATION_LOCATION,
    STATION_MESSAGE,
    SERVICE_INFORMATION_MESSAGE,
    SIS_PARAMETER_MESSAGE,
    UNIVERSAL_SHORT_STATION_NAME,
    ACTIVE_RADIO_MESSAGE,
    ADVANCED_SERVICE_INFORMATION_MESSAGE
};

enum class name_type { UNIVERSAL_SHORT_STATION_NAME, SLOGAN };

enum class time_status { NOT_LOCKED, LOCKED };

enum class name_extension { NONE, FM };

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
    int utc_offset;
    unsigned int dst_schedule;
    bool dst_local;
    bool dst_regional;

    unsigned char* bit;

    unsigned int long_name_current_frame;
    unsigned int long_name_seq;

    unsigned int slogan_current_frame;

    unsigned int message_current_frame;
    unsigned int message_seq;

    std::vector<program_type> program_types;
    unsigned int current_program;

    unsigned int current_parameter;

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
