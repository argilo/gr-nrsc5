/* -*- c++ -*- */
/*
 * Copyright 2017 Clayton Smith.
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */


#ifndef INCLUDED_NRSC5_PIDS_ENCODER_H
#define INCLUDED_NRSC5_PIDS_ENCODER_H

#include <nrsc5/api.h>
#include <gnuradio/sync_block.h>

#define PIDS_BITS 80
#define BLOCKS_PER_FRAME 16

namespace gr {
  namespace nrsc5 {

    /*!
     * \brief <+description of block+>
     * \ingroup nrsc5
     *
     */
    class NRSC5_API pids_encoder : virtual public gr::sync_block
    {
     public:
      typedef boost::shared_ptr<pids_encoder> sptr;

      /*!
       * \brief Return a shared_ptr to a new instance of nrsc5::pids_encoder.
       *
       * To avoid accidental use of raw pointers, nrsc5::pids_encoder's
       * constructor is in a private implementation
       * class. nrsc5::pids_encoder::make is the public interface for
       * creating new instances.
       */
      static sptr make();
    };

  } // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_PIDS_ENCODER_H */
