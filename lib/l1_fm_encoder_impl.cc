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

      for (int scid = 0; scid < 4; scid++) {
        for (int bc = 0; bc < BLOCKS_PER_FRAME; bc++) {
          primary_sc_data_seq(primary_sc_symbols[scid] + (bc * SYMBOLS_PER_BLOCK), scid, 0, bc, 1);
        }
        unsigned char last_symbol = 0;
        for (int i = 0; i < SYMBOLS_PER_FRAME; i++) {
          if (primary_sc_symbols[scid][i]) {
            last_symbol ^= 3;
          }
          primary_sc_symbols[scid][i] = last_symbol;
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
      int out_off = 0;
      for (int in_off = 0; in_off < noutput_items; in_off += SYMBOLS_PER_FRAME * FFT_SIZE) {
        for (int i = 0; i < BLOCKS_PER_FRAME; i++) {
          reverse_bytes(pids + pids_off, pids_s, PIDS_BITS);
          scramble(pids_s, PIDS_BITS);
          conv_2_5(pids_s, pids_g + (PIDS_BITS * 5 / 2 * i), PIDS_BITS);
          pids_off += PIDS_BITS;
        }
        reverse_bytes(p1 + p1_off, p1_s, P1_BITS);
        scramble(p1_s, P1_BITS);
        conv_2_5(p1_s, p1_g, P1_BITS);
        interleaver_i_ii();
        p1_off += P1_BITS;

        for (int symbol = 0; symbol < SYMBOLS_PER_FRAME; symbol++) {
          for (int i = 0; i < FFT_SIZE; i++) {
            out[out_off + i] = 4;
          }

          for (int chan = 0; chan < 61; chan++) {
            out[out_off + REF_SC_CHAN[chan]] = primary_sc_symbols[REF_SC_ID[chan]][symbol];
            if (chan == 10) chan = 49;
          }

          int part = 0;
          for (int chan = 0; chan < 60; chan++) {
            for (int j = 0; j < 18; j++) {
              unsigned char ii = int_mat_i_ii[symbol][(part * 36) + (j * 2)];
              unsigned char qq = int_mat_i_ii[symbol][(part * 36) + (j * 2) + 1];
              unsigned char symbol = (ii << 1) | qq;
              int carrier = REF_SC_CHAN[chan] + j + 1;
              out[out_off + carrier] = symbol;
            }
            part++;
            if (chan == 9) chan = 49;
          }

          out_off += FFT_SIZE;
        }
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

    /* 1011sG.pdf sections 10.2.3 & 10.2.4 */
    void
    l1_fm_encoder_impl::interleaver_i_ii()
    {
      int J = 20; // number of partitions
      int B = 16; // blocks
      int C = 36; // columns per partition
      int M = 1;  // factor: 1, 2 or 4

      int b = 200;
      int N1 = 365440;
      int N2 = 3200;

      for (int i = 0; i < N1; i++) {
        int partition = V[((i + (2 * (M / 4))) / M) % J];
        int block;
        if (M == 1)
          block = ((i / J) + (partition * 7)) % B;
        else
          block = (i + (i / (J * B))) % B;
        int ki = i / (J * B);
        int row = (ki * 11) % 32;
        int col = ((ki * 11) + (ki / (32*9))) % C;
        int_mat_i_ii[(block * 32) + row][(partition * C) + col] = p1_g[i];
      }

      for (int i = 0; i < N2; i++) {
        int partition = V[i % J];
        int block = i / b;
        int ki = ((i / J) % (b / J)) + (N1 / (J * B));
        int row = (ki * 11) % 32;
        int col = ((ki * 11) + (ki / (32*9))) % C;
        int_mat_i_ii[(block * 32) + row][(partition * C) + col] = pids_g[i];
      }
    }

    /* 1011sG.pdf table 11-1 */
    void
    l1_fm_encoder_impl::primary_sc_data_seq(unsigned char *out, int scid, int sci, int bc, int psmi)
    {
      out[0] = 0;
      out[1] = 1;
      out[2] = 1;
      out[3] = 0;
      out[4] = 0;
      out[5] = 1;
      out[6] = 0;

      out[7] = 0;
      out[8] = out[7]; // parity

      out[9] = 1;

      out[10] = (scid & 0x2) >> 1;
      out[11] = (scid & 0x1);
      out[12] = sci;
      out[13] = out[10] ^ out[11] ^ out[12]; // parity

      out[14] = 0;

      out[15] = 0;
      out[16] = (bc & 0x8) >> 3;
      out[17] = (bc & 0x4) >> 2;
      out[18] = (bc & 0x2) >> 1;
      out[19] = (bc & 0x1);
      out[20] = out[15] ^ out[16] ^ out[17] ^ out[18] ^ out[19]; // parity

      out[21] = 1;
      out[22] = 1;

      out[23] = 1;
      out[24] = 0;
      out[25] = (psmi & 0x20) >> 5;
      out[26] = (psmi & 0x10) >> 4;
      out[27] = (psmi & 0x08) >> 3;
      out[28] = (psmi & 0x04) >> 2;
      out[29] = (psmi & 0x02) >> 1;
      out[30] = (psmi & 0x01);
      out[31] = out[23] ^ out[24] ^ out[25] ^ out[26] ^ out[27] ^ out[28] ^ out[29] ^ out[30]; // parity
    }

  } /* namespace nrsc5 */
} /* namespace gr */
