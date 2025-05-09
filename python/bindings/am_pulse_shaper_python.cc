/*
 * Copyright 2025 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/***********************************************************************************/
/* This file is automatically generated using bindtool and can be manually edited  */
/* The following lines can be configured to regenerate this file during cmake      */
/* If manual edits are made, the following tags should be modified accordingly.    */
/* BINDTOOL_GEN_AUTOMATIC(1)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(am_pulse_shaper.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(63976a7ef63bf73db3b965a0f491ce28)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <nrsc5/am_pulse_shaper.h>
// pydoc.h is automatically generated in the build directory
#include <am_pulse_shaper_pydoc.h>

void bind_am_pulse_shaper(py::module& m)
{

    using am_pulse_shaper = gr::nrsc5::am_pulse_shaper;


    py::class_<am_pulse_shaper,
               gr::block,
               gr::basic_block,
               std::shared_ptr<am_pulse_shaper>>(m, "am_pulse_shaper", D(am_pulse_shaper))

        .def(py::init(&am_pulse_shaper::make), D(am_pulse_shaper, make))


        ;
}
