/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_HDC_ENCODER_H
#define INCLUDED_NRSC5_HDC_ENCODER_H

#include <gnuradio/block.h>
#include <nrsc5/api.h>

namespace gr {
namespace nrsc5 {

/*!
 * \brief <+description of block+>
 * \ingroup nrsc5
 *
 */
class NRSC5_API hdc_encoder : virtual public gr::block
{
public:
    typedef std::shared_ptr<hdc_encoder> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of nrsc5::hdc_encoder.
     *
     * To avoid accidental use of raw pointers, nrsc5::hdc_encoder's
     * constructor is in a private implementation
     * class. nrsc5::hdc_encoder::make is the public interface for
     * creating new instances.
     *
     * \param channels Number of audio channels (1 or 2)
     * \param bitrate Bitrate in bits per second
     * \param use_parametric_stereo Use Parametric Stereo (AOT 128) instead of Regular Stereo (AOT 127)
     * \param tx_digital_gain TX Digital Audio Gain in dB (-8 to +6)
     */
    static sptr make(int channels = 2, int bitrate = 64000, bool use_parametric_stereo = false, int tx_digital_gain = 0);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_HDC_ENCODER_H */
