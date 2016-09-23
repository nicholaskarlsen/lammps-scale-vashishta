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

#ifndef LMP_IMBALANCE_STORE_H
#define LMP_IMBALANCE_STORE_H

#include "imbalance.h"

namespace LAMMPS_NS {

class ImbalanceStore : public Imbalance {
 public:
  ImbalanceStore(class LAMMPS *);
  virtual ~ImbalanceStore();

 public:
  // parse options, return number of arguments consumed
  virtual int options(int, char **);
  // compute per-atom imbalance and apply to weight array
  virtual void compute(double *);
  // print information about the state of this imbalance compute (required)
  virtual void info(FILE *);

 private:
  char *name;                  // property name
};

}

#endif
