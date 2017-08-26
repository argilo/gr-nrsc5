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
    l2_encoder::make(const int num_progs)
    {
      return gnuradio::get_initial_sptr
        (new l2_encoder_impl(num_progs));
    }

    /*
     * The private constructor
     */
    l2_encoder_impl::l2_encoder_impl(const int num_progs)
      : gr::block("l2_encoder",
              gr::io_signature::make(2, 16, sizeof(unsigned char)),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      this->num_progs = num_progs;
      set_output_multiple(P1_FRAME_LEN);
      rs_enc = init_rs_char(8, 0x11d, 1, 1, 8);
      memset(rs_buf, 0, 255);
      pdu_seq_no = 0;
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
      for (int p = 0; p < num_progs; p++) {
        ninput_items_required[p] = noutput_items / 8;
        ninput_items_required[num_progs + p] = noutput_items / P1_FRAME_LEN * (128 - 3);
      }
    }

    int
    l2_encoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      const unsigned char **hdc = (const unsigned char **) &input_items[0];
      const unsigned char **psd = (const unsigned char **) &input_items[num_progs];
      unsigned char *out = (unsigned char *) output_items[0];

      int hdc_off[8] = {0};
      int psd_off[8] = {0};

      for (int out_off = 0; out_off < noutput_items; out_off += P1_FRAME_LEN) {
        memset(out_buf, 0, (P1_FRAME_LEN - 24) / 8);

        unsigned char *out_program = out_buf;
        for (int p = 0; p < num_progs; p++) {
          int nop = 32;
          int la_loc = 8 + 6 + nop*2 + 128 - 1;

          // PDU control word
          out_program[8] = pdu_seq_no ? 64 : 0;
          out_program[9] = 4;
          out_program[10] = 0;
          out_program[11] = 1;
          out_program[12] = pdu_seq_no ? 193 : 192;
          out_program[13] = 205;

          int end = la_loc;
          for (int i = 0; i < nop; i++) {
            int length = ((hdc[p][hdc_off[p]+3] & 0x03) << 11 | (hdc[p][hdc_off[p]+4] << 3) | (hdc[p][hdc_off[p]+5] >> 5)) - 7;
            hdc_off[p] += 7;
            unsigned char crc_reg = 0xff;
            for (int j = 0; j < length; j++) {
              crc_reg = CRC8_TABLE[crc_reg ^ hdc[p][hdc_off[p]]];
              out_program[++end] = hdc[p][hdc_off[p]++];
            }
            out_program[++end] = crc_reg;
            out_program[14+i*2] = (end & 0xff);
            out_program[14+i*2+1] = (end >> 8);
          }

          // HEF
          out_program[14+nop*2] = 144 | (p << 1);
          out_program[14+nop*2+1] = 160;
          out_program[14+nop*2+2] = 14;

          memcpy(out_program + (14+nop*2+3), psd[p] + psd_off[p], 128 - 3);
          psd_off[p] += (128 - 3);

          // Reed-Solomon encoding
          for (int i = 95; i >= 8; i--) {
            rs_buf[255-i-1] = out_program[i];
          }
          encode_rs_char(rs_enc, rs_buf, rs_buf + 247);
          for (int i = 7; i >= 0; i--) {
            out_program[i] = rs_buf[255-i-1];
          }

          out_program += (end + 1);
        }

        header_spread(out_buf, out + out_off, CW0);

        pdu_seq_no ^= 1;
      }

      for (int p = 0; p < num_progs; p++) {
        consume(p, hdc_off[p]);
        consume(num_progs + p, psd_off[p]);
      }
      return noutput_items;
    }

    /* 1014sI.pdf figure 5-2 */
    void
    l2_encoder_impl::header_spread(const unsigned char *in, unsigned char *out, unsigned char *pci)
    {
      int n_start = P1_FRAME_LEN - 30000;
      int n_offset = 1247;

      int out_off = 0;
      int pci_off = 0;
      for (int i = 0; i < (P1_FRAME_LEN - 24) / 8; i++) {
        for (int j = 0; j < 8; j++) {
          if ((out_off >= n_start) && (pci_off < 24) && ((out_off - n_start) % (n_offset + 1) == 0)) {
            out[out_off++] = pci[pci_off++];
          }
          out[out_off++] = (in[i] >> (7-j)) & 1;
        }
      }
    }

  } /* namespace nrsc5 */
} /* namespace gr */
