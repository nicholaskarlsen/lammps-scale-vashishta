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

#include <cstring>
#include "fix_npt_omp.h"
#include "modify.h"
#include "error.h"

using namespace LAMMPS_NS;
using namespace FixConst;

/* ---------------------------------------------------------------------- */

FixNPTOMP::FixNPTOMP(LAMMPS *lmp, int narg, char **arg) :
  FixNHOMP(lmp, narg, arg)
{
  if (!tstat_flag)
    error->all(FLERR,"Temperature control must be used with fix npt/omp");
  if (!pstat_flag)
    error->all(FLERR,"Pressure control must be used with fix npt/omp");

  // create a new compute temp style
  // id = fix-ID + temp
  // compute group = all since pressure is always global (group all)
  // and thus its KE/temperature contribution should use group all

  id_temp = utils::strdup(std::string(id)+"_temp");
  modify->add_compute(std::string(id_temp)+" all temp");
  tcomputeflag = 1;

  // create a new compute pressure style
  // id = fix-ID + press, compute group = all
  // pass id_temp as 4th arg to pressure constructor

  id_press = utils::strdup(std::string(id)+"_press");
  modify->add_compute(std::string(id_press)+" all pressure "+id_temp);
  pcomputeflag = 1;
}
