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

FixStyle(propel/self,FixPropelSelf)

#else

#ifndef LMP_FIX_PROPEL_SELF_H
#define LMP_FIX_PROPEL_SELF_H

#include "fix.h"

namespace LAMMPS_NS {

class FixPropelSelf : public Fix {
 public:

  enum operation_modes {
    VELOCITY = 0,
    QUATERNION = 1
  };
	
  FixPropelSelf(class LAMMPS *, int, char **);
  virtual ~FixPropelSelf();
  virtual int setmask();
  virtual void post_force(int);
  // virtual void post_force_respa(int, int, int);

  double memory_usage();

private:
  double magnitude;
  int thermostat_orient;
  int mode;

  int verify_atoms_have_quaternion();
  void post_force_velocity(int);
  void post_force_quaternion(int);
	
	
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

*/
