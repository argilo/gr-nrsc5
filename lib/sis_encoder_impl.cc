/* -*- c++ -*- */
/*
 * Copyright 2017, 2023 Clayton Smith.
 * Copyright 2023 Vladislav Fomitchev.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "sis_encoder_impl.h"
#include <gnuradio/io_signature.h>
#include <cmath>

using namespace std::string_literals;

namespace gr {
namespace nrsc5 {

sis_encoder::sptr sis_encoder::make(const pids_mode mode,
                                    const std::string& short_name,
                                    const std::string& slogan,
                                    const std::string& message,
                                    const std::vector<std::string> program_names,
                                    const std::vector<program_type> program_types,
                                    const std::vector<service_data_type> data_types,
                                    const std::vector<unsigned int> data_mime_types,
                                    float latitude,
                                    float longitude,
                                    float altitude,
                                    const std::string& country_code,
                                    const unsigned int fcc_facility_id)
{
    return gnuradio::get_initial_sptr(new sis_encoder_impl(mode,
                                                           short_name,
                                                           slogan,
                                                           message,
                                                           program_names,
                                                           program_types,
                                                           data_types,
                                                           data_mime_types,
                                                           latitude,
                                                           longitude,
                                                           altitude,
                                                           country_code,
                                                           fcc_facility_id));
}


/*
 * The private constructor
 */
sis_encoder_impl::sis_encoder_impl(const pids_mode mode,
                                   const std::string& short_name,
                                   const std::string& slogan,
                                   const std::string& message,
                                   const std::vector<std::string> program_names,
                                   const std::vector<program_type> program_types,
                                   const std::vector<service_data_type> data_types,
                                   const std::vector<unsigned int> data_mime_types,
                                   const float latitude,
                                   const float longitude,
                                   const float altitude,
                                   const std::string& country_code,
                                   const unsigned int fcc_facility_id)
    : gr::sync_block("sis_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char) * SIS_BITS))
{
    message_port_register_in(pmt::intern("clock"));
    set_msg_handler(pmt::intern("clock"),
                    [this](pmt::pmt_t msg) { this->handle_clock(msg); });

    message_port_register_in(pmt::intern("ready"));
    set_msg_handler(pmt::intern("ready"),
                    [this](pmt::pmt_t msg) { this->handle_notify(msg); });

    message_port_register_in(pmt::intern("command"));
    set_msg_handler(pmt::intern("command"),
                    [this](pmt::pmt_t msg) { this->handle_command(msg); });

    message_port_register_out(pmt::intern("aas"));

    if (country_code.length() != 2) {
        throw std::invalid_argument("country code must be two characters");
    }

    alfn = 800000000;
    this->country_code = country_code;
    this->fcc_facility_id = fcc_facility_id;

    if ((short_name.length() >= 3) &&
        (short_name.compare(short_name.length() - 3, 3, "-FM") == 0)) {
        this->short_name = short_name.substr(0, short_name.length() - 3);
        fm_suffix = true;
    } else {
        this->short_name = short_name;
        fm_suffix = false;
    }

    this->mode = mode;
    if (this->mode == pids_mode::FM) {
        blocks_per_frame = BLOCKS_PER_FRAME_FM;
    } else {
        blocks_per_frame = BLOCKS_PER_FRAME_AM;
    }
    set_output_multiple(blocks_per_frame);
    blocks_allowed = blocks_per_frame * 2;

    this->program_names = program_names;
    this->program_types = program_types;
    this->data_types = data_types;
    this->data_mime_types = data_mime_types;
    this->slogan = slogan;
    this->message = message;
    this->emergency_alert = "";
    this->emergency_alert_cnt_len = 0;
    this->latitude = latitude;
    this->longitude = longitude;
    this->altitude = altitude;
    pending_leap_second_offset = 18;
    current_leap_second_offset = 18;
    leap_second_alfn = 0;
    utc_offset = -360;
    dst_sched = dst_schedule::US_CANADA;
    dst_local = true;
    dst_regional = true;
    exciter_manufacturer_id = "CS";
    exciter_core_version = { 1, 0, 0, 0 };
    exciter_core_status = 0;
    exciter_manufacturer_version = { 1, 0, 0, 0 };
    exciter_manufacturer_status = 0;
    importer_manufacturer_id = "CS";
    importer_core_version = { 1, 0, 0, 0 };
    importer_core_status = 0;
    importer_manufacturer_version = { 1, 0, 0, 0 };
    importer_manufacturer_status = 0;
    importer_configuration_number = 0;

    long_name_current_frame = 0;
    long_name_seq = 0;

    ussn_current_frame = 0;
    slogan_current_frame = 0;

    message_current_frame = 0;
    message_seq = 0;

    emergency_alert_current_frame = 0;
    emergency_alert_seq = 0;

    current_service = 0;

    current_parameter = 0;

    location_high = true;

    d_seq = 0;
}

/*
 * Our virtual destructor.
 */
sis_encoder_impl::~sis_encoder_impl() {}

int sis_encoder_impl::work(int noutput_items,
                           gr_vector_const_void_star& input_items,
                           gr_vector_void_star& output_items)
{
    unsigned char* out = (unsigned char*)output_items[0];

    int noutput_items_reduced = std::min(noutput_items, blocks_allowed);

    std::vector<std::vector<sched_item>>* schedule;
    if (this->mode == pids_mode::FM) {
        if (this->short_name.length() <= 4) {
            if (this->emergency_alert.length() == 0) {
                schedule = &schedule_fm_short_no_ea;
            } else {
                schedule = &schedule_fm_short_ea;
            }
        } else {
            if (this->emergency_alert.length() == 0) {
                schedule = &schedule_fm_long_no_ea;
            } else {
                schedule = &schedule_fm_long_ea;
            }
        }
    } else {
        if (this->short_name.length() <= 4) {
            if (this->emergency_alert.length() == 0) {
                schedule = &schedule_am_short_no_ea;
            } else {
                schedule = &schedule_am_short_ea;
            }
        } else {
            if (this->emergency_alert.length() == 0) {
                schedule = &schedule_am_long_no_ea;
            } else {
                schedule = &schedule_am_long_ea;
            }
        }
    }

    bit = out;
    while (bit < out + (noutput_items_reduced * SIS_BITS)) {
        for (int block = 0; block < blocks_per_frame; block++) {
            unsigned char* start = bit;

            write_bit(static_cast<int>(pdu_type::PIDS_FORMATTED));

            std::vector<sched_item> payloads = (*schedule)[block];

            if (payloads.size() == 1) {
                write_bit(static_cast<int>(extension::NO_EXTENSION));
            } else {
                write_bit(static_cast<int>(extension::EXTENDED_FORMAT));
            }

            for (sched_item payload : payloads) {
                switch (payload) {
                case sched_item::STATION_ID:
                    write_station_id();
                    break;
                case sched_item::SHORT_STATION_NAME:
                    write_station_name_short();
                    break;
                case sched_item::LONG_STATION_NAME:
                    write_station_name_long();
                    break;
                case sched_item::STATION_LOCATION:
                    write_station_location();
                    break;
                case sched_item::STATION_MESSAGE:
                    write_station_message();
                    break;
                case sched_item::SERVICE_INFO_MESSAGE:
                    write_service_information_message();
                    break;
                case sched_item::SIS_PARAMETER_MESSAGE:
                    write_sis_parameter_message();
                    break;
                case sched_item::UNIVERSAL_SHORT_STATION_NAME:
                    write_universal_short_station_name();
                    break;
                case sched_item::STATION_SLOGAN:
                    write_station_slogan();
                    break;
                case sched_item::EA_MESSAGE:
                    write_emergency_alert();
                    break;
                }
            }

            while (bit < start + 64) {
                write_bit(0);
            }
            write_bit(0); // reserved
            write_bit(static_cast<int>(time_status::NOT_LOCKED));
            if (mode == pids_mode::FM) {
                write_int((alfn >> (block * 2)) & 0x3, 2);
            } else {
                if ((alfn & 0x3) == 0) {
                    // write most significant bits once every four blocks
                    write_int((alfn >> (16 + block * 2)) & 0x3, 2);
                } else {
                    // write least significant bits in the remaining three blocks
                    write_int((alfn >> (block * 2)) & 0x3, 2);
                }
            }
            write_int(crc12(start), 12);
        }
        alfn++;
    }

    blocks_allowed -= noutput_items_reduced;
    // Tell runtime system how many output items we produced.
    return noutput_items_reduced;
}

/* 1020s.pdf section 4.10
 * Note: The specified CRC is incorrect. It's actually a 16-bit CRC
 * truncated to 12 bits, and g(x) = X^16 + X^11 + X^3 + X + 1 */
int sis_encoder_impl::crc12(unsigned char* sis)
{
    unsigned short poly = 0xD010;
    unsigned short reg = 0x0000;
    int i, lowbit;

    for (i = 67; i >= 0; i--) {
        lowbit = reg & 1;
        reg >>= 1;
        reg ^= ((unsigned short)sis[i] << 15);
        if (lowbit)
            reg ^= poly;
    }
    for (i = 0; i < 16; i++) {
        lowbit = reg & 1;
        reg >>= 1;
        if (lowbit)
            reg ^= poly;
    }
    return reg ^ 0x955;
}

void sis_encoder_impl::update_control_data_crc(std::string& control_data)
{
    unsigned short poly = 0xD010;
    unsigned short reg = 0x7E1B;
    int lowbit;

    control_data[1] &= 0x00;
    control_data[2] &= 0xf0;

    int byte_index, bit_index;

    for (byte_index = control_data.length() - 1; byte_index >= 1; byte_index--) {
        for (bit_index = 0; bit_index < 8; bit_index++) {
            unsigned short bit =
                ((unsigned char)control_data.at(byte_index) >> bit_index) & 1;
            lowbit = reg & 1;
            reg >>= 1;
            reg ^= (bit << 15);
            if (lowbit)
                reg ^= poly;
        }
    }

    for (bit_index = 0; bit_index < 16; bit_index++) {
        lowbit = reg & 1;
        reg >>= 1;
        if (lowbit)
            reg ^= poly;
    }

    control_data[1] |= (reg & 0x00ff);
    control_data[2] |= (reg & 0x0f00) >> 8;
}

int sis_encoder_impl::crc7(const std::string alert)
{
    const unsigned char poly = 0x09;
    unsigned char reg = 0x42;
    int byte_index, bit_index;

    for (byte_index = alert.length() - 1; byte_index >= 0; byte_index--) {
        for (bit_index = 6; bit_index >= 0; bit_index--) {
            unsigned char bit = ((unsigned char)alert.at(byte_index) >> bit_index) & 1;
            if ((bit_index == 0) && (byte_index > 0))
                bit ^= ((unsigned char)alert.at(byte_index - 1) >> 7);

            reg <<= 1;
            reg ^= bit;
            if (reg & 0x80)
                reg ^= (0x80 | poly);
        }
    }

    for (bit_index = 6; bit_index >= 0; bit_index--) {
        reg <<= 1;
        if (reg & 0x80)
            reg ^= (0x80 | poly);
    }

    return reg;
}

void sis_encoder_impl::write_bit(int b) { *(bit++) = b; }

void sis_encoder_impl::write_int(int n, int len)
{
    if (n < 0)
        n += (1 << len);

    for (int i = 0; i < len; i++) {
        write_bit((n >> (len - i - 1)) & 1);
    }
}

void sis_encoder_impl::write_char5(char c)
{
    int n;
    if (c >= 'A' && c <= 'Z') {
        n = (c - 'A');
    } else if (c >= 'a' && c <= 'z') {
        n = (c - 'a');
    } else {
        switch (c) {
        case '?':
            n = 27;
            break;
        case '-':
            n = 28;
            break;
        case '*':
            n = 29;
            break;
        case '$':
            n = 30;
            break;
        default:
            n = 26;
        }
    }
    write_int(n, 5);
}

void sis_encoder_impl::write_station_id()
{
    write_int(static_cast<int>(msg_id::STATION_ID_NUMBER), 4);
    for (int i = 0; i < 2; i++) {
        write_char5(country_code[i]);
    }
    write_int(0, 3); // reserved
    write_int(fcc_facility_id, 19);
}

void sis_encoder_impl::write_station_name_short()
{
    write_int(static_cast<int>(msg_id::STATION_NAME_SHORT), 4);
    for (int i = 0; i < 4; i++) {
        if (i < short_name.length()) {
            write_char5(short_name[i]);
        } else {
            write_char5(' ');
        }
    }
    if (fm_suffix) {
        write_int(static_cast<int>(name_extension::FM), 2);
    } else {
        write_int(static_cast<int>(name_extension::NONE), 2);
    }
}

void sis_encoder_impl::write_station_name_long()
{
    write_int(static_cast<int>(msg_id::STATION_NAME_LONG), 4);

    unsigned int name_length = std::min((unsigned int)slogan.length(), 56u);
    unsigned int num_frames = std::max((name_length + 6) / 7, 1u);

    write_int(num_frames - 1, 3);
    write_int(long_name_current_frame, 3);
    for (int i = long_name_current_frame * 7; i < long_name_current_frame * 7 + 7; i++) {
        if (i < name_length) {
            write_int(slogan.at(i), 7);
        } else {
            write_int(0, 7);
        }
    }
    write_int(long_name_seq, 3);

    long_name_current_frame = (long_name_current_frame + 1) % num_frames;
}

void sis_encoder_impl::write_station_location()
{
    int altitude_int = static_cast<int>(std::round(altitude / 16));
    altitude_int = std::max(std::min(altitude_int, 255), 0);

    write_int(static_cast<int>(msg_id::STATION_LOCATION), 4);
    write_bit(location_high);
    if (location_high) {
        write_int(std::round(latitude * 8192), 22);
        write_int(altitude_int >> 4, 4);
    } else {
        write_int(std::round(longitude * 8192), 22);
        write_int(altitude_int & 0xf, 4);
    }

    location_high = !location_high;
}

void sis_encoder_impl::write_station_message()
{
    write_int(static_cast<int>(msg_id::STATION_MESSAGE), 4);

    unsigned int message_length = std::min((unsigned int)message.length(), 190u);
    unsigned int num_frames = (message_length + 7) / 6;

    write_int(message_current_frame, 5);
    write_int(message_seq, 2);

    if (message_current_frame == 0) {
        unsigned int checksum = 0;
        for (int j = 0; j < message_length; j++)
            checksum += (unsigned char)message.at(j);
        checksum = (((checksum >> 8) & 0x7f) + (checksum & 0xff)) & 0x7f;

        write_bit(static_cast<int>(priority::NORMAL));
        write_int(static_cast<int>(encoding::ISO_8859_1), 3);
        write_int(message_length, 8);
        write_int(checksum, 7);
        for (int i = 0; i < 4; i++) {
            if (i < message_length) {
                write_int(message.at(i), 8);
            } else {
                write_int(0, 8);
            }
        }
    } else {
        write_int(0, 3); // reserved
        for (int i = message_current_frame * 6 - 2; i < message_current_frame * 6 + 4;
             i++) {
            if (i < message_length) {
                write_int(message.at(i), 8);
            } else {
                write_int(0, 8);
            }
        }
    }

    message_current_frame = (message_current_frame + 1) % num_frames;
}

void sis_encoder_impl::write_service_information_message()
{
    write_int(static_cast<int>(msg_id::SERVICE_INFORMATION_MESSAGE), 4);

    if (current_service < program_types.size()) {
        write_int(static_cast<int>(service_category::AUDIO), 2);
        write_bit(static_cast<int>(access::PUBLIC));
        write_int(current_service, 6);
        write_int(static_cast<int>(program_types[current_service]), 8);
        write_int(0, 5); // reserved
        write_int(static_cast<int>(sound_experience::NONE), 5);
    } else {
        unsigned int data_index = current_service - program_types.size();

        write_int(static_cast<int>(service_category::DATA), 2);
        write_bit(static_cast<int>(access::PUBLIC));
        write_int(static_cast<int>(data_types[data_index]), 9);
        write_int(0, 3); // reserved
        write_int(data_mime_types[data_index], 12);
    }

    current_service = (current_service + 1) % (program_types.size() + data_types.size());
}

void sis_encoder_impl::write_sis_parameter_message()
{
    write_int(static_cast<int>(msg_id::SIS_PARAMETER_MESSAGE), 4);
    write_int(current_parameter, 6);

    switch (static_cast<parameter_type>(current_parameter)) {
    case parameter_type::LEAP_SECOND_OFFSET:
        write_int(pending_leap_second_offset, 8);
        write_int(current_leap_second_offset, 8);
        break;
    case parameter_type::LEAP_SECOND_ALFN_LSB:
        write_int(leap_second_alfn & 0xffff, 16);
        break;
    case parameter_type::LEAP_SECOND_ALFN_MSB:
        write_int(leap_second_alfn >> 16, 16);
        break;
    case parameter_type::LOCAL_TIME_DATA:
        write_int(utc_offset, 11);
        write_int(static_cast<int>(dst_sched), 3);
        write_bit(dst_local);
        write_bit(dst_regional);
        break;
    case parameter_type::EXCITER_MANUFACTURER_ID:
        write_bit(0); // reserved
        write_int(exciter_manufacturer_id[0], 7);
        write_bit(static_cast<int>(icb::IMPORTER_CONNECTED));
        write_int(exciter_manufacturer_id[1], 7);
        break;
    case parameter_type::EXCITER_CORE_VERSION_NUMBER_1_2_3:
        write_int(exciter_core_version[0], 5);
        write_int(exciter_core_version[1], 5);
        write_int(exciter_core_version[2], 5);
        write_bit(0); // reserved
        break;
    case parameter_type::EXCITER_MANUFACTURER_VERSION_NUMBER_1_2_3:
        write_int(exciter_manufacturer_version[0], 5);
        write_int(exciter_manufacturer_version[1], 5);
        write_int(exciter_manufacturer_version[2], 5);
        write_bit(0); // reserved
        break;
    case parameter_type::EXCITER_VERSION_NUMBER_4_AND_STATUS:
        write_int(exciter_core_version[3], 5);
        write_int(exciter_manufacturer_version[3], 5);
        write_int(exciter_core_status, 3);
        write_int(exciter_manufacturer_status, 3);
        break;
    case parameter_type::IMPORTER_MANUFACTURER_ID:
        write_bit(0); // reserved
        write_int(importer_manufacturer_id[0], 7);
        write_bit(0); // reserved
        write_int(importer_manufacturer_id[1], 7);
        break;
    case parameter_type::IMPORTER_CORE_VERSION_NUMBER_1_2_3:
        write_int(importer_core_version[0], 5);
        write_int(importer_core_version[1], 5);
        write_int(importer_core_version[2], 5);
        write_bit(0); // reserved
        break;
    case parameter_type::IMPORTER_MANUFACTURER_VERSION_NUMBER_1_2_3:
        write_int(importer_manufacturer_version[0], 5);
        write_int(importer_manufacturer_version[1], 5);
        write_int(importer_manufacturer_version[2], 5);
        write_bit(0); // reserved
        break;
    case parameter_type::IMPORTER_VERSION_NUMBER_4_AND_STATUS:
        write_int(importer_core_version[3], 5);
        write_int(importer_manufacturer_version[3], 5);
        write_int(importer_core_status, 3);
        write_int(importer_manufacturer_status, 3);
        break;
    case parameter_type::IMPORTER_CONFIGURATION_NUMBER:
        write_int(importer_configuration_number, 16);
    }
    current_parameter = (current_parameter + 1) % NUM_PARAMETERS;
}

void sis_encoder_impl::write_universal_short_station_name()
{
    write_int(static_cast<int>(msg_id::UNIVERSAL_SHORT_STATION_NAME), 4);

    unsigned int short_name_length = std::min((unsigned int)short_name.length(), 12u);
    unsigned int num_frames = std::max((short_name_length + 5) / 6, 1u);

    write_int(ussn_current_frame, 4);
    write_bit(static_cast<int>(name_type::UNIVERSAL_SHORT_STATION_NAME));

    if (ussn_current_frame == 0) {
        write_int(static_cast<int>(encoding::ISO_8859_1), 3);
        write_bit(fm_suffix);
        write_bit(num_frames - 1);
    } else {
        write_int(0, 5); // reserved
    }

    for (int i = ussn_current_frame * 6; i < ussn_current_frame * 6 + 6; i++) {
        if (i < short_name_length) {
            write_int(short_name.at(i), 8);
        } else {
            write_int(0, 8);
        }
    }

    ussn_current_frame = (ussn_current_frame + 1) % num_frames;
}

void sis_encoder_impl::write_station_slogan()
{
    write_int(static_cast<int>(msg_id::UNIVERSAL_SHORT_STATION_NAME), 4);

    unsigned int slogan_length = std::min((unsigned int)slogan.length(), 95u);
    unsigned int num_frames = (slogan_length + 6) / 6;

    write_int(slogan_current_frame, 4);
    write_bit(static_cast<int>(name_type::SLOGAN));

    if (slogan_current_frame == 0) {
        write_int(static_cast<int>(encoding::ISO_8859_1), 3);
        write_int(0, 3); // reserved
        write_int(slogan_length, 7);
        for (int i = 0; i < 5; i++) {
            if (i < slogan_length) {
                write_int(slogan.at(i), 8);
            } else {
                write_int(0, 8);
            }
        }
    } else {
        write_int(0, 5); // reserved
        for (int i = slogan_current_frame * 6 - 1; i < slogan_current_frame * 6 + 5;
             i++) {
            if (i < slogan_length) {
                write_int(slogan.at(i), 8);
            } else {
                write_int(0, 8);
            }
        }
    }

    slogan_current_frame = (slogan_current_frame + 1) % num_frames;
}

void sis_encoder_impl::write_emergency_alert()
{
    write_int(static_cast<int>(msg_id::EMERGENCY_ALERTS_MESSAGE), 4);

    unsigned int num_frames = (emergency_alert.length() + 8) / 6;

    write_int(emergency_alert_current_frame, 6);
    write_int(emergency_alert_seq, 2);
    write_int(0, 2); // reserved

    if (emergency_alert_current_frame == 0) {
        write_int(static_cast<int>(encoding::ISO_8859_1), 3);
        write_int(emergency_alert.length(), 9);
        write_int(crc7(emergency_alert), 7);
        write_int(emergency_alert_cnt_len, 5);
        for (int i = 0; i < 3; i++) {
            write_int(emergency_alert.at(i), 8);
        }
    } else {
        for (int i = emergency_alert_current_frame * 6 - 3;
             i < emergency_alert_current_frame * 6 + 3;
             i++) {
            if (i < emergency_alert.length()) {
                write_int(emergency_alert.at(i), 8);
            } else {
                write_int(0, 8);
            }
        }
    }

    emergency_alert_current_frame = (emergency_alert_current_frame + 1) % num_frames;
}

std::string sis_encoder_impl::generate_sig()
{
    std::stringstream out;
    unsigned int program_id = 0;
    unsigned int port = 0x1000;

    for (unsigned int program_id = 0; program_id < program_names.size(); program_id++) {
        unsigned int component_id = 0;

        out << generate_sig_service(
            sig_service_type::AUDIO, program_id + 1, program_names[program_id]);

        out << generate_sig_audio_component(
            component_id++, program_id, program_types[program_id], mime_hash::HDC);

        out << generate_sig_data_component(component_id++,
                                           port++,
                                           service_data_type::AUDIO_RELATED_DATA,
                                           data_type::LOT,
                                           mime_hash::PRIMARY_IMAGE,
                                           0x28 + program_id);

        out << generate_sig_data_component(component_id++,
                                           port++,
                                           service_data_type::AUDIO_RELATED_DATA,
                                           data_type::LOT,
                                           mime_hash::STATION_LOGO,
                                           0x32 + program_id);
    }

    return out.str();
}

std::string sis_encoder_impl::generate_sig_service(sig_service_type type,
                                                   unsigned int number,
                                                   const std::string name)
{
    std::stringstream out;

    out << (char)type;
    out << (char)number;
    out << (char)0; // unknown
    out << (char)2; // unknown

    out << (char)sig_tag::SERVICE_NAME;
    out << (char)(name.length() + 2);
    out << (char)0; // encoding?
    out << name;

    return out.str();
}

std::string sis_encoder_impl::generate_sig_audio_component(unsigned int component_id,
                                                           unsigned int program_id,
                                                           program_type type,
                                                           mime_hash mime)
{
    std::stringstream out;
    uint32_t mime_int = static_cast<uint32_t>(mime);

    out << (char)sig_tag::AUDIO_COMPONENT;
    out << (char)12; // length
    out << (char)component_id;
    out << (char)program_id;
    out << (char)type;
    out << (char)0; // unknown
    out << (char)0; // unknown
    out << (char)0; // unknown
    out << (char)0; // unknown
    out << (char)(mime_int & 0xff);
    out << (char)((mime_int >> 8) & 0xff);
    out << (char)((mime_int >> 16) & 0xff);
    out << (char)((mime_int >> 24) & 0xff);

    return out.str();
}

std::string sis_encoder_impl::generate_sig_data_component(unsigned int component_id,
                                                          uint16_t port,
                                                          service_data_type sdt,
                                                          data_type type,
                                                          mime_hash mime,
                                                          unsigned int vendor_id)
{
    std::stringstream out;
    uint32_t mime_int = static_cast<uint32_t>(mime);
    uint16_t sdt_int = static_cast<uint16_t>(sdt);

    out << (char)sig_tag::DATA_COMPONENT;
    out << (char)13; // length
    out << (char)component_id;
    out << (char)(port & 0xff);
    out << (char)((port >> 8) & 0xff);
    out << (char)(sdt_int & 0xff);
    out << (char)((sdt_int >> 8) & 0xff);
    out << (char)type;
    out << (char)0; // unknown
    out << (char)0; // unknown
    out << (char)(mime_int & 0xff);
    out << (char)((mime_int >> 8) & 0xff);
    out << (char)((mime_int >> 16) & 0xff);
    out << (char)((mime_int >> 24) & 0xff);

    out << (char)sig_tag::DATA_INFO;
    out << (char)9; // length
    out << "SELF";  // vendor?
    out << (char)vendor_id;
    out << (char)0; // unknown
    out << (char)0; // unknown
    out << (char)0; // unknown

    return out.str();
}

std::string sis_encoder_impl::generate_aas_header(uint16_t port, uint16_t seq)
{
    std::stringstream out;

    out << (char)AAS_PACKET_FORMAT;
    out << (char)(port & 0xff);
    out << (char)((port >> 8) & 0xff);
    out << (char)(seq & 0xff);
    out << (char)((seq >> 8) & 0xff);

    return out.str();
}

bool sis_encoder_impl::start()
{
    send_sig();
    return block::start();
}

void sis_encoder_impl::handle_clock(pmt::pmt_t msg)
{
    blocks_allowed += blocks_per_frame;
}

void sis_encoder_impl::handle_notify(pmt::pmt_t msg)
{
    long port = pmt::to_long(msg);
    if (port == SIG_PORT) {
        send_sig();
    }
}

void sis_encoder_impl::handle_command(pmt::pmt_t msg)
{
    for (auto byte : pmt::u8vector_elements(pmt::cdr(msg))) {
        if (byte == '\n') {
            auto command_line = command_buffer.str();

            auto command = command_line.substr(0, command_line.find('|'));

            if (command == "clear_alert") {
                this->emergency_alert = "";
                this->emergency_alert_cnt_len = 0;
                d_logger->info("clearing emergency alert");
            } else if (command == "set_alert") {
                if (command_line.size() >= 10) {
                    auto args = command_line.substr(10, -1);
                    auto control_bytes = args.substr(0, args.find('|'));
                    if (args.size() >= control_bytes.size() + 1) {
                        auto message = args.substr(control_bytes.size() + 1, -1);

                        std::ostringstream cnt;
                        for (int i = 0; i < control_bytes.size(); i += 2) {
                            unsigned char control_byte = (unsigned char)strtol(
                                control_bytes.substr(i, 2).c_str(), NULL, 16);
                            cnt.put(control_byte);
                        }
                        auto cnt_str = cnt.str();

                        bool error = false;
                        if ((cnt_str.length() < 7) || (cnt_str.length() > 63)) {
                            d_logger->error(
                                "emergency alert control data must be 7-63 bytes");
                            error = true;
                        }
                        if (cnt_str.length() % 2 != 1) {
                            d_logger->error(
                                "number of emergency alert control bytes must be odd");
                            error = true;
                        }
                        if (cnt_str.length() + message.length() > 381) {
                            d_logger->error(
                                "emergency alert payload cannot exceed 381 bytes");
                            error = true;
                        }

                        if (!error) {
                            update_control_data_crc(cnt_str);

                            emergency_alert = cnt_str + message;
                            emergency_alert_cnt_len = (cnt_str.length() - 1) / 2;
                            emergency_alert_current_frame = 0;
                            emergency_alert_seq = (emergency_alert_seq + 1) % 4;

                            d_logger->info("sending emergency alert");
                        }
                    } else {
                        d_logger->error("missing emergency alert message");
                    }
                } else {
                    d_logger->error("missing command data");
                }
            } else {
                d_logger->error("invalid command");
            }

            command_buffer.str("");
        } else {
            command_buffer.put(byte);
        }
    }
}

void sis_encoder_impl::send_sig()
{
    std::string sig_str = generate_aas_header(SIG_PORT, d_seq++) + generate_sig();
    pmt::pmt_t msg =
        pmt::cons(pmt::make_dict(),
                  pmt::init_u8vector(sig_str.length(), (const uint8_t*)sig_str.c_str()));

    message_port_pub(pmt::intern("aas"), msg);
}

} /* namespace nrsc5 */
} /* namespace gr */
