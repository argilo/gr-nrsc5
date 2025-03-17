/* -*- c++ -*- */
/*
 * Copyright 2025 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_AM_PULSE_SHAPER_H
#define INCLUDED_NRSC5_AM_PULSE_SHAPER_H

#include <gnuradio/sync_interpolator.h>
#include <nrsc5/api.h>

namespace gr {
namespace nrsc5 {

/*!
 * \brief <+description of block+>
 * \ingroup nrsc5
 *
 */
class NRSC5_API am_pulse_shaper : virtual public gr::sync_interpolator
{
public:
    typedef std::shared_ptr<am_pulse_shaper> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of nrsc5::am_pulse_shaper.
     *
     * To avoid accidental use of raw pointers, nrsc5::am_pulse_shaper's
     * constructor is in a private implementation
     * class. nrsc5::am_pulse_shaper::make is the public interface for
     * creating new instances.
     */
    static sptr make();
};

} // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_AM_PULSE_SHAPER_H */
