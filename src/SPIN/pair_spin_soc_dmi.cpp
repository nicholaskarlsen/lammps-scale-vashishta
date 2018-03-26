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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "pair_hybrid.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "math_const.h"
#include "memory.h"
#include "pair_spin_soc_dmi.h"
#include "update.h"

using namespace LAMMPS_NS;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

PairSpinSocDmi::PairSpinSocDmi(LAMMPS *lmp) : Pair(lmp)
{
  hbar = force->hplanck/MY_2PI;

  single_enable = 0;
  soc_dmi_flag = 0;

  no_virial_fdotr_compute = 1;
}

/* ---------------------------------------------------------------------- */

PairSpinSocDmi::~PairSpinSocDmi()
{
  if (allocated) {
    memory->destroy(setflag);
    
    memory->destroy(cut_soc_dmi);
    memory->destroy(DM);
    memory->destroy(v_dmx);
    memory->destroy(v_dmy);
    memory->destroy(v_dmz);

    memory->destroy(cutsq);
  }
}

/* ---------------------------------------------------------------------- */

void PairSpinSocDmi::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;  
  double evdwl, ecoul;
  double xi[3], rij[3];
  double spi[3], spj[3];
  double fi[3], fmi[3];
  double cut_soc_dmi_2;
  double rsq, rd, inorm;
  int *ilist,*jlist,*numneigh,**firstneigh;  

  evdwl = ecoul = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;
  cut_soc_dmi_2 = cut_soc_global*cut_soc_global;
  
  double **x = atom->x;
  double **f = atom->f;
  double **fm = atom->fm;
  double **sp = atom->sp;	
  int *type = atom->type;  
  int nlocal = atom->nlocal;  
  int newton_pair = force->newton_pair;
  
  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // dmi computation
  // loop over neighbors of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xi[0] = x[i][0];
    xi[1] = x[i][1];
    xi[2] = x[i][2];
    jlist = firstneigh[i];
    jnum = numneigh[i]; 
    spi[0] = sp[i][0]; 
    spi[1] = sp[i][1]; 
    spi[2] = sp[i][2];
  
    // loop on neighbors
    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;

      spj[0] = sp[j][0]; 
      spj[1] = sp[j][1]; 
      spj[2] = sp[j][2]; 

      evdwl = 0.0;

      fi[0] = fi[1] = fi[2] = 0.0;
      fmi[0] = fmi[1] = fmi[2] = 0.0;
      rij[0] = rij[1] = rij[2] = 0.0;
     
      rij[0] = x[j][0] - xi[0];
      rij[1] = x[j][1] - xi[1];
      rij[2] = x[j][2] - xi[2];
      rsq = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2]; 
      inorm = 1.0/sqrt(rsq);
      rij[0] *= inorm;
      rij[1] *= inorm;
      rij[2] *= inorm;
      
      itype = type[i];
      jtype = type[j];

      // compute magnetic and mechanical components of soc_dmi
      
      if (soc_dmi_flag){
        cut_soc_dmi_2 = cut_soc_dmi[itype][jtype]*cut_soc_dmi[itype][jtype];
        if (rsq <= cut_soc_dmi_2){
          compute_soc_dmi(i,j,fmi,spi,spj);
          compute_soc_dmi_mech(i,j,fi,spi,spj);
        } 
      }

      f[i][0] += fi[0];	 
      f[i][1] += fi[1];	  	  
      f[i][2] += fi[2];
      fm[i][0] += fmi[0];	 
      fm[i][1] += fmi[1];	  	  
      fm[i][2] += fmi[2];

      // check newton pair  =>  see if needs correction
      if (newton_pair || j < nlocal) {
	f[j][0] -= fi[0];	 
        f[j][1] -= fi[1];	  	  
        f[j][2] -= fi[2];
      }
 
      if (eflag) {
	if (rsq <= cut_soc_dmi_2) {
	  evdwl -= spi[0]*fmi[0];
	  evdwl -= spi[1]*fmi[1];
	  evdwl -= spi[2]*fmi[2];
	  evdwl *= hbar;
	} else evdwl = 0.0;
      }

      if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
	  evdwl,ecoul,fi[0],fi[1],fi[2],rij[0],rij[1],rij[2]);
    }
  }  

  if (vflag_fdotr) virial_fdotr_compute();
  
}

/* ---------------------------------------------------------------------- */

void PairSpinSocDmi::compute_soc_dmi(int i, int j, double fmi[3],  double spi[3], double spj[3])
{
  int *type = atom->type;  
  int itype, jtype;
  double dmix,dmiy,dmiz;	
  itype = type[i];
  jtype = type[j];
          
  dmix = DM[itype][jtype]*v_dmx[itype][jtype];
  dmiy = DM[itype][jtype]*v_dmy[itype][jtype];
  dmiz = DM[itype][jtype]*v_dmz[itype][jtype];

  fmi[0] += spj[1]*dmiz-spj[2]*dmiy;
  fmi[1] += spj[2]*dmix-spj[0]*dmiz;
  fmi[2] += spj[0]*dmiy-spj[1]*dmix;

}

/* ---------------------------------------------------------------------- */

void PairSpinSocDmi::compute_soc_dmi_mech(int i, int j, double fi[3], double spi[3], double spj[3])
{
  fi[0] += 0.0;
  fi[1] += 0.0;
  fi[2] += 0.0;
}


/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairSpinSocDmi::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;
      
  memory->create(cut_soc_dmi,n+1,n+1,"pair:cut_soc_dmi");
  memory->create(DM,n+1,n+1,"pair:DM");
  memory->create(v_dmx,n+1,n+1,"pair:DM_vector_x");
  memory->create(v_dmy,n+1,n+1,"pair:DM_vector_y");
  memory->create(v_dmz,n+1,n+1,"pair:DM_vector_z");
 
  memory->create(cutsq,n+1,n+1,"pair:cutsq");  
  
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairSpinSocDmi::settings(int narg, char **arg)
{
  if (narg < 1 || narg > 2)
    error->all(FLERR,"Incorrect number of args in pair/spin/dmi command");

  if (strcmp(update->unit_style,"metal") != 0)
    error->all(FLERR,"Spin simulations require metal unit style");
    
  cut_soc_global = force->numeric(FLERR,arg[0]);
    
  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i+1; j <= atom->ntypes; j++)
        if (setflag[i][j]) {
          cut_soc_dmi[i][j] = cut_soc_global;
        }
  }
   
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type spin pairs (only one for now)
------------------------------------------------------------------------- */

void PairSpinSocDmi::coeff(int narg, char **arg)
{
  const double hbar = force->hplanck/MY_2PI;

  if (!allocated) allocate();

  if (strcmp(arg[2],"dmi")==0) {
    if (narg != 8) error->all(FLERR,"Incorrect args in pair_style command");
    soc_dmi_flag = 1;   

    int ilo,ihi,jlo,jhi;
    force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
    force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);
    
    const double rij = force->numeric(FLERR,arg[3]);
    const double dm = (force->numeric(FLERR,arg[4]))/hbar;
    double dmx = force->numeric(FLERR,arg[5]);  
    double dmy = force->numeric(FLERR,arg[6]); 
    double dmz = force->numeric(FLERR,arg[7]); 

    double inorm = 1.0/(dmx*dmx+dmy*dmy+dmz*dmz);
    dmx *= inorm; 
    dmy *= inorm; 
    dmz *= inorm; 
 
    int count = 0;
    for (int i = ilo; i <= ihi; i++) {
      for (int j = MAX(jlo,i); j <= jhi; j++) {
        cut_soc_dmi[i][j] = rij;
        DM[i][j] = dm;
        v_dmx[i][j] = dmx;
        v_dmy[i][j] = dmy;
        v_dmz[i][j] = dmz;
        setflag[i][j] = 1;
        count++;
      }
    }
    if (count == 0) error->all(FLERR,"Incorrect args in pair_style command"); 
  } else error->all(FLERR,"Incorrect args in pair_style command");

}


/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairSpinSocDmi::init_style()
{
  if (!atom->sp_flag)
    error->all(FLERR,"Pair spin requires atom/spin style");

  neighbor->request(this,instance_me);

  // check this half/full request  => to be verified
  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;

}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairSpinSocDmi::init_one(int i, int j)
{
   
   if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  return cut_soc_global;
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinSocDmi::write_restart(FILE *fp)
{
  write_restart_settings(fp);
  
  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        if (soc_dmi_flag) {
          fwrite(&DM[i][j],sizeof(double),1,fp);
          fwrite(&v_dmx[i][j],sizeof(double),1,fp);
          fwrite(&v_dmy[i][j],sizeof(double),1,fp);
          fwrite(&v_dmz[i][j],sizeof(double),1,fp);
          fwrite(&cut_soc_dmi[i][j],sizeof(double),1,fp);
        } 
      }
    }
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinSocDmi::read_restart(FILE *fp)
{
  read_restart_settings(fp);

  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) {
        if (me == 0) {
          fread(&DM[i][j],sizeof(double),1,fp);
          fread(&v_dmx[i][j],sizeof(double),1,fp);
          fread(&v_dmy[i][j],sizeof(double),1,fp);
          fread(&v_dmz[i][j],sizeof(double),1,fp);
          fread(&cut_soc_dmi[i][j],sizeof(double),1,fp);
        }
        MPI_Bcast(&DM[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_dmx[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_dmy[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_dmz[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut_soc_dmi[i][j],1,MPI_DOUBLE,0,world);
      }
    }
  }
}

 
/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinSocDmi::write_restart_settings(FILE *fp)
{
  fwrite(&cut_soc_global,sizeof(double),1,fp);
  fwrite(&offset_flag,sizeof(int),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinSocDmi::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&cut_soc_global,sizeof(double),1,fp);
    fread(&offset_flag,sizeof(int),1,fp);
    fread(&mix_flag,sizeof(int),1,fp);
  }
  MPI_Bcast(&cut_soc_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&offset_flag,1,MPI_INT,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world); 
}
