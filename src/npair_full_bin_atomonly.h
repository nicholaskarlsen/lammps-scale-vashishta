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

#ifdef NPAIR_CLASS

NPairStyle(full/bin/atomonly,
           NPairFullBinAtomonly,
           NP_FULL | NP_BIN | NP_ATOMONLY |
           NP_NEWTON | NP_NEWTOFF | NP_ORTHO | NP_TRI)

#else

#ifndef LMP_NPAIR_FULL_BIN_ATOMONLY_H
#define LMP_NPAIR_FULL_BIN_ATOMONLY_H

#include "npair.h"

namespace LAMMPS_NS {

class NPairFullBinAtomonly : public NPair {
 public:
  NPairFullBinAtomonly(class LAMMPS *);
  ~NPairFullBinAtomonly() {}
  void build(class NeighList *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
