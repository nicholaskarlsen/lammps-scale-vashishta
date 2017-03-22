/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------
   Contributing author: Oliver Henrich (EPCC, University of Edinburgh)
------------------------------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include "bond_oxdna2_fene.h"
#include "atom.h"
#include "neighbor.h"
#include "domain.h"
#include "comm.h"
#include "update.h"
#include "force.h"
#include "memory.h"
#include "error.h"
#include "atom_vec_ellipsoid.h"
#include "math_extra.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

BondOxdna2Fene::BondOxdna2Fene(LAMMPS *lmp) : BondOxdnaFene(lmp)
{

}

/* ---------------------------------------------------------------------- */

BondOxdna2Fene::~BondOxdna2Fene()
{

}

/* ----------------------------------------------------------------------
    compute vector COM-sugar-phosphate backbone interaction site in oxDNA2
------------------------------------------------------------------------- */
void BondOxdna2Fene::compute_interaction_sites(double e1[3],
  double e2[3], double r[3])
{
  double d_cs_x=-0.34, d_cs_y=+0.3408;

  r[0] = d_cs_x*e1[0] + d_cs_y*e2[0]; 
  r[1] = d_cs_x*e1[1] + d_cs_y*e2[1]; 
  r[2] = d_cs_x*e1[2] + d_cs_y*e2[2]; 

}
