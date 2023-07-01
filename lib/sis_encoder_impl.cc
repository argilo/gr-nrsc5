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

namespace gr {
namespace nrsc5 {

sis_encoder::sptr sis_encoder::make(const std::string& short_name,
                                    const std::string& slogan,
                                    const std::string& message,
                                    float latitude,
                                    float longitude,
                                    float altitude,
                                    const std::string& country_code,
                                    const unsigned int fcc_facility_id)
{
    return gnuradio::get_initial_sptr(new sis_encoder_impl(short_name));
}


/*
 * The private constructor
 */
sis_encoder_impl::sis_encoder_impl(const std::string& short_name,
                                   const std::string& slogan,
                                   const std::string& message,
                                   const float latitude,
                                   const float longitude,
                                   const float altitude,
                                   const std::string& country_code,
                                   const unsigned int fcc_facility_id)
    : gr::sync_block("sis_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char) * SIS_BITS))
{
    set_output_multiple(BLOCKS_PER_FRAME);
    alfn = 800000000;
    this->country_code = country_code;
    this->fcc_facility_id = fcc_facility_id;
    this->short_name = short_name;
    this->slogan = slogan;
    this->message = message;
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

    slogan_current_frame = 0;

    message_current_frame = 0;
    message_seq = 0;

    program_types = { program_type::CLASSICAL, program_type::TALK };
    current_program = 0;

    current_parameter = 0;
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

    bit = out;
    while (bit < out + (noutput_items * SIS_BITS)) {
        for (int block = 0; block < BLOCKS_PER_FRAME; block++) {
            unsigned char* start = bit;

            write_bit(static_cast<int>(pdu_type::PIDS_FORMATTED));

            switch (block) {
            case 0:
            case 2:
            case 4:
            case 6:
            case 9:
            case 14:
                write_bit(static_cast<int>(extension::EXTENDED_FORMAT));
                write_station_name_short();
                write_station_id();
                break;
            case 1:
            case 5:
            case 10:
            case 15:
                write_bit(static_cast<int>(extension::EXTENDED_FORMAT));
                write_service_information_message();
                write_service_information_message();
                break;
            case 3:
                write_bit(static_cast<int>(extension::NO_EXTENSION));
                write_station_slogan();
                break;
            case 7:
                write_bit(static_cast<int>(extension::EXTENDED_FORMAT));
                write_station_location(true);
                write_station_location(false);
                break;
            case 8:
            case 13:
                write_bit(static_cast<int>(extension::NO_EXTENSION));
                write_station_message();
                break;
            case 11:
                write_bit(static_cast<int>(extension::NO_EXTENSION));
                write_station_name_long();
                break;
            case 12:
                write_bit(static_cast<int>(extension::EXTENDED_FORMAT));
                write_sis_parameter_message();
                write_station_id();
                break;
            }

            while (bit < start + 64) {
                write_bit(0);
            }
            write_bit(0); // reserved
            write_bit(static_cast<int>(time_status::NOT_LOCKED));
            write_int((alfn >> (block * 2)) & 0x3, 2);
            write_int(crc12(start), 12);
        }
        alfn++;
    }

    // Tell runtime system how many output items we produced.
    return noutput_items;
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
        write_char5(short_name[i]);
    }
    write_int(static_cast<int>(name_extension::FM), 2);
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

void sis_encoder_impl::write_station_location(bool high)
{
    int altitude_int = static_cast<int>(std::round(altitude / 16));
    altitude_int = std::max(std::min(altitude_int, 255), 0);

    write_int(static_cast<int>(msg_id::STATION_LOCATION), 4);
    write_bit(high);
    if (high) {
        write_int(std::round(latitude * 8192), 22);
        write_int(altitude_int >> 4, 4);
    } else {
        write_int(std::round(longitude * 8192), 22);
        write_int(altitude_int & 0xf, 4);
    }
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

    write_int(static_cast<int>(service_category::AUDIO), 2);
    write_bit(static_cast<int>(access::PUBLIC));
    write_int(current_program, 6);
    write_int(static_cast<int>(program_types[current_program]), 8);
    write_int(0, 5); // reserved
    write_int(static_cast<int>(sound_experience::NONE), 5);

    current_program = (current_program + 1) % program_types.size();
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

} /* namespace nrsc5 */
} /* namespace gr */
