/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include "test_config.h"
#include <string>

extern TestConfig test_config;
extern bool print_stats;
extern bool verbose;

#define EXPECT_FP_LE_WITH_EPS(val1,val2,eps)                \
    do {                                                    \
        const double diff = fabs(val1-val2);                \
        const double div = std::min(fabs(val1),fabs(val2)); \
        const double err = (div == 0.0) ? diff : diff/div;  \
        stats.add(err);                                     \
        EXPECT_PRED_FORMAT2(::testing::DoubleLE, err, eps); \
    } while (0);

#endif

#define STRINGIFY(val) XSTR(val)
#define XSTR(val) #val
static const std::string INPUT_FOLDER = STRINGIFY(TEST_INPUT_FOLDER);
#undef STRINGIFY
#undef XSTR

#if defined _WIN32
static const char PATH_SEP = '\\';
#else
static const char PATH_SEP = '/';
#endif
