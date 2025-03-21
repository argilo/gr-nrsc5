/*
 * Copyright 2021 Free Software Foundation, Inc.
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
/* BINDTOOL_GEN_AUTOMATIC(0)                                                       */
/* BINDTOOL_USE_PYGCCXML(0)                                                        */
/* BINDTOOL_HEADER_FILE(l2_encoder.h)                                        */
/* BINDTOOL_HEADER_FILE_HASH(b0ded61b840bd7589aff93c0979dcf5d)                     */
/***********************************************************************************/

#include <pybind11/complex.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include <nrsc5/l2_encoder.h>
// pydoc.h is automatically generated in the build directory
#include <l2_encoder_pydoc.h>

void bind_l2_encoder(py::module& m)
{

    using l2_encoder    = ::gr::nrsc5::l2_encoder;

    py::enum_<::gr::nrsc5::blend>(m, "blend")
        .value("DISABLE", ::gr::nrsc5::blend::DISABLE)
        .value("SELECT", ::gr::nrsc5::blend::SELECT)
        .value("ENABLE", ::gr::nrsc5::blend::ENABLE)
        .export_values();

    py::class_<l2_encoder, gr::block, gr::basic_block,
        std::shared_ptr<l2_encoder>>(m, "l2_encoder", D(l2_encoder))

        .def(py::init(&l2_encoder::make),
           py::arg("num_progs"),
           py::arg("first_prog"),
           py::arg("size"),
           py::arg("data_bytes") = 0,
           py::arg("blend_control") = ::gr::nrsc5::blend::ENABLE,
           D(l2_encoder,make)
        )
        



        ;




}








