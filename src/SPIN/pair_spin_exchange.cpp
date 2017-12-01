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
#include "pair_spin_exchange.h"
#include "update.h"

using namespace LAMMPS_NS;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

PairSpinExchange::PairSpinExchange(LAMMPS *lmp) : Pair(lmp)
{
  hbar = force->hplanck/MY_2PI;

  newton_pair_spin = 0; // no newton pair for now => to be corrected
 // newton_pair = 0;

  single_enable = 0;
  exch_flag = 0; 
  exch_mech_flag = 0; 

  no_virial_fdotr_compute = 1;
}

/* ---------------------------------------------------------------------- */

PairSpinExchange::~PairSpinExchange()
{
  if (allocated) {
    memory->destroy(setflag);
    
    memory->destroy(cut_spin_exchange);
    memory->destroy(J1_mag);
    memory->destroy(J1_mech);
    memory->destroy(J2);
    memory->destroy(J3);  

    memory->destroy(cutsq);
  }
}

/* ---------------------------------------------------------------------- */

void PairSpinExchange::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;  
  double evdwl,ecoul;
  double xi[3], rij[3];
  double fi[3], fmi[3];
  double fj[3], fmj[3];
  double cut_ex_2,cut_spin_exchange_global2;
  double rsq,rd,inorm;
  int *ilist,*jlist,*numneigh,**firstneigh;  
  double spi[3],spj[3];

  evdwl = ecoul = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;
  cut_spin_exchange_global2 = cut_spin_exchange_global*cut_spin_exchange_global;
  
  double **x = atom->x;
  double **f = atom->f;
  double **fm = atom->fm;
  double *mumag = atom->mumag;
  double **sp = atom->sp;	
  int *type = atom->type;  
  int nlocal = atom->nlocal;  
  int newton_pair = force->newton_pair;
  
  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  // computation of the exchange interaction
  // loop over atoms and their neighbors

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
      fj[0] = fj[1] = fj[2] = 0.0;
      fmi[0] = fmi[1] = fmi[2] = 0.0;
      fmj[0] = fmj[1] = fmj[2] = 0.0;
     
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

      // compute exchange interaction

      if (exch_flag) {
        cut_ex_2 = cut_spin_exchange[itype][jtype]*cut_spin_exchange[itype][jtype];
        if (rsq <= cut_ex_2) {
          compute_exchange(i,j,rsq,fmi,fmj,spi,spj);   
	  compute_exchange_mech(i,j,rsq,rij,fi,fj,spi,spj);
        }
      }

      f[i][0] += fi[0];	 
      f[i][1] += fi[1];	  	  
      f[i][2] += fi[2];
      fm[i][0] += fmi[0];	 
      fm[i][1] += fmi[1];	  	  
      fm[i][2] += fmi[2];

      // check newton pair  =>  needs correction
//      if (newton_pair || j < nlocal) {
      if (newton_pair_spin) {
	f[j][0] += fj[0];	 
        f[j][1] += fj[1];	  	  
        f[j][2] += fj[2];
        fm[j][0] += fmj[0];	 
        fm[j][1] += fmj[1];	  	  
        fm[j][2] += fmj[2];
      }
 
      if (eflag) {
	if (rsq <= cut_ex_2) {
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

void PairSpinExchange::compute_exchange(int i, int j, double rsq, double fmi[3],  double fmj[3], double spi[3], double spj[3])
{
  int *type = atom->type;  
  int itype, jtype;
  double Jex, ra;
  itype = type[i];
  jtype = type[j];
          
  ra = rsq/J3[itype][jtype]/J3[itype][jtype]; 
  Jex = 4.0*J1_mag[itype][jtype]*ra;
  Jex *= (1.0-J2[itype][jtype]*ra);
  Jex *= exp(-ra);

  fmi[0] -= 0.5*Jex*spj[0];
  fmi[1] -= 0.5*Jex*spj[1];
  fmi[2] -= 0.5*Jex*spj[2];
          
  fmj[0] -= 0.5*Jex*spi[0];
  fmj[1] -= 0.5*Jex*spi[1];
  fmj[2] -= 0.5*Jex*spi[2];

}

/* ---------------------------------------------------------------------- */

void PairSpinExchange::compute_exchange_mech(int i, int j, double rsq, double rij[3], double fi[3],  double fj[3], double *spi, double *spj)
{
  int *type = atom->type;  
  int itype, jtype;
  double Jex, Jex_mech, ra, rr, iJ3;
  itype = type[i];
  jtype = type[j];
  Jex = J1_mech[itype][jtype];        
  iJ3 = 1.0/(J3[itype][jtype]*J3[itype][jtype]);

  ra = rsq*iJ3; 
  rr = sqrt(rsq)*iJ3;

  Jex_mech = 1.0-ra-J2[itype][jtype]*ra*(2.0-ra);
  Jex_mech *= 8.0*Jex*rr*exp(-ra);
  Jex_mech *= (spi[0]*spj[0]+spi[1]*spj[1]+spi[2]*spj[2]); 

  fi[0] += Jex_mech*rij[0];
  fi[1] += Jex_mech*rij[1];
  fi[2] += Jex_mech*rij[2];
          
  fj[0] -= Jex_mech*rij[0];
  fj[1] -= Jex_mech*rij[1];
  fj[2] -= Jex_mech*rij[2];

}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairSpinExchange::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;
      
  memory->create(cut_spin_exchange,n+1,n+1,"pair:cut_spin_exchange");
  memory->create(J1_mag,n+1,n+1,"pair:J1_mag");
  memory->create(J1_mech,n+1,n+1,"pair:J1_mech");
  memory->create(J2,n+1,n+1,"pair:J2");  
  memory->create(J3,n+1,n+1,"pair:J3");
 
  memory->create(cutsq,n+1,n+1,"pair:cutsq");  
  
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairSpinExchange::settings(int narg, char **arg)
{
  if (narg < 1 || narg > 2)
    error->all(FLERR,"Incorrect number of args in pair_style pair/spin command");

  if (strcmp(update->unit_style,"metal") != 0)
    error->all(FLERR,"Spin simulations require metal unit style");
    
  cut_spin_exchange_global = force->numeric(FLERR,arg[0]);
    
  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i+1; j <= atom->ntypes; j++)
        if (setflag[i][j]) {
          cut_spin_exchange[i][j] = cut_spin_exchange_global;
        }
  }
   
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type spin pairs (only one for now)
------------------------------------------------------------------------- */

void PairSpinExchange::coeff(int narg, char **arg)
{
  const double hbar = force->hplanck/MY_2PI;

  if (!allocated) allocate();
  
  // set exch_mech_flag to 1 if magneto-mech simulation

  if (strstr(force->pair_style,"pair/spin")) {
    exch_mech_flag = 0;
  } else if (strstr(force->pair_style,"hybrid/overlay")) {
    exch_mech_flag = 1;
  } else error->all(FLERR,"Incorrect args in pair_style command");


  if (strcmp(arg[2],"exchange")==0){
    if (narg != 7) error->all(FLERR,"Incorrect args in pair_style command");
    exch_flag = 1;    
    
    int ilo,ihi,jlo,jhi;
    force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
    force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);
    
    const double rc = force->numeric(FLERR,arg[3]);
    const double j1 = force->numeric(FLERR,arg[4]);
    const double j2 = force->numeric(FLERR,arg[5]);  
    const double j3 = force->numeric(FLERR,arg[6]); 
  
    int count = 0;
    for (int i = ilo; i <= ihi; i++) {
      for (int j = MAX(jlo,i); j <= jhi; j++) {
        cut_spin_exchange[i][j] = rc;   
        J1_mag[i][j] = j1/hbar;
	if (exch_mech_flag) {
	  J1_mech[i][j] = j1;
	} else {
	  J1_mech[i][j] = 0.0;
	}
        J2[i][j] = j2;
        J3[i][j] = j3;
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

void PairSpinExchange::init_style()
{
  if (!atom->sp_flag || !atom->mumag_flag)
    error->all(FLERR,"Pair spin requires atom attributes sp, mumag");

  neighbor->request(this,instance_me);

  // check this half/full request  =>  needs correction/review
#define FULLNEI
#if defined FULLNEI
  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;
#endif

}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairSpinExchange::init_one(int i, int j)
{
   
   if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  return cut_spin_exchange_global;
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinExchange::write_restart(FILE *fp)
{
  write_restart_settings(fp);
  
  int i,j;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        if (exch_flag){
          fwrite(&J1_mag[i][j],sizeof(double),1,fp);
          fwrite(&J1_mech[i][j],sizeof(double),1,fp);
          fwrite(&J2[i][j],sizeof(double),1,fp);
          fwrite(&J3[i][j],sizeof(double),1,fp);
          fwrite(&cut_spin_exchange[i][j],sizeof(double),1,fp);
        }
      }
    }
  }

}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinExchange::read_restart(FILE *fp)
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
          fread(&J1_mag[i][j],sizeof(double),1,fp);
          fread(&J1_mech[i][j],sizeof(double),1,fp);
          fread(&J2[i][j],sizeof(double),1,fp);
          fread(&J2[i][j],sizeof(double),1,fp);
          fread(&cut_spin_exchange[i][j],sizeof(double),1,fp);
        }
        MPI_Bcast(&J1_mag[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&J1_mech[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&J2[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&J3[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut_spin_exchange[i][j],1,MPI_DOUBLE,0,world);
      }
    }
  }
}

 
/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinExchange::write_restart_settings(FILE *fp)
{
  fwrite(&cut_spin_exchange_global,sizeof(double),1,fp);
  fwrite(&offset_flag,sizeof(int),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinExchange::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&cut_spin_exchange_global,sizeof(double),1,fp);
    fread(&offset_flag,sizeof(int),1,fp);
    fread(&mix_flag,sizeof(int),1,fp);
  }
  MPI_Bcast(&cut_spin_exchange_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&offset_flag,1,MPI_INT,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world); 
}
