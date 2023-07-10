/* -*- c++ -*- */
/*
 * Copyright 2023 Clayton Smith.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_NRSC5_FCS_H
#define INCLUDED_NRSC5_FCS_H

#include <vector>

std::vector<unsigned char> hdlc_encode(std::vector<unsigned char> in);

#endif
