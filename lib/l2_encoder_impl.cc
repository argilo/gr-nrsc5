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
#include "l2_encoder_impl.h"

extern "C" {
#include <gnuradio/fec/rs.h>
}

namespace gr {
  namespace nrsc5 {

    l2_encoder::sptr
    l2_encoder::make()
    {
      return gnuradio::get_initial_sptr
        (new l2_encoder_impl());
    }

    /*
     * The private constructor
     */
    l2_encoder_impl::l2_encoder_impl()
      : gr::block("l2_encoder",
              gr::io_signature::make(1, 1, sizeof(unsigned char)),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      set_output_multiple(P1_FRAME_LEN);
      rs_enc = init_rs_char(8, 0x11d, 1, 1, 8);
    }

    /*
     * Our virtual destructor.
     */
    l2_encoder_impl::~l2_encoder_impl()
    {
      free_rs_char(rs_enc);
    }

    void
    l2_encoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      ninput_items_required[0] = noutput_items / 8;
    }

    int
    l2_encoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned char *in = (const unsigned char *) input_items[0];
      unsigned char *out = (unsigned char *) output_items[0];

      consume(0, noutput_items / 8);
      return noutput_items;
    }

  } /* namespace nrsc5 */
} /* namespace gr */
