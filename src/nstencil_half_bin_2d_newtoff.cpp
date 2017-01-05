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

#include "nstencil_half_bin_2d_newtoff.h"
#include "neighbor.h"
#include "neigh_list.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NStencilHalfBin2dNewtoff::NStencilHalfBin2dNewtoff(LAMMPS *lmp) : 
  NStencil(lmp) {}

/* ----------------------------------------------------------------------
   create stencil based on bin geometry and cutoff
------------------------------------------------------------------------- */

void NStencilHalfBin2dNewtoff::create()
{
  int i,j;

  nstencil = 0;

  for (j = -sy; j <= sy; j++)
    for (i = -sx; i <= sx; i++)
      if (bin_distance(i,j,0) < cutneighmaxsq)
        stencil[nstencil++] = j*mbinx + i;
}
