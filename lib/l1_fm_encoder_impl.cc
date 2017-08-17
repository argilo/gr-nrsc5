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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "l1_fm_encoder_impl.h"

namespace gr {
  namespace nrsc5 {

    l1_fm_encoder::sptr
    l1_fm_encoder::make()
    {
      return gnuradio::get_initial_sptr
        (new l1_fm_encoder_impl());
    }

    /*
     * The private constructor
     */
    l1_fm_encoder_impl::l1_fm_encoder_impl()
      : gr::block("l1_fm_encoder",
              gr::io_signature::make(2, 2, sizeof(unsigned char)),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      set_output_multiple(SYMBOLS_PER_FRAME * FFT_SIZE);
    }

    /*
     * Our virtual destructor.
     */
    l1_fm_encoder_impl::~l1_fm_encoder_impl()
    {
    }

    void
    l1_fm_encoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      int frames = noutput_items / (SYMBOLS_PER_FRAME * FFT_SIZE);
      ninput_items_required[0] = frames * PIDS_BITS * BLOCKS_PER_FRAME;
      ninput_items_required[1] = frames * P1_BITS;
    }

    int
    l1_fm_encoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned char *pids = (const unsigned char *) input_items[0];
      const unsigned char *p1 = (const unsigned char *) input_items[1];
      unsigned char *out = (unsigned char *) output_items[0];
      int frames = noutput_items / (SYMBOLS_PER_FRAME * FFT_SIZE);

      int pids_off = 0;
      int p1_off = 0;
      for (int in_off = 0; in_off < noutput_items; in_off += SYMBOLS_PER_FRAME * FFT_SIZE)
      {
        for (int i = 0; i < BLOCKS_PER_FRAME; i++) {
          for (int j = 0; j < PIDS_BITS; j++) {
          }
          pids_off += PIDS_BITS;
        }
        p1_off += P1_BITS;
      }

      consume(0, frames * PIDS_BITS * BLOCKS_PER_FRAME);
      consume(1, frames * P1_BITS);
      return noutput_items;
    }

  } /* namespace nrsc5 */
} /* namespace gr */
