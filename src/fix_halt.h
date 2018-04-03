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

FixStyle(halt,FixHalt)

#else

#ifndef LMP_FIX_HALT_H
#define LMP_FIX_HALT_H

#include <stdio.h>
#include "fix.h"

namespace LAMMPS_NS {

class FixHalt : public Fix {
 public:
  FixHalt(class LAMMPS *, int, char **);
  ~FixHalt();
  int setmask();
  void init();
  void end_of_step();
  void post_run();

 private:
  int attribute,operation,eflag,msgflag,ivar;
  bigint nextstep;
  double value,tratio;
  char *idvar;

  double bondmax();
  double tlimit();
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Could not find fix halt variable name

UNDOCUMENTED

E: Fix halt variable is not equal-style variable

UNDOCUMENTED

E: Invalid fix halt attribute

UNDOCUMENTED

E: Invalid fix halt operator

UNDOCUMENTED

E: Fix halt %s condition met on step %ld with value %g

UNDOCUMENTED

U: Cannot open fix print file %s

The output file generated by the fix print command cannot be opened

*/
