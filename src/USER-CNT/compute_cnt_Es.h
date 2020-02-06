/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
   
   Contributing author: Maxim Shugaev (UVA), mvs9t@virginia.edu
------------------------------------------------------------------------- */

#ifdef COMPUTE_CLASS

ComputeStyle(cnt/Es,ComputeCNT_Es)

#else

#ifndef LMP_COMPUTE_CNT_ES_ATOM_H
#define LMP_COMPUTE_CNT_ES_ATOM_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputeCNT_Es : public Compute {
 public:
  ComputeCNT_Es(class LAMMPS *, int, char **);
  ~ComputeCNT_Es();
  void init() {}
  void compute_peratom();
  int pack_reverse_comm(int, int, double *);
  void unpack_reverse_comm(int, int *, double *);
  double memory_usage();

 private:
  int nmax;
  double *energy;
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal compute cnt/Es command

Incorrect argument list in the compute init.

E: Per-atom energy was not tallied on needed timestep

UNSPECIFIED.

E: cnt/Es is allowed only with cnt pair style

Use cnt pair style.

*/