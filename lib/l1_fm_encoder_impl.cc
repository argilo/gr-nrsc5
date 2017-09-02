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
    l1_fm_encoder::make(const int psm)
    {
      return gnuradio::get_initial_sptr
        (new l1_fm_encoder_impl(psm));
    }

    /*
     * The private constructor
     */
    l1_fm_encoder_impl::l1_fm_encoder_impl(const int psm)
      : gr::block("l1_fm_encoder",
              gr::io_signature::make(2, 4, sizeof(unsigned char)),
              gr::io_signature::make(1, 1, sizeof(unsigned char)))
    {
      this->psm = psm;
      set_output_multiple(SYMBOLS_PER_FRAME * FFT_SIZE);

      p1_bits = 0;
      p2_bits = 0;
      p3_bits = 0;
      p4_bits = 0;
      p1_mod = 1;
      p2_mod = 1;
      p3_mod = 8;
      p4_mod = 8;
      switch(psm) {
      case 1:
        p1_bits = 146176;
        break;
      case 2:
        p1_bits = 146176;
        p3_bits = 2304;
        break;
      case 3:
        p1_bits = 146176;
        p3_bits = 4608;
        break;
      case 11:
        p1_bits = 146176;
        p3_bits = 4608;
        p4_bits = 4608;
        break;
      case 5:
        p1_bits = 4608;
        p1_mod = 8;
        p2_bits = 109312;
        p3_bits = 4608;
        break;
      case 6:
        p1_bits = 9216;
        p1_mod = 8;
        p2_bits = 72448;
        break;
      }

      if (p1_mod == 8) {
        p1_prime_off = 0;
        p1_prime = (unsigned char *) malloc(p1_bits * p1_mod * 3);
        p1_prime_s = (unsigned char *) malloc(p1_bits * p1_mod);
        p1_prime_g = (unsigned char *) malloc(p1_bits * 2 * p1_mod);
        px2_matrix = (unsigned char *) malloc(p1_bits * 2 * p1_mod);
      }
      if (p3_bits) {
        p3_p4_s = (unsigned char *) malloc(p3_bits);
        p3_p4_g = (unsigned char *) malloc(p3_bits * 2 * p3_mod);
        px1_matrix = (unsigned char *) malloc(p3_bits * 2 * p3_mod);
        px1_internal = (unsigned char *) malloc(p3_bits * 2 * p3_mod * 2);
      }
      if (p4_bits) {
        px2_matrix = (unsigned char *) malloc(p4_bits * 2 * p4_mod);
        px2_internal = (unsigned char *) malloc(p4_bits * 2 * p4_mod * 2);
      }
      internal_half = 0;

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
          primary_sc_data_seq(primary_sc_symbols[scid] + (bc * SYMBOLS_PER_BLOCK), scid, 0, bc, psm);
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
      if (p1_mod == 8) {
        free(p1_prime);
        free(p1_prime_s);
        free(p1_prime_g);
        free(px2_matrix);
      }
      if (p3_bits) {
        free(p3_p4_s);
        free(p3_p4_g);
        free(px1_matrix);
        free(px1_internal);
      }
      if (p4_bits) {
        free(px2_matrix);
        free(px2_internal);
      }
    }

    void
    l1_fm_encoder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
      int frames = noutput_items / (SYMBOLS_PER_FRAME * FFT_SIZE);
      int port = 0;

      ninput_items_required[port++] = frames * PIDS_BITS * BLOCKS_PER_FRAME;
      ninput_items_required[port++] = frames * p1_bits * p1_mod;
      if (p2_bits) ninput_items_required[port++] = frames * p2_bits * p2_mod;
      if (p3_bits) ninput_items_required[port++] = frames * p3_bits * p3_mod;
      if (p4_bits) ninput_items_required[port++] = frames * p4_bits * p4_mod;
    }

    int
    l1_fm_encoder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
      int port = 0;
      const unsigned char *pids = (const unsigned char *) input_items[port++];
      const unsigned char *p1 = (const unsigned char *) input_items[port++];
      const unsigned char *p2 = NULL;
      const unsigned char *p3 = NULL;
      const unsigned char *p4 = NULL;
      if (p2_bits) p2 = (const unsigned char*) input_items[port++];
      if (p3_bits) p3 = (const unsigned char*) input_items[port++];
      if (p4_bits) p4 = (const unsigned char*) input_items[port++];

      unsigned char *out = (unsigned char *) output_items[0];
      int frames = noutput_items / (SYMBOLS_PER_FRAME * FFT_SIZE);

      int pids_off = 0;
      int p1_off = 0;
      int p2_off = 0;
      int p3_off = 0;
      int p4_off = 0;
      int out_off = 0;
      for (int in_off = 0; in_off < noutput_items; in_off += SYMBOLS_PER_FRAME * FFT_SIZE) {
        for (int i = 0; i < BLOCKS_PER_FRAME; i++) {
          reverse_bytes(pids + pids_off, pids_s, PIDS_BITS);
          scramble(pids_s, PIDS_BITS);
          conv_2_5(pids_s, pids_g + (PIDS_BITS * 5 / 2 * i), PIDS_BITS);
          pids_off += PIDS_BITS;
        }

        if (p1_mod == 1) {
          reverse_bytes(p1 + p1_off, p1_s, p1_bits);
          scramble(p1_s, p1_bits);
          conv_2_5(p1_s, p1_g, p1_bits);
          p1_off += p1_bits;
        } else {
          for (int i = 0; i < p1_mod; i++) {
            reverse_bytes(p1 + p1_off, p1_s + (p1_bits * i), p1_bits);
            scramble(p1_s + (p1_bits * i), p1_bits);
            conv_2_5(p1_s + (p1_bits * i), p1_g + (p1_bits * 5 / 2 * i), p1_bits);

            reverse_bytes(p1_prime + p1_prime_off, p1_prime_s + (p1_bits * i), p1_bits);
            scramble(p1_prime_s + (p1_bits * i), p1_bits);
            conv_1_2(p1_prime_s + (p1_bits * i), p1_prime_g + (p1_bits * 2 * i), p1_bits);

            if (psm == 5) {
              interleaver_i(p1_prime_g + (p1_bits * 2 * i), px2_matrix + (p1_bits * 2 * i), 4, 2, 36, 2, V_PX2_MP5, 9216);
            } else {
              interleaver_i(p1_prime_g + (p1_bits * 2 * i), px2_matrix + (p1_bits * 2 * i), 8, 2, 36, 1, V_PX2_MP6, 18432);
            }

            memcpy(p1_prime + p1_prime_off, p1 + p1_off, p1_bits);
            p1_off += p1_bits;
            p1_prime_off = (p1_prime_off + p1_bits) % (p1_bits * p1_mod * 3);
          }
          reverse_bytes(p2 + p2_off, p1_s + (p1_bits * p1_mod), p2_bits);
          scramble(p1_s + (p1_bits * p1_mod), p2_bits);
          conv_2_5(p1_s + (p1_bits * p1_mod), p1_g + (p1_bits * 5 / 2 * p1_mod), p2_bits);
          p2_off += p2_bits;
        }
        interleaver_i(p1_g, pm_matrix, 20, 16, 36, 1, V_PM, 365440);
        interleaver_ii(pids_g, pm_matrix, 20, 16, 36, 1, V_PM, 200, 365440, 3200);

        if (p3_bits) {
          for (int i = 0; i < p3_mod; i++) {
            reverse_bytes(p3 + p3_off, p3_p4_s, p3_bits);
            scramble(p3_p4_s, p3_bits);
            conv_1_2(p3_p4_s, p3_p4_g + (p3_bits * 2 * i), p3_bits);
            p3_off += p3_bits;
          }
          interleaver_iv(px1_matrix, px1_internal, internal_half);
        }
        if (p4_bits) {
          for (int i = 0; i < p4_mod; i++) {
            reverse_bytes(p4 + p4_off, p3_p4_s, p4_bits);
            scramble(p3_p4_s, p4_bits);
            conv_1_2(p3_p4_s, p3_p4_g + (p4_bits * 2 * i), p4_bits);
            p4_off += p4_bits;
          }
          interleaver_iv(px2_matrix, px2_internal, internal_half);
        }
        internal_half ^= 1;

        for (int symbol = 0; symbol < SYMBOLS_PER_FRAME; symbol++) {
          for (int i = 0; i < FFT_SIZE; i++) {
            out[out_off + i] = 4;
          }

          for (int chan = 0; chan < 61; chan++) {
            out[out_off + REF_SC_CHAN[chan]] = primary_sc_symbols[REF_SC_ID[chan]][symbol];
            if (chan == partitions_per_band()) chan = 61 - partitions_per_band() - 2;
          }

          int pm_channels[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
            50, 51, 52, 53, 54, 55, 56, 57, 58, 59
          };
          write_symbol(pm_matrix + (symbol * 20 * 36), out + out_off, pm_channels, 20);

          if (psm == 2) {
            int px1_channels[] = { 10, 49 };
            write_symbol(px1_matrix + (symbol * 2 * 36), out + out_off, px1_channels, 2);
          }
          if (psm == 3 || psm == 11 || psm == 5) {
            int px1_channels[] = { 10, 11, 48, 49 };
            write_symbol(px1_matrix + (symbol * 4 * 36), out + out_off, px1_channels, 4);
          }
          if (psm == 11 || psm == 5) {
            int px2_channels[] = { 12, 13, 46, 47 };
            write_symbol(px2_matrix + (symbol * 4 * 36), out + out_off, px2_channels, 4);
          }
          if (psm == 6) {
            int px2_channels[] = { 10, 11, 12, 13, 46, 47, 48, 49 };
            write_symbol(px2_matrix + (symbol * 8 * 36), out + out_off, px2_channels, 8);
          }

          out_off += FFT_SIZE;
        }
      }

      port = 0;
      consume(port++, frames * PIDS_BITS * BLOCKS_PER_FRAME);
      consume(port++, frames * p1_bits * p1_mod);
      if (p2_bits) consume(port++, frames * p2_bits * p2_mod);
      if (p3_bits) consume(port++, frames * p3_bits * p3_mod);
      if (p4_bits) consume(port++, frames * p4_bits * p4_mod);

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

    /* 1011sG.pdf section 9.3.4.3 */
    void
    l1_fm_encoder_impl::conv_1_2(const unsigned char *in, unsigned char *out, int len)
    {
      unsigned char poly[3] = { 0133, 0165 };
      conv_enc(in, out, len, poly, 2, 2);
    }

    /* 1011sG.pdf sections 10.2.3 */
    void
    l1_fm_encoder_impl::interleaver_i(unsigned char *in, unsigned char *matrix,
      int J, int B, int C, int M, unsigned char *V, int N)
    {
      for (int i = 0; i < N; i++) {
        int partition = V[((i + 2 * (M / 4)) / M) % J];
        int block;
        if (M == 1)
          block = ((i / J) + (partition * 7)) % B;
        else
          block = (i + (i / (J * B))) % B;
        int ki = i / (J * B);
        int row = (ki * 11) % 32;
        int col = ((ki * 11) + (ki / (32 * 9))) % C;
        matrix[((block * 32) + row) * (J * C) + (partition * C) + col] = in[i];
      }
    }

    /* 1011sG.pdf sections 10.2.4 */
    void
    l1_fm_encoder_impl::interleaver_ii(unsigned char *in, unsigned char *matrix,
      int J, int B, int C, int M, unsigned char *V, int b, int I0, int N)
    {
      for (int i = 0; i < N; i++) {
        int partition = V[i % J];
        int block = i / b;
        int ki = ((i / J) % (b / J)) + (I0 / (J * B));
        int row = (ki * 11) % 32;
        int col = ((ki * 11) + (ki / (32 * 9))) % C;
        matrix[((block * 32) + row) * (J * C) + (partition * C) + col] = in[i];
      }
    }

    /* 1011sG.pdf sections 10.2.6 */
    void
    l1_fm_encoder_impl::interleaver_iv(unsigned char *matrix, unsigned char *internal, int half)
    {
      int J = psm == 2 ? 2 : 4;  // number of partitions
      int B = 32; // blocks
      int C = 36; // columns per partition
      int M = psm == 2 ? 4 : 2;  // factor: 1, 2 or 4
      int N = psm == 2 ? 73728 : 147456;

      int bk_bits = 32 * C;
      int bk_adj = 32 * C - 1;

      int internal_off = half * (N / 2);
      int pt[4];
      for (int i = 0; i < J; i++) {
        pt[i] = internal_off / J;
      }

      for (int i = 0; i < N / 2; i++) {
        int partition = ((i + 2 * (M / 4)) / M) % J;
        unsigned int pti = pt[partition]++;
        int block = (pti + (partition * 7) - (bk_adj * (pti / bk_bits))) % B;
        int row = ((11 * pti) % bk_bits) / C;
        int column = (pti * 11) % C;
        internal[(block * 32 + row) * (J * C) + partition * C + column] = p3_p4_g[i];
        matrix[i] = internal[internal_off++];
      }
    }

    void
    l1_fm_encoder_impl::write_symbol(unsigned char *matrix_row, unsigned char *out_row, int *channels, int num_channels)
    {
      int partition = 0;
      for (int i = 0; i < num_channels; i++) {
        for (int j = 0; j < 18; j++) {
          unsigned char ii = matrix_row[(partition * 36) + (j * 2)];
          unsigned char qq = matrix_row[(partition * 36) + (j * 2) + 1];
          unsigned char symbol = (ii << 1) | qq;
          int carrier = REF_SC_CHAN[channels[i]] + j + 1;
          out_row[carrier] = symbol;
        }
        partition++;
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

    int l1_fm_encoder_impl::partitions_per_band()
    {
      switch (psm) {
      case 2:
        return 11;
      case 3:
        return 12;
      case 5:
      case 6:
      case 11:
        return 14;
      default:
        return 10;
      }
    }

  } /* namespace nrsc5 */
} /* namespace gr */
