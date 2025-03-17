/* -*- c++ -*- */
/*
 * Copyright 2025 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "am_pulse_shaper_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace nrsc5 {

am_pulse_shaper::sptr am_pulse_shaper::make()
{
    return gnuradio::make_block_sptr<am_pulse_shaper_impl>();
}


/*
 * The private constructor
 */
am_pulse_shaper_impl::am_pulse_shaper_impl()
    : gr::sync_interpolator(
          "am_pulse_shaper",
          gr::io_signature::make(1, 1, sizeof(gr_complex) * AM_FFT_SIZE),
          gr::io_signature::make(1, 1, sizeof(gr_complex)),
          AM_FFTCP_SIZE)
{
    set_history(2);
}

/*
 * Our virtual destructor.
 */
am_pulse_shaper_impl::~am_pulse_shaper_impl() {}

int am_pulse_shaper_impl::work(int noutput_items,
                               gr_vector_const_void_star& input_items,
                               gr_vector_void_star& output_items)
{
    auto in = static_cast<const gr_complex*>(input_items[0]);
    auto out = static_cast<gr_complex*>(output_items[0]);

    int in_offset = 0, out_offset = 0;
    while (out_offset < noutput_items) {
        for (int i = 0; i < AM_FFT_SIZE; i++) {
            out[out_offset + i] = in[in_offset + i] * pulse[AM_FFT_SIZE + i];
        }
        for (int i = AM_FFT_SIZE; i < AM_FFTCP_SIZE; i++) {
            out[out_offset + i] = 0;
        }
        for (int i = 0; i < AM_FFT_SIZE; i++) {
            out[out_offset + AM_CP_SIZE + i] +=
                in[in_offset + AM_FFT_SIZE + i] * pulse[i];
        }

        in_offset += AM_FFT_SIZE;
        out_offset += AM_FFTCP_SIZE;
    }

    return noutput_items;
}

} /* namespace nrsc5 */
} /* namespace gr */
