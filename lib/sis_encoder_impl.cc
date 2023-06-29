/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
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

sis_encoder::sptr sis_encoder::make(const std::string& short_name)
{
    return gnuradio::get_initial_sptr(new sis_encoder_impl(short_name));
}


/*
 * The private constructor
 */
sis_encoder_impl::sis_encoder_impl(const std::string& short_name)
    : gr::sync_block("sis_encoder",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(unsigned char) * SIS_BITS))
{
    set_output_multiple(BLOCKS_PER_FRAME);
    alfn = 800000000;
    country_code = "US";
    fcc_facility_id = 1337;
    this->short_name = short_name;

    /* Station location vars
    S9.13 fractional format/WGS84: P.351 NRSC-5-E reference doc.
    also requires two frames to send Lo/Hi -> long/lat
    altitude is in units of 16m. okay.
    These will be converted to apropraite format
    */
    NS = 0;
    EW = 1;
    lat = 47;
    lon = 105;
    altitude = 2000;
    //computational vars
    //convert to apropriate values according to pg. 382 of NRSC5E standard
    nlat = std::round(lat*8192);
    //convert to negative value by taking 2's compliment if needed
    if (NS){nlat = (~nlat);}
    nlon = std::round(lon*8192);
    if (EW){nlon = (~nlon);}
    nalt = std::round(altitude/16);
    //var for determining frame number in sending lat/lon
    sendLat = 0;

    //vars for audio service descriptors
    programs = 2;
    progtypes = {15, 4};
    progno = 0;

    //vars for service parameter message
    sis7idx = 0;
    UTCoffset = -6;
    DSTSchedule = 1;
    DSTLocal = 1;
    DSTReg = 1;

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

            write_bit(PIDS_FORMATTED);

            switch (block) {
            case 0:
            case 2:
            case 4:
            case 6:
            case 9:
            case 14:
                write_bit(EXTENDED_FORMAT);
                write_station_name_short();
                write_station_id();
                break;
            case 1:
            case 5:
            case 10:
            case 15:
                write_bit(EXTENDED_FORMAT);
                write_service_information_message();
                write_service_information_message();
                break;
            case 3:
                write_bit(NO_EXTENSION);
                write_station_slogan();
                break;
            case 7:
                write_bit(EXTENDED_FORMAT);
                write_station_location();
                write_station_location();
                break;
            case 8:
            case 13:
                write_bit(NO_EXTENSION);
                write_station_message();
                break;
            case 11:
                write_bit(NO_EXTENSION);
                write_station_name_long();
                break;
            case 12:
                write_bit(EXTENDED_FORMAT);
                write_sis_parameter_message();
                write_station_id();
                break;
            }

            while (bit < start + 64) {
                write_bit(0);
            }
            write_bit(0); // Reserved
            write_bit(TIME_NOT_LOCKED);
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
    write_int(STATION_ID_NUMBER, 4);
    for (int i = 0; i < 2; i++) {
        write_char5(country_code[i]);
    }
    write_bit(0); // Reserved
    write_bit(0); // Reserved
    write_bit(0); // Reserved
    write_int(fcc_facility_id, 19);
}

void sis_encoder_impl::write_station_name_short()
{
    write_int(STATION_NAME_SHORT, 4);
    for (int i = 0; i < 4; i++) {
        write_char5(short_name[i]);
    }
    write_int(EXTENSION_FM, 2);
}

void sis_encoder_impl::write_station_name_long()
{
    write_int(STATION_NAME_LONG, 4);
    // TODO: Implement
}

void sis_encoder_impl::write_station_location()
{
    write_int(STATION_LOCATION, 4);
    //frame number
    write_bit(sendLat);
    if (sendLat){
        //send latitude and alt. bits 7-4
        write_int(nlat, 22);
        write_int(nalt >> 4, 4);
    }
    else {
        //send llongitude and alt. bits 3-0
        write_int(nlon, 22);
        write_int(nalt, 4);
    }
    sendLat = !sendLat;
}

void sis_encoder_impl::write_station_message()
{
    write_int(STATION_MESSAGE, 4);
    // TODO: Implement
}

void sis_encoder_impl::write_service_information_message()
{
    write_int(SERVICE_INFORMATION_MESSAGE, 4);
    //audio and data programs must be listed here
    //for now iterate through indicated audio programs
    //code 00 for audio program, 01 for data Pg. 355
    if (progno >= 0){
        //service category
        write_int(0, 2);
        //write access
        write_bit(0);
        //program number
        write_int(progno, 6);
        //program type
        write_int(progtypes[progno], 8);
        //reserved bits
        write_bit(0); // Reserved
        write_bit(0); // Reserved
        write_bit(0); // Reserved
        write_bit(0); // Reserved
        write_bit(0); // Reserved
        //sound experience
        write_int(0, 5);
    }
    progno++;
    if (progno > programs-1) {progno=0;}
}

void sis_encoder_impl::write_sis_parameter_message()
{
    write_int(SIS_PARAMETER_MESSAGE, 4);
    //write DST and various TX BS that we'll ignore pg. 361
    //reset frame count
    if (sis7idx > 7){sis7idx=0;}
    switch (sis7idx){
        case 0:
            //leap second offset
            write_int(sis7idx, 6);
            write_int(18, 8);
            write_int(18, 8);
        case 1:
            //GPS leap second ALFN
            write_int(sis7idx, 6);
            write_int(0, 16);
        case 2:
            //second half
            write_int(sis7idx, 6);
            write_int(0, 16);
        case 3:
            //local time data (DST and UTC offset)
            write_int(sis7idx, 6);
            write_int(static_cast<int>(UTCoffset*60), 11);
            write_int(DSTSchedule, 3);
            write_bit(DSTLocal);
            write_bit(DSTReg);
        case 4:
            //exciter man iD
            write_int(sis7idx, 6);
            write_bit(0); //reserved
            write_int(33, 7);
            write_bit(0);
            write_int(33, 7);
        case 5:
            //exciter core ver.
            write_int(sis7idx, 6);
            write_int(0, 5);
            write_int(0, 5);
            write_int(0, 5);
            write_bit(0); //reserved
        case 6:
            //exciter man. ver.
            write_int(sis7idx, 6);
            write_int(0, 5);
            write_int(0, 5);
            write_int(0, 5);
            write_bit(0); //reserved*/
        case 7:
            //exciter man. ver.
            write_int(sis7idx, 6);
            write_int(0, 5);
            write_int(0, 5);
            write_int(0, 3);
            write_int(0, 3);
        default:
            write_int(sis7idx, 6);
            write_int(0, 16);
    }
    sis7idx++;


}

void sis_encoder_impl::write_station_slogan()
{
    write_int(UNIVERSAL_SHORT_STATION_NAME, 4);
    // TODO: Implement
}

} /* namespace nrsc5 */
} /* namespace gr */
