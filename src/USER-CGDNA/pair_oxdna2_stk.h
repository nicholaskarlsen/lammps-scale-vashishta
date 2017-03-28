/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------
   Contributing author: Oliver Henrich (University of Strathclyde, Glasgow)
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(oxdna2/stk,PairOxdna2Stk)

#else

#ifndef LMP_PAIR_OXDNA2_STK_H
#define LMP_PAIR_OXDNA2_STK_H

#include "pair_oxdna_stk.h"

namespace LAMMPS_NS {

class PairOxdna2Stk : public PairOxdnaStk {
 public:
  PairOxdna2Stk(class LAMMPS *);
  virtual ~PairOxdna2Stk();

 protected:
  virtual double stacking_strength(double);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

*/
