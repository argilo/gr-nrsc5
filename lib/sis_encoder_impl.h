/* -*- c++ -*- */
/*
 * Copyright 2017, 2023 Clayton Smith.
 * Copyright 2023 Vladislav Fomitchev.
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

enum class parameter_type {
    LEAP_SECOND_OFFSET,
    LEAP_SECOND_ALFN_LSB,
    LEAP_SECOND_ALFN_MSB,
    LOCAL_TIME_DATA,
    EXCITER_MANUFACTURER_ID,
    EXCITER_CORE_VERSION_NUMBER_1_2_3,
    EXCITER_MANUFACTURER_VERSION_NUMBER_1_2_3,
    EXCITER_VERSION_NUMBER_4_AND_STATUS,
    IMPORTER_MANUFACTURER_ID,
    IMPORTER_CORE_VERSION_NUMBER_1_2_3,
    IMPORTER_MANUFACTURER_VERSION_NUMBER_1_2_3,
    IMPORTER_VERSION_NUMBER_4_AND_STATUS,
    IMPORTER_CONFIGURATION_NUMBER
};

constexpr int NUM_PARAMETERS = 13;

enum class icb { IMPORTER_NOT_CONNECTED, IMPORTER_CONNECTED };

enum class dst_schedule { NONE, US_CANADA, EUROPE };

enum class priority { NORMAL, HIGH };

enum class encoding { ISO_8859_1 = 0, UCS_2 = 4 };

enum class service_category { AUDIO, DATA };

enum class access { PUBLIC, RESTRICTED };

enum class sound_experience {
    NONE = 0,
    DOLBY_PRO_LOGIC_II_SURROUND = 2,
    DTS_NEURAL_SURROUND = 3,
    FHG_MP3_SURROUND = 4,
    DTS_NEO_6_SURROUND = 5,
    DTS_NEURAL_X_SURROUND = 7,
    DOLBY_PRO_LOGIC_IIX_SURROUND = 8,
    DOLBY_PRO_LOGIC_IIZ_SURROUND = 9
};

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
    unsigned int pending_leap_second_offset;
    unsigned int current_leap_second_offset;
    unsigned int leap_second_alfn;
    int utc_offset;
    dst_schedule dst_sched;
    bool dst_local;
    bool dst_regional;
    std::string exciter_manufacturer_id;
    std::vector<unsigned int> exciter_core_version;
    unsigned int exciter_core_status;
    std::vector<unsigned int> exciter_manufacturer_version;
    unsigned int exciter_manufacturer_status;
    std::string importer_manufacturer_id;
    std::vector<unsigned int> importer_core_version;
    unsigned int importer_core_status;
    std::vector<unsigned int> importer_manufacturer_version;
    unsigned int importer_manufacturer_status;
    unsigned int importer_configuration_number;

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
    sis_encoder_impl(const std::string& short_name = "ABCD",
                     const std::string& slogan = "",
                     const std::string& message = "",
                     const float latitude = 40.6892,
                     const float longitude = -74.0445,
                     const float altitude = 93.0,
                     const std::string& country_code = "US",
                     const unsigned int fcc_facility_id = 0);
    ~sis_encoder_impl();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_SIS_ENCODER_IMPL_H */
