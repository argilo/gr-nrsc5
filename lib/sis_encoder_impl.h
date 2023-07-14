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
constexpr int BLOCKS_PER_FRAME_FM = 16;
constexpr int BLOCKS_PER_FRAME_AM = 8;

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

enum class sched_item {
    STATION_ID,
    SHORT_STATION_NAME,
    LONG_STATION_NAME,
    STATION_LOCATION,
    STATION_MESSAGE,
    SERVICE_INFO_MESSAGE,
    SIS_PARAMETER_MESSAGE,
    UNIVERSAL_SHORT_STATION_NAME,
    STATION_SLOGAN,
    EA_MESSAGE
};

std::vector<std::vector<sched_item>> schedule_fm_short_no_ea = {
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::STATION_SLOGAN },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::STATION_LOCATION, sched_item::STATION_LOCATION },
    { sched_item::STATION_MESSAGE },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::LONG_STATION_NAME },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::STATION_MESSAGE },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE }
};

std::vector<std::vector<sched_item>> schedule_fm_long_no_ea = {
    { sched_item::UNIVERSAL_SHORT_STATION_NAME },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::STATION_SLOGAN },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::UNIVERSAL_SHORT_STATION_NAME },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::STATION_LOCATION, sched_item::STATION_LOCATION },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::STATION_MESSAGE },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::UNIVERSAL_SHORT_STATION_NAME },
    { sched_item::STATION_SLOGAN },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::STATION_MESSAGE },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID }
};

std::vector<std::vector<sched_item>> schedule_am_short_no_ea = {
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::STATION_MESSAGE },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SHORT_STATION_NAME },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_LOCATION },
    { sched_item::SHORT_STATION_NAME, sched_item::STATION_ID },
    { sched_item::STATION_SLOGAN },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SHORT_STATION_NAME },
    { sched_item::LONG_STATION_NAME }
};

std::vector<std::vector<sched_item>> schedule_am_long_no_ea = {
    { sched_item::UNIVERSAL_SHORT_STATION_NAME },
    { sched_item::STATION_MESSAGE },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::STATION_LOCATION },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE },
    { sched_item::STATION_SLOGAN },
    { sched_item::SIS_PARAMETER_MESSAGE, sched_item::STATION_ID },
    { sched_item::SERVICE_INFO_MESSAGE, sched_item::SERVICE_INFO_MESSAGE }
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

enum class sig_service_type { AUDIO = 0x40, DATA = 0x41 };

enum class sig_tag {
    DATA_INFO = 0x60,
    AUDIO_COMPONENT = 0x66,
    DATA_COMPONENT = 0x67,
    SERVICE_NAME = 0x69
};

enum class service_data_type {
    NON_SPECIFIC = 0,
    NEWS = 1,
    SPORTS = 3,
    WEATHER = 29,
    EMERGENCY = 31,
    TRAFFIC = 65,
    IMAGE_MAPS = 66,
    TEXT = 80,
    ADVERTISING = 256,
    FINANCIAL = 257,
    STOCK_TICKER = 258,
    NAVIGATION = 259,
    ELECTRONIC_PROGRAM_GUIDE = 260,
    AUDIO = 261,
    PRIVATE_DATA_NETWORK = 262,
    SERVICE_MAINTENANCE = 263,
    HD_RADIO_SYSTEM_SERVICES = 264,
    AUDIO_RELATED_DATA = 265
};

enum class data_type { STREAM = 0, PACKET = 1, LOT = 3 };

constexpr uint8_t AAS_PACKET_FORMAT = 0x21;
constexpr uint16_t SIG_PORT = 0x20;

class sis_encoder_impl : public sis_encoder
{
private:
    pids_mode mode;
    int blocks_per_frame;
    std::vector<std::vector<sched_item>>* schedule;
    unsigned int alfn;
    std::string country_code;
    unsigned int fcc_facility_id;
    std::string short_name;
    bool fm_suffix;
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

    unsigned int ussn_current_frame;
    unsigned int slogan_current_frame;

    unsigned int message_current_frame;
    unsigned int message_seq;

    std::vector<program_type> program_types;
    unsigned int current_program;

    unsigned int current_parameter;

    bool location_high;

    uint16_t d_seq;

    int crc12(unsigned char* sis);
    void write_bit(int b);
    void write_int(int n, int len);
    void write_char5(char c);
    void write_station_id();
    void write_station_name_short();
    void write_station_name_long();
    void write_station_location();
    void write_station_message();
    void write_service_information_message();
    void write_sis_parameter_message();
    void write_universal_short_station_name();
    void write_station_slogan();
    std::string generate_sig();
    std::string generate_sig_service(sig_service_type type,
                                     unsigned int number,
                                     const std::string name);
    std::string generate_sig_audio_component(unsigned int component_id,
                                             unsigned int program_id,
                                             program_type type,
                                             mime_hash mime);
    std::string generate_sig_data_component(unsigned int component_id,
                                            uint16_t port,
                                            service_data_type sdt,
                                            data_type type,
                                            mime_hash mime,
                                            unsigned int vendor_id);
    std::string generate_aas_header(uint16_t port, uint16_t seq);
    void handle_notify(pmt::pmt_t msg);
    void send_sig();

public:
    sis_encoder_impl(
        const pids_mode mode = pids_mode::FM,
        const std::string& short_name = "ABCD",
        const std::string& slogan = "",
        const std::string& message = "",
        const std::vector<program_type> program_types = { program_type::UNDEFINED },
        const float latitude = 40.6892,
        const float longitude = -74.0445,
        const float altitude = 93.0,
        const std::string& country_code = "US",
        const unsigned int fcc_facility_id = 0);
    ~sis_encoder_impl();

    // Where all the action really happens
    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
    bool start() override;
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_SIS_ENCODER_IMPL_H */
