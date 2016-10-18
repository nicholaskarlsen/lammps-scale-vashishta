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

#ifdef FIX_CLASS

FixStyle(shardlow,FixShardlow)

#else

#ifndef LMP_FIX_SHARDLOW_H
#define LMP_FIX_SHARDLOW_H

#include "fix.h"

namespace LAMMPS_NS {

class FixShardlow : public Fix {
 public:
  FixShardlow(class LAMMPS *, int, char **);
  ~FixShardlow();
  int setmask();
  virtual void setup(int);
  virtual void initial_integrate(int);
  void setup_pre_exchange();
  void pre_exchange();
  void min_setup_pre_exchange();
  void min_pre_exchange();

  void grow_arrays(int);
  void copy_arrays(int, int, int);
  void set_arrays(int);

  int pack_border(int, int *, double *);
  int unpack_border(int, int, double *);
  int unpack_exchange(int, double *);
  void unpack_restart(int, int);

  double memory_usage();

 protected:
  int pack_reverse_comm(int, int, double *);
  void unpack_reverse_comm(int, int *, double *);
  int pack_forward_comm(int , int *, double *, int, int *);
  void unpack_forward_comm(int , int , double *);

  class PairDPDfdt *pairDPD;
  class PairDPDfdtEnergy *pairDPDE;
  double (*v_t0)[3];

 private:
  double dtsqrt; // = sqrt(update->dt);

  int coord2ssaAIR(double *);  // map atom coord to an AIR number
  void ssa_update(int, int *, int, class RanMars *);


};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Must use dpd/fdt pair_style with fix shardlow

Self-explanatory.

E: Must use pair_style dpd/fdt or dpd/fdt/energy with fix shardlow

E: A deterministic integrator must be specified after fix shardlow in input
file (e.g. fix nve or fix nph).

Self-explanatory.

E: Cannot use constant temperature integration routines with DPD

Self-explanatory.  Must use deterministic integrators such as nve or nph

E: Fix shardlow does not yet support triclinic geometries

Self-explanatory.

E:  Shardlow algorithm requires sub-domain length > 2*(rcut+skin). Either
reduce the number of processors requested, or change the cutoff/skin

The Shardlow splitting algorithm requires the size of the sub-domain lengths
to be are larger than twice the cutoff+skin.  Generally, the domain decomposition
is dependant on the number of processors requested.

*/
