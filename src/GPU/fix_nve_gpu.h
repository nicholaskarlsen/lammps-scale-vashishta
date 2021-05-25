/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://www.lammps.org/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing author: W. Michael Brown (Intel)
------------------------------------------------------------------------- */

#ifdef FIX_CLASS
// clang-format off
FixStyle(nve/gpu,FixNVEGPU);
// clang-format on
#else

#ifndef LMP_FIX_NVE_GPU_H
#define LMP_FIX_NVE_GPU_H

#include "fix_nve.h"

namespace LAMMPS_NS {

class FixNVEGPU : public FixNVE {
 public:
  FixNVEGPU(class LAMMPS *, int, char **);
  virtual ~FixNVEGPU();
  virtual void setup(int);
  virtual void initial_integrate(int);
  virtual void final_integrate();
  virtual void reset_dt();
  virtual double memory_usage();

 protected:
  void reset_dt_omp(const int, const int, const int);
  double *_dtfm;
  int _nlocal_max, _respa_on;
};

}    // namespace LAMMPS_NS

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

*/
