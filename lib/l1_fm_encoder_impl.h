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

#ifndef INCLUDED_NRSC5_L1_FM_ENCODER_IMPL_H
#define INCLUDED_NRSC5_L1_FM_ENCODER_IMPL_H

#include <nrsc5/l1_fm_encoder.h>

namespace gr {
  namespace nrsc5 {

    class l1_fm_encoder_impl : public l1_fm_encoder
    {
     private:
      unsigned char pids_s[PIDS_BITS];
      unsigned char p1_s[P1_BITS];
      unsigned char pids_g[PIDS_BITS * 5 / 2];
      unsigned char p1_g[P1_BITS * 5 / 2];
      unsigned char parity[128];

      void reverse_bytes(const unsigned char *in, unsigned char *out, int len);
      void scramble(unsigned char *buf, int len);
      void conv_enc(const unsigned char *in, unsigned char *out, int len,
                    const unsigned char *poly, int poly_l1, int poly_l2);
      void conv_2_5(const unsigned char *in, unsigned char *out, int len);

     public:
      l1_fm_encoder_impl();
      ~l1_fm_encoder_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_L1_FM_ENCODER_IMPL_H */
