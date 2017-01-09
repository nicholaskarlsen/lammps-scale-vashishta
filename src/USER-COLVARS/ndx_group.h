// -*- c++ -*-

/* ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef COMMAND_CLASS

CommandStyle(ndx2group,Ndx2Group)

#else

#ifndef LMP_NDX_GROUP_H
#define LMP_NDX_GROUP_H

#include "pointers.h"

namespace LAMMPS_NS {

class Ndx2Group : protected Pointers {
 public:
  Ndx2Group(class LAMMPS *lmp) : Pointers(lmp) {};
  void command(int, char **);
  void create(char *, bigint, tagint *);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Must have atom IDs for ndx2group command

There are no atom IDs defined in the system, but they are required
to identify atoms in a gromacs style index file.

E: Cannot open index file for reading

Self-explanatory. Check your filename, permissions, and disk space or quota.

*/
