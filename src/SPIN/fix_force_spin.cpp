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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fix_force_spin.h"
#include "atom.h"
#include "update.h"
#include "domain.h"
#include "respa.h"
#include "modify.h"
#include "input.h"
#include "variable.h"
#include "math_const.h"
#include "error.h"
#include "force.h"
#include "memory.h"

using namespace LAMMPS_NS;
using namespace FixConst;
using namespace MathConst;

enum{ZEEMAN,ANISOTROPY};
enum{CONSTANT,EQUAL};

/* ---------------------------------------------------------------------- */

FixForceSpin::FixForceSpin(LAMMPS *lmp, int narg, char **arg) : Fix(lmp, narg, arg)
{
	
  if (narg < 7) error->all(FLERR,"Illegal fix spin command");
  // 7 arguments for a force/spin fix command:
  //(fix  ID  group  force/spin  magnitude (T or eV)  style (zeeman or anisotropy)  direction (3 cartesian coordinates) 
  
  //Magnetic interactions only coded for cartesian coordinates

  dynamic_group_allow = 1;
  scalar_flag = 1;
  global_freq = 1;
  extscalar = 1;
  respa_level_support = 1;
  ilevel_respa = 0;
  
  magstr = NULL;
  magfieldstyle = CONSTANT;
  
  H_field = 0.0;
  Hx = Hy = Hz = 0.0;
  Ka = 0.0;
  Kax = Kay = Kaz = 0.0;
 
  zeeman_flag = aniso_flag = 0;
 
  if (strcmp(arg[3],"zeeman") == 0) {
	  if (narg != 8) error->all(FLERR,"Illegal fix zeeman command");
	  style = ZEEMAN;
          zeeman_flag = 1;
	  H_field = force->numeric(FLERR,arg[4]);
	  Hx = force->numeric(FLERR,arg[5]);
	  Hy = force->numeric(FLERR,arg[6]);
	  Hz = force->numeric(FLERR,arg[7]);	
	  magfieldstyle = CONSTANT; 	  	  
  } else if (strcmp(arg[3],"anisotropy") == 0) {
	  if (narg != 8) error->all(FLERR,"Illegal fix anisotropy command");
	  style = ANISOTROPY;
          aniso_flag = 1;
	  Ka = force->numeric(FLERR,arg[4]);
	  Kax = force->numeric(FLERR,arg[5]);
	  Kay = force->numeric(FLERR,arg[6]);
	  Kaz = force->numeric(FLERR,arg[7]);	  
  } else error->all(FLERR,"Illegal fix force/spin command");
 
  degree2rad = MY_PI/180.0;
  time_origin = update->ntimestep;

  eflag = 0;
  emag = 0.0;  
}

/* ---------------------------------------------------------------------- */

FixForceSpin::~FixForceSpin()
{
  memory->destroy(spi);
  memory->destroy(fmi);
  delete [] magstr;
}

/* ---------------------------------------------------------------------- */

int FixForceSpin::setmask()
{
  int mask = 0;
  mask |= POST_FORCE;
  mask |= THERMO_ENERGY;
  mask |= POST_FORCE_RESPA;
  return mask;
}


/* ---------------------------------------------------------------------- */

void FixForceSpin::init()
{
  double hbar = force->hplanck/MY_2PI; //eV/(rad.THz)
  double mub = 5.78901e-5; //in eV/T 
  double gyro = mub/hbar; //in rad.THz/T  

  H_field *= gyro; //in rad.THz
  Ka /= hbar; //in rad.THz

  if (strstr(update->integrate_style,"respa")) {
    ilevel_respa = ((Respa *) update->integrate)->nlevels-1;
    if (respa_level >= 0) ilevel_respa = MIN(respa_level,ilevel_respa);
  }

  // check variables
  if (magstr) {
  magvar = input->variable->find(magstr);
  if (magvar < 0) 
        error->all(FLERR,"Variable name for fix magnetic field does not exist");
  if (!input->variable->equalstyle(magvar))
        error->all(FLERR,"Variable for fix magnetic field is invalid style");
  }
  
  varflag = CONSTANT;
  if (magfieldstyle != CONSTANT) varflag = EQUAL;
 
  // set magnetic field components once and for all
  if (varflag == CONSTANT) set_magneticforce();
  
  memory->create(spi,3,"forcespin:spi"); 
  memory->create(fmi,3,"forcespin:fmi"); 

}

/* ---------------------------------------------------------------------- */

void FixForceSpin::setup(int vflag)
{
  if (strstr(update->integrate_style,"verlet"))
    post_force(vflag);
  else {
    ((Respa *) update->integrate)->copy_flevel_f(ilevel_respa);
    post_force_respa(vflag,ilevel_respa,0);
    ((Respa *) update->integrate)->copy_f_flevel(ilevel_respa);
  }
}

/* ---------------------------------------------------------------------- */

void FixForceSpin::post_force(int vflag)
{
  // update gravity due to variables
  if (varflag != CONSTANT) {
    modify->clearstep_compute();
    modify->addstep_compute(update->ntimestep + 1);
    set_magneticforce(); //Update value of the mag. field if time-dependent
  }

//  double **x = atom->x;
  double **sp = atom->sp; 
  double *mumag = atom->mumag;
  double **fm = atom->fm; 
  int nlocal = atom->nlocal;  
  double scalar;
  
  eflag = 0;
  emag = 0.0;

  for (int i = 0; i < nlocal; i++) {
    fmi[0] = fmi[1] = fmi[2] = 0.0;
    //if (style == ZEEMAN) {
    if (zeeman_flag) {
      compute_zeeman(i,fmi);
    }  
    //if (style == ANISOTROPY) {
    if (aniso_flag) {
      spi[0] = sp[i][0];
      spi[1] = sp[i][1];
      spi[2] = sp[i][2];
      compute_anisotropy(i,spi,fmi);
    }
    fm[i][0] += fmi[0];
    fm[i][1] += fmi[1];
    fm[i][2] += fmi[2];
  }  
}


/* ---------------------------------------------------------------------- */
void FixForceSpin::compute_zeeman(int i, double *fmi)
{
double *mumag = atom->mumag;
  fmi[0] += mumag[i]*xmag;
  fmi[1] += mumag[i]*ymag;
  fmi[2] += mumag[i]*zmag;
}

void FixForceSpin::compute_anisotropy(int i, double * spi, double *fmi)
{
  double scalar = Kax*spi[0] + Kay*spi[1] + Kaz*spi[2];
  fmi[0] += scalar*xmag;
  fmi[1] += scalar*ymag;
  fmi[2] += scalar*zmag;
}

/* ---------------------------------------------------------------------- */

void FixForceSpin::post_force_respa(int vflag, int ilevel, int iloop)
{
  if (ilevel == ilevel_respa) post_force(vflag);
}

/* ---------------------------------------------------------------------- */
//No acceleration for magnetic EOM, only a "magnetic force" 
void FixForceSpin::set_magneticforce()
{
  if (style == ZEEMAN) {
	  xmag = H_field*Hx;
	  ymag = H_field*Hy;
	  zmag = H_field*Hz;	  
  }
  if (style == ANISOTROPY) {
	  xmag = 2.0*Ka*Kax;
	  ymag = 2.0*Ka*Kay;
	  zmag = 2.0*Ka*Kaz;	  
  }
}

/* ----------------------------------------------------------------------
   potential energy in magnetic field
------------------------------------------------------------------------- */

double FixForceSpin::compute_scalar()
{
  // only sum across procs one time
  if (eflag == 0) {
    MPI_Allreduce(&emag,&emag_all,1,MPI_DOUBLE,MPI_SUM,world);
    eflag = 1;
  }
  return emag_all;
}
