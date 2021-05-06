// clang-format off
/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "nstencil_half_ghost_bin_2d_newtoff.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NStencilHalfGhostBin2dNewtoff::
NStencilHalfGhostBin2dNewtoff(LAMMPS *lmp) : NStencil(lmp)
{
  xyzflag = 1;
}

/* ----------------------------------------------------------------------
   create stencil based on bin geometry and cutoff
------------------------------------------------------------------------- */

void NStencilHalfGhostBin2dNewtoff::create()
{
  int i,j;

  nstencil = 0;

  for (j = -sy; j <= sy; j++)
    for (i = -sx; i <= sx; i++)
      if (bin_distance(i,j,0) < cutneighmaxsq) {
        stencilxyz[nstencil][0] = i;
        stencilxyz[nstencil][1] = j;
        stencilxyz[nstencil][2] = 0;
        stencil[nstencil++] = j*mbinx + i;
      }
}
