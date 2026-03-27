/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_L2_ENCODER_H
#define INCLUDED_NRSC5_L2_ENCODER_H

#include <gnuradio/block.h>
#include <nrsc5/api.h>

namespace gr {
namespace nrsc5 {

enum class blend { DISABLE, SELECT, ENABLE };

/*!
 * \brief <+description of block+>
 * \ingroup nrsc5
 *
 */
class NRSC5_API l2_encoder : virtual public gr::block
{
public:
    typedef std::shared_ptr<l2_encoder> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of nrsc5::l2_encoder.
     *
     * To avoid accidental use of raw pointers, nrsc5::l2_encoder's
     * constructor is in a private implementation
     * class. nrsc5::l2_encoder::make is the public interface for
     * creating new instances.
     *
     * \param num_progs Number of programs
     * \param first_prog First program number
     * \param size PDU size in bits
     * \param data_bytes Number of data bytes
     * \param blend_control Blend control mode
     * \param tx_digital_gain TX Digital Audio Gain in dB (range: -8 to +6 dB)
     * \param debug_logs Enable debug logging for L2 Lot ID port operations
     * \param ccc_width Configuration control channel width (1-30 bytes)
     */
    static sptr make(const int num_progs,
                     const int first_prog,
                     const int size,
                     const int data_bytes = 0,
                     const blend blend_control = blend::ENABLE,
                     const int tx_digital_gain = 0,
                     const bool debug_logs = false,
                     const int ccc_width = 24);
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_L2_ENCODER_H */
