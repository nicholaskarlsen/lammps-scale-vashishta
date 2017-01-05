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

#ifdef NBIN_CLASS

NBinStyle(standard,
          NBinStandard,
          0)

#else

#ifndef LMP_NBIN_STANDARD_H
#define LMP_NBIN_STANDARD_H

#include "nbin.h"

namespace LAMMPS_NS {

class NBinStandard : public NBin {
 public:
  NBinStandard(class LAMMPS *);
  ~NBinStandard() {}
  void setup_bins(int);
  void bin_atoms();
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
