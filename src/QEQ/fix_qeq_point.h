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

FixStyle(qeq/point,FixQEqPoint)

#else

#ifndef LMP_FIX_QEQ_POINT_H
#define LMP_FIX_QEQ_POINT_H

#include "fix_qeq.h"

namespace LAMMPS_NS {

class FixQEqPoint : public FixQEq {
 public:
  FixQEqPoint(class LAMMPS *, int, char **);
  ~FixQEqPoint() {}
  void init();
  void pre_force(int);

 private:
  void init_matvec();
  void compute_H();

};
}
#endif
#endif

/* ERROR/WARNING messages:

E: Fix qeq/point requires atom attribute q

Self-explanatory.

E: Fix qeq/point group has no atoms

Self-explanatory.

W: H matrix size has been exceeded: m_fill=%d H.m=%d\n

This is the size of the matrix.

E: Fix qeq/point has insufficient QEq matrix size

UNDOCUMENTED

*/
