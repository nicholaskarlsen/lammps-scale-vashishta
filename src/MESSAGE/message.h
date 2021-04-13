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

#ifdef COMMAND_CLASS

CommandStyle(message,Message)

#else

#ifndef LMP_MESSAGE_H
#define LMP_MESSAGE_H

#include "command.h"

namespace LAMMPS_NS {

class Message : protected Command {
 public:
  Message(class LAMMPS *lmp) : Command(lmp) {};
  void command(int, char **);

 private:
  void quit();
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
