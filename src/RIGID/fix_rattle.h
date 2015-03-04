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

FixStyle(rattle,FixRattle)

#else

#ifndef LMP_FIX_RATTLE_H
#define LMP_FIX_RATTLE_H

#include "fix.h"
#include "fix_shake.h"

namespace LAMMPS_NS {

class FixRattle : public FixShake {
 public:
  double **vp;                // array for unconstrained velocities
  double dtfv;                // timestep for velocity update
  int comm_mode;              // mode for communication pack/unpack

  FixRattle(class LAMMPS *, int, char **);
  ~FixRattle();
  int setmask();
  virtual void init();
  virtual void post_force(int);
  virtual void post_force_respa(int, int, int);
  virtual void final_integrate();
  virtual void final_integrate_respa(int,int);
  virtual void coordinate_constraints_end_of_step();

  virtual double memory_usage();
  virtual void grow_arrays(int);
  virtual int pack_forward_comm(int, int *, double *, int, int *);
  virtual void unpack_forward_comm(int, int, double *);
  virtual void reset_dt();

 private:
  void update_v_half_nocons();
  void update_v_half_nocons_respa(int);

  void vrattle2(int m);
  void vrattle3(int m);
  void vrattle4(int m);
  void vrattle3angle(int m);
  void solve3x3exactly(const double a[][3], const double c[], double l[]);
  void solve2x2exactly(const double a[][2], const double c[], double l[]);

  // debugging methosd

  bool check3angle(double ** v, int m, bool checkr, bool checkv);
  bool check2(double **v, int m, bool checkr, bool checkv);
  bool check3(double **v, int m, bool checkr, bool checkv);
  bool check4(double **v, int m, bool checkr, bool checkv);
  bool check_constraints(double **v, bool checkr, bool checkv);
  void end_of_step();
};

}

#endif
#endif
