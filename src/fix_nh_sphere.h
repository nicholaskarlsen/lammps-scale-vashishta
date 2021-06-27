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

#ifndef LMP_FIX_NH_SPHERE_H
#define LMP_FIX_NH_SPHERE_H

#include "fix_nh.h"

namespace LAMMPS_NS {

class FixNHSphere : public FixNH {
 public:
  FixNHSphere(class LAMMPS *, int, char **);
  virtual ~FixNHSphere() {}
  void init();

 protected:
  double inertia;

  void nve_v();
  void nve_x();
  void nh_v_temp();
};

}    // namespace LAMMPS_NS

#endif

/* ERROR/WARNING messages:

E: Fix nvt/nph/npt sphere requires atom style sphere

Self-explanatory.

E: Fix nvt/nph/npt sphere disc option requires 2d simulation

UNDOCUMENTED

E: Fix nvt/npt/nph/sphere require extended particles

UNDOCUMENTED

U: Fix nvt/sphere requires extended particles

This fix can only be used for particles of a finite size.

*/
