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
#include "pids_encoder_impl.h"

namespace gr {
  namespace nrsc5 {

    pids_encoder::sptr
    pids_encoder::make()
    {
      return gnuradio::get_initial_sptr
        (new pids_encoder_impl());
    }

    /*
     * The private constructor
     */
    pids_encoder_impl::pids_encoder_impl()
      : gr::sync_block("pids_encoder",
              gr::io_signature::make(0, 0, 0),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      set_output_multiple(PIDS_BITS * BLOCKS_PER_FRAME);
      alfn = 800000000;
    }

    /*
     * Our virtual destructor.
     */
    pids_encoder_impl::~pids_encoder_impl()
    {
    }

    int
    pids_encoder_impl::work(int noutput_items,
        gr_vector_const_void_star &input_items,
        gr_vector_void_star &output_items)
    {
      unsigned char *out = (unsigned char *) output_items[0];

      int off = 0;
      while (off < noutput_items) {
        for (int block = 0; block < BLOCKS_PER_FRAME; block++) {
          for (int i = 0; i < 68; i++) {
            out[off + i] = 0;
          }
          out[off + 5] = 1;
          calc_crc12(out + off);
          off += PIDS_BITS;
        }
        alfn++;
      }

      // Tell runtime system how many output items we produced.
      return noutput_items;
    }

    /* 1020sI.pdf section 4.10
     * Note: The specified CRC is incorrect. It's actually a 16-bit CRC
     * truncated to 12 bits, and g(x) = X^16 + X^11 + X^3 + X + 1 */
    void
    pids_encoder_impl::calc_crc12(unsigned char *pids) {
      unsigned short poly = 0xD010;
      unsigned short reg = 0x0000;
      int i, lowbit;

      for (i = 67; i >= 0; i--) {
        lowbit = reg & 1;
        reg >>= 1;
        reg ^= ((unsigned short)pids[i] << 15);
        if (lowbit) reg ^= poly;
      }
      for (i = 0; i < 16; i++) {
        lowbit = reg & 1;
        reg >>= 1;
        if (lowbit) reg ^= poly;
      }
      reg ^= 0x955;
      for (i = 0; i < 12; i++) {
        pids[68+i] = (reg >> (11-i)) & 1;
      }
    }

  } /* namespace nrsc5 */
} /* namespace gr */
