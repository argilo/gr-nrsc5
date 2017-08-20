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

#ifndef INCLUDED_NRSC5_L2_ENCODER_IMPL_H
#define INCLUDED_NRSC5_L2_ENCODER_IMPL_H

#include <nrsc5/l2_encoder.h>

namespace gr {
  namespace nrsc5 {

    class l2_encoder_impl : public l2_encoder
    {
     private:
      unsigned char rs_buf[255];
      void *rs_enc;

     public:
      l2_encoder_impl();
      ~l2_encoder_impl();

      // Where all the action really happens
      void forecast (int noutput_items, gr_vector_int &ninput_items_required);

      int general_work(int noutput_items,
           gr_vector_int &ninput_items,
           gr_vector_const_void_star &input_items,
           gr_vector_void_star &output_items);
    };

  } // namespace nrsc5
} // namespace gr

#endif /* INCLUDED_NRSC5_L2_ENCODER_IMPL_H */
