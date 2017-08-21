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

      int in_off = 0;

      for (int out_off = 0; out_off < noutput_items; out_off += P1_FRAME_LEN) {
        memset(out_buf, 0, (P1_FRAME_LEN - 24) / 8);
        int nop = 32;
        int la_loc = 8 + 6 + nop*2 + 128 - 1;

        // PDU control word
        out_buf[8] = pdu_seq_no ? 64 : 0;
        out_buf[9] = 4;
        out_buf[10] = 0;
        out_buf[11] = 1;
        out_buf[12] = pdu_seq_no ? 193 : 192;
        out_buf[13] = 205;

        int end = la_loc;
        for (int i = 0; i < nop; i++) {
          int length = ((in[in_off+3] & 0x03) << 11 | (in[in_off+4] << 3) | (in[in_off+5] >> 5)) - 7;
          in_off += 7;
          unsigned char crc_reg = 0xff;
          for (int j = 0; j < length; j++) {
            crc_reg = CRC8_TABLE[crc_reg ^ in[in_off]];
            out_buf[++end] = in[in_off++];
          }
          out_buf[++end] = crc_reg;
          out_buf[14+i*2] = (end & 0xff);
          out_buf[14+i*2+1] = (end >> 8);
        }

        // HEF
        out_buf[14+nop*2] = 144;
        out_buf[14+nop*2+1] = 160;
        out_buf[14+nop*2+2] = 14;

        // Reed-Solomon encoding
        for (int i = 95; i >= 8; i--) {
          rs_buf[255-i-1] = out_buf[i];
        }
        encode_rs_char(rs_enc, rs_buf, rs_buf + 247);
        for (int i = 7; i >= 0; i--) {
          out_buf[i] = rs_buf[255-i-1];
        }

        header_spread(out_buf, out + out_off, CW0);

        pdu_seq_no ^= 1;
      }

      consume(0, in_off);
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
