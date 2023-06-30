/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_SIS_ENCODER_H
#define INCLUDED_NRSC5_SIS_ENCODER_H

#include <gnuradio/sync_block.h>
#include <nrsc5/api.h>

#define SIS_BITS 80
#define BLOCKS_PER_FRAME 16

/* 1020s.pdf Figure 4-1 */
#define PIDS_FORMATTED 0

#define NO_EXTENSION 0
#define EXTENDED_FORMAT 1

#define STATION_ID_NUMBER 0
#define STATION_NAME_SHORT 1
#define STATION_NAME_LONG 2
#define ALFN 3
#define STATION_LOCATION 4
#define STATION_MESSAGE 5
#define SERVICE_INFORMATION_MESSAGE 6
#define SIS_PARAMETER_MESSAGE 7
#define UNIVERSAL_SHORT_STATION_NAME 8
#define ACTIVE_RADIO_MESSAGE 9

#define NAME_TYPE_USSN 0
#define NAME_TYPE_SLOGAN 1

#define TIME_NOT_LOCKED 0
#define TIME_LOCKED 1

#define EXTENSION_NONE 0
#define EXTENSION_FM 1

namespace gr {
namespace nrsc5 {

enum class program_type {
    UNDEFINED = 0,
    NEWS = 1,
    INFORMATION = 2,
    SPORTS = 3,
    TALK = 4,
    ROCK = 5,
    CLASSIC_ROCK = 6,
    ADULT_HITS = 7,
    SOFT_ROCK = 8,
    TOP_40 = 9,
    COUNTRY = 10,
    OLDIES = 11,
    SOFT = 12,
    NOSTALGIA = 13,
    JAZZ = 14,
    CLASSICAL = 15,
    RHYTHM_AND_BLUES = 16,
    SOFT_RHYTHM_AND_BLUES = 17,
    FOREIGN_LANGUAGE = 18,
    RELIGIOUS_MUSIC = 19,
    RELIGIOUS_TALK = 20,
    PERSONALITY = 21,
    PUBLIC = 22,
    COLLEGE = 23,
    SPANISH_TALK = 24,
    SPANISH_MUSIC = 25,
    HIP_HOP = 26,
    WEATHER = 29,
    EMERGENCY_TEST = 30,
    EMERGENCY = 31,
    TRAFFIC = 65,
    SPECIAL_READING_SERVICES = 76
};

/*!
 * \brief <+description of block+>
 * \ingroup nrsc5
 *
 */
class NRSC5_API sis_encoder : virtual public gr::sync_block
{
public:
    typedef std::shared_ptr<sis_encoder> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of nrsc5::sis_encoder.
     *
     * To avoid accidental use of raw pointers, nrsc5::sis_encoder's
     * constructor is in a private implementation
     * class. nrsc5::sis_encoder::make is the public interface for
     * creating new instances.
     */
    static sptr make(const std::string& short_name = "ABCD");
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_SIS_ENCODER_H */
