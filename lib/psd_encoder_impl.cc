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
#include "psd_encoder_impl.h"

namespace gr {
  namespace nrsc5 {

    psd_encoder::sptr
    psd_encoder::make(const std::string& title, const std::string& artist)
    {
      return gnuradio::get_initial_sptr
        (new psd_encoder_impl(title, artist));
    }

    /*
     * The private constructor
     */
    psd_encoder_impl::psd_encoder_impl(const std::string& title, const std::string& artist)
      : gr::sync_block("psd_encoder",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      seq_num = 0;
    }

    /*
     * Our virtual destructor.
     */
    psd_encoder_impl::~psd_encoder_impl()
    {
    }

    int
    psd_encoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      unsigned char *out = (unsigned char *) output_items[0];

      // Do <+signal processing+>

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

  } /* namespace nrsc5 */
} /* namespace gr */
