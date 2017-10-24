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

/* ------------------------------------------------------------------------
   Contributing authors: Julien Tranchida (SNL)
                         Aidan Thompson (SNL)
------------------------------------------------------------------------- */

#include <mpi.h>
#include <string.h>
#include "atom.h"
#include "compute_spin.h"
#include "domain.h"
#include "error.h"
#include "force.h"
#include "math_special.h"
#include "math_const.h"
#include "memory.h"
#include "modify.h"
#include "update.h"

using namespace LAMMPS_NS;
using namespace MathSpecial;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

ComputeSpin::ComputeSpin(LAMMPS *lmp, int narg, char **arg) : 
  Compute(lmp, narg, arg), mag(NULL)
{
  if ((narg != 3) && (narg != 4)) error->all(FLERR,"Illegal compute compute/spin command");

  vector_flag = 1;
  size_vector = 7;
  extvector = 0;

  init();

  allocate();
  
}

/* ---------------------------------------------------------------------- */

ComputeSpin::~ComputeSpin()
{
  memory->destroy(mag);
}

/* ---------------------------------------------------------------------- */

void ComputeSpin::init()
{
  hbar = force->hplanck/MY_2PI;
  kb = force->boltz;
}

/* ---------------------------------------------------------------------- */

void ComputeSpin::compute_vector()
{
  int i, index;
 
  invoked_vector = update->ntimestep;
  
  countsp = countsptot = 0.0;	
  mag[0] = mag[1] = mag[2] = mag[3] = 0.0; 
  magtot[0] = magtot[1] = magtot[2] = magtot[3] = 0.0; 
  magenergy = magenergytot = 0.0; 
  tempnum = tempnumtot = 0.0;
  tempdenom = tempdenomtot = 0.0; 
  spintemperature = 0.0;  

  double **x = atom->x;
  int *mask = atom->mask;
  int *type = atom->type;
  imageint *image = atom->image;
  double *mumag = atom->mumag;
  double **sp = atom->sp;  
  double **fm = atom->fm;
  double tx,ty,tz;
	
  int nlocal = atom->nlocal;

  // compute total magnetization and magnetic energy 
  // compute spin temperature (Nurdin et al., Phys. Rev. E 61, 2000)
  for (i = 0; i < nlocal; i++) {
    if (mask[i] & groupbit) {
      if (atom->mumag_flag && atom->sp_flag) {
		mag[0] += sp[i][0];
		mag[1] += sp[i][1];
		mag[2] += sp[i][2];
		magenergy -= sp[i][0]*fm[i][0];  
		magenergy -= sp[i][1]*fm[i][1]; 
		magenergy -= sp[i][2]*fm[i][2];
                tx = sp[i][1]*fm[i][2]-sp[i][2]*fm[i][1];
                ty = sp[i][2]*fm[i][0]-sp[i][0]*fm[i][2];
                tz = sp[i][0]*fm[i][1]-sp[i][1]*fm[i][0];
                tempnum += tx*tx+ty*ty+tz*tz;
                tempdenom += sp[i][0]*fm[i][0]+fm[i][1]*sp[i][1]+sp[i][2]*fm[i][2];  	
		countsp++;
                }
      }
      else error->all(FLERR,"Compute spin/compute declared magnetic quantities (sp and mumag flags)");
  }

  MPI_Allreduce(mag,magtot,4,MPI_DOUBLE,MPI_SUM,world);
  MPI_Allreduce(&magenergy,&magenergytot,1,MPI_DOUBLE,MPI_SUM,world);
  MPI_Allreduce(&tempnum,&tempnumtot,1,MPI_DOUBLE,MPI_SUM,world);
  MPI_Allreduce(&tempdenom,&tempdenomtot,1,MPI_DOUBLE,MPI_SUM,world);
  MPI_Allreduce(&countsp,&countsptot,1,MPI_INT,MPI_SUM,world);
  
  double scale = 1.0/countsptot;
  magtot[0] *= scale;
  magtot[1] *= scale;
  magtot[2] *= scale;    
  magtot[3] = sqrt((magtot[0]*magtot[0])+(magtot[1]*magtot[1])+(magtot[2]*magtot[2]));
  spintemperature = hbar*tempnumtot;    
  spintemperature /= (kb*tempdenomtot);
 
  vector[0] = invoked_vector*update->dt;
  vector[1] = magtot[0];
  vector[2] = magtot[1];
  vector[3] = magtot[2];
  vector[4] = magtot[3];
  vector[5] = magenergytot*hbar; 
  vector[6] = spintemperature;
 
}

/* ----------------------------------------------------------------------
   free and reallocate arrays
------------------------------------------------------------------------- */

void ComputeSpin::allocate()
{
  memory->destroy(mag);
  memory->create(mag,4,"compute/spin:mag");
  memory->create(magtot,5,"compute/spin:mag");
  memory->create(vector,7,"compute/spin:vector");
}

