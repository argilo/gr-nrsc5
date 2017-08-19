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

      for (int i = 0; i < 128; i++) {
        int tmp = i;
        parity[i] = 0;
        while (tmp != 0) {
          parity[i] ^= 1;
          tmp &= (tmp - 1);
        }
      }
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
      for (int in_off = 0; in_off < noutput_items; in_off += SYMBOLS_PER_FRAME * FFT_SIZE) {
        for (int i = 0; i < BLOCKS_PER_FRAME; i++) {
          reverse_bytes(pids + pids_off, pids_s, PIDS_BITS);
          scramble(pids_s, PIDS_BITS);
          conv_2_5(pids_s, pids_g, PIDS_BITS);
          pids_off += PIDS_BITS;
        }
        reverse_bytes(p1 + p1_off, p1_s, P1_BITS);
        scramble(p1_s, P1_BITS);
        conv_2_5(p1_s, p1_g, P1_BITS);
        p1_off += P1_BITS;
      }

      consume(0, frames * PIDS_BITS * BLOCKS_PER_FRAME);
      consume(1, frames * P1_BITS);
      return noutput_items;
    }

    void
    l1_fm_encoder_impl::reverse_bytes(const unsigned char *in, unsigned char *out, int len)
    {
      for (int off = 0; off < len; off += 8) {
        for (int i = 0; i < 8; i++) {
          out[off + i] = in[off + 7 - i];
        }
      }
    }

    /* 1011sG.pdf section 8.2 */
    void
    l1_fm_encoder_impl::scramble(unsigned char *buf, int len)
    {
      unsigned int reg = 0x3ff;
      for (int off = 0; off < len; off++) {
        unsigned char next_bit = ((reg >> 9) ^ reg) & 1;
        buf[off] ^= next_bit;
        reg = (reg >> 1) | (next_bit << 10);
      }
    }

    /* 1011sG.pdf section 9.3 */
    void
    l1_fm_encoder_impl::conv_enc(const unsigned char *in, unsigned char *out, int len,
                                 const unsigned char *poly, int poly_l1, int poly_l2)
    {
      unsigned char reg = (in[len-6] << 1) | (in[len-5] << 2) | (in[len-4] << 3)
                        | (in[len-3] << 4) | (in[len-2] << 5) | (in[len-1] << 6);
      int out_off = 0;
      for (int in_off = 0; in_off < len; in_off++) {
        reg = (reg >> 1) | (in[in_off] << 6);
        for (int i = 0; i < ((in_off & 1) ? poly_l2 : poly_l1); i++) {
          out[out_off++] = parity[reg & poly[i]];
        }
      }
    }

    /* 1011sG.pdf section 9.3.4.2 */
    void
    l1_fm_encoder_impl::conv_2_5(const unsigned char *in, unsigned char *out, int len)
    {
      unsigned char poly[3] = { 0133, 0171, 0165 };
      conv_enc(in, out, len, poly, 3, 2);
    }

  } /* namespace nrsc5 */
} /* namespace gr */
