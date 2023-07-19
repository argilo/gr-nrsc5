/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_L1_FM_ENCODER_H
#define INCLUDED_NRSC5_L1_FM_ENCODER_H

#include <gnuradio/block.h>
#include <nrsc5/api.h>

namespace gr {
namespace nrsc5 {

/*!
 * \brief <+description of block+>
 * \ingroup nrsc5
 *
 */
class NRSC5_API l1_fm_encoder : virtual public gr::block
{
public:
    typedef std::shared_ptr<l1_fm_encoder> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of nrsc5::l1_fm_encoder.
     *
     * To avoid accidental use of raw pointers, nrsc5::l1_fm_encoder's
     * constructor is in a private implementation
     * class. nrsc5::l1_fm_encoder::make is the public interface for
     * creating new instances.
     */
    static sptr make(const int psm, const int ssm = 0);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_L1_FM_ENCODER_H */
