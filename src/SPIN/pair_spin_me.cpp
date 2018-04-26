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
   
   Please cite the related publication:
   Tranchida, J., Plimpton, S. J., Thibaudeau, P., & Thompson, A. P. (2018). 
   Massively parallel symplectic algorithm for coupled magnetic spin dynamics 
   and molecular dynamics. arXiv preprint arXiv:1801.10233.
------------------------------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "atom.h"
#include "comm.h"
#include "error.h"
#include "fix.h"
#include "fix_nve_spin.h"
#include "force.h"
#include "pair_hybrid.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "math_const.h"
#include "memory.h"
#include "modify.h"
#include "pair_spin_me.h"
#include "update.h"

using namespace LAMMPS_NS;
using namespace MathConst;

/* ---------------------------------------------------------------------- */

PairSpinMe::PairSpinMe(LAMMPS *lmp) : PairSpin(lmp)
{
  single_enable = 0;
  no_virial_fdotr_compute = 1;
  lattice_flag = 0;
}

/* ---------------------------------------------------------------------- */

PairSpinMe::~PairSpinMe()
{
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cut_spin_me);
    memory->destroy(ME);
    memory->destroy(ME_mech);
    memory->destroy(v_mex);
    memory->destroy(v_mey);
    memory->destroy(v_mez);
    memory->destroy(cutsq); // to be deteled
  }
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairSpinMe::settings(int narg, char **arg)
{
  if (narg < 1 || narg > 2)
    error->all(FLERR,"Incorrect number of args in pair_style pair/spin command");

  if (strcmp(update->unit_style,"metal") != 0)
    error->all(FLERR,"Spin simulations require metal unit style");
    
  cut_spin_me_global = force->numeric(FLERR,arg[0]);
    
  // reset cutoffs that have been explicitly set

  if (allocated) {
    int i,j;
    for (i = 1; i <= atom->ntypes; i++)
      for (j = i+1; j <= atom->ntypes; j++)
        if (setflag[i][j]) {
          cut_spin_me[i][j] = cut_spin_me_global;
        }
  }
   
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type spin pairs (only one for now)
------------------------------------------------------------------------- */

void PairSpinMe::coeff(int narg, char **arg)
{
  const double hbar = force->hplanck/MY_2PI;

  if (!allocated) allocate();

  if (strcmp(arg[2],"me")==0) {
    if (narg != 8) error->all(FLERR,"Incorrect args in pair_style command");
    int ilo,ihi,jlo,jhi;
    force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
    force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);
    
    const double rij = force->numeric(FLERR,arg[3]);
    const double me = (force->numeric(FLERR,arg[4]));
    double mex = force->numeric(FLERR,arg[5]);  
    double mey = force->numeric(FLERR,arg[6]); 
    double mez = force->numeric(FLERR,arg[7]); 

    double inorm = 1.0/(mex*mex+mey*mey+mez*mez);
    mex *= inorm; 
    mey *= inorm; 
    mez *= inorm; 
 
    int count = 0;
    for (int i = ilo; i <= ihi; i++) {
      for (int j = MAX(jlo,i); j <= jhi; j++) {
        cut_spin_me[i][j] = rij;
        ME[i][j] = me/hbar;
        ME_mech[i][j] = me;
        v_mex[i][j] = mex;
        v_mey[i][j] = mey;
        v_mez[i][j] = mez;
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

void PairSpinMe::init_style()
{
  if (!atom->sp_flag)
    error->all(FLERR,"Pair spin requires atom/spin style");

  // need a full neighbor list
  
  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;

  // checking if nve/spin is a listed fix

  int ifix = 0;
  while (ifix < modify->nfix) {
    if (strcmp(modify->fix[ifix]->style,"nve/spin") == 0) break;
    ifix++;
  }
  if (ifix == modify->nfix)
    error->all(FLERR,"pair/spin style requires nve/spin");
  
  // get the lattice_flag from nve/spin

  for (int i = 0; i < modify->nfix; i++) {
    if (strcmp(modify->fix[i]->style,"nve/spin") == 0) {
      lockfixnvespin = (FixNVESpin *) modify->fix[i];
      lattice_flag = lockfixnvespin->lattice_flag;
    }
  }

}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairSpinMe::init_one(int i, int j)
{
   
   if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  return cut_spin_me_global;
}

/* ----------------------------------------------------------------------
   extract the larger cutoff
------------------------------------------------------------------------- */

void *PairSpinMe::extract(const char *str, int &dim)
{
  dim = 0;
  if (strcmp(str,"cut") == 0) return (void *) &cut_spin_me_global;
  return NULL;
}


/* ---------------------------------------------------------------------- */

void PairSpinMe::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum,itype,jtype;  
  double evdwl, ecoul;
  double xi[3], rij[3], eij[3];
  double spi[3], spj[3];
  double fi[3], fj[3];
  double fmi[3], fmj[3];
  double local_cut2;
  double rsq, inorm;
  int *ilist,*jlist,*numneigh,**firstneigh;  

  evdwl = ecoul = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;
  
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

  // magneto-electric computation
  // loop over atoms and their neighbors

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    itype = type[i];

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
      rij[0] = rij[1] = rij[2] = 0.0;
     
      rij[0] = x[j][0] - xi[0];
      rij[1] = x[j][1] - xi[1];
      rij[2] = x[j][2] - xi[2];
      rsq = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2]; 
      inorm = 1.0/sqrt(rsq);
      eij[0] *= inorm;
      eij[1] *= inorm;
      eij[2] *= inorm;

      jtype = type[j];

      local_cut2 = cut_spin_me[itype][jtype]*cut_spin_me[itype][jtype];

      // compute me interaction

      if (rsq <= local_cut2) {
	compute_me(i,j,rsq,eij,fmi,spi,spj);
	if (lattice_flag) {
	  compute_me_mech(i,j,fi,spi,spj);
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
	f[j][0] -= fj[0];	 
        f[j][1] -= fj[1];	  	  
        f[j][2] -= fj[2];
      }
 
      if (eflag) {
	evdwl -= (spi[0]*fmi[0] + spi[1]*fmi[1] + spi[2]*fmi[2]);
	evdwl *= hbar;
      } else evdwl = 0.0;

      if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
	  evdwl,ecoul,fi[0],fi[1],fi[2],rij[0],rij[1],rij[2]);
    }
  }  

  if (vflag_fdotr) virial_fdotr_compute();
  
}

/* ---------------------------------------------------------------------- */

void PairSpinMe::compute_single_pair(int ii, double fmi[3]) 
{

  const int nlocal = atom->nlocal;
  int *type = atom->type;
  double **x = atom->x;
  double **sp = atom->sp;
  double local_cut2;

  double xi[3], rij[3], eij[3];
  double spi[3], spj[3];

  int iexchange, idmi, ineel, ime;
  int i,j,jj,inum,jnum,itype,jtype;
  int *ilist,*jlist,*numneigh,**firstneigh;

  double rsq, inorm;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  i = ilist[ii];
  itype = type[i];

  spi[0] = sp[i][0];
  spi[1] = sp[i][1];
  spi[2] = sp[i][2];
 
  xi[0] = x[i][0];
  xi[1] = x[i][1];
  xi[2] = x[i][2];

  eij[0] = eij[1] = eij[2] = 0.0;
 
  jlist = firstneigh[i];
  jnum = numneigh[i];

  for (int jj = 0; jj < jnum; jj++) {

    j = jlist[jj];
    j &= NEIGHMASK;
    jtype = type[j];
    local_cut2 = cut_spin_me[itype][jtype]*cut_spin_me[itype][jtype];

    spj[0] = sp[j][0];
    spj[1] = sp[j][1];
    spj[2] = sp[j][2];

    rij[0] = x[j][0] - xi[0];
    rij[1] = x[j][1] - xi[1];
    rij[2] = x[j][2] - xi[2];
    rsq = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2];
    inorm = 1.0/sqrt(rsq);
    eij[0] = inorm*rij[0];
    eij[1] = inorm*rij[1];
    eij[2] = inorm*rij[2];

    if (rsq <= local_cut2) {
      compute_me(i,j,rsq,eij,fmi,spi,spj);
    }
  }

}

/* ---------------------------------------------------------------------- */

void PairSpinMe::compute_me(int i, int j, double rsq, double eij[3], double fmi[3], double spi[3], double spj[3]) 
{
  int *type = atom->type;  
  int itype, jtype;
  itype = type[i];
  jtype = type[j];
  
  double local_cut2 = cut_spin_me[itype][jtype]*cut_spin_me[itype][jtype]; 
 
  if (rsq <= local_cut2) {
    double meix,meiy,meiz;
    double rx, ry, rz;
    double vx, vy, vz;
  
    rx = eij[0];
    ry = eij[1]; 
    rz = eij[2]; 
  
    vx = v_mex[itype][jtype];
    vy = v_mey[itype][jtype];
    vz = v_mez[itype][jtype];
    
    meix = vy*rz - vz*ry; 
    meiy = vz*rx - vx*rz; 
    meiz = vx*ry - vy*rx; 
  
    meix *= ME[itype][jtype]; 
    meiy *= ME[itype][jtype]; 
    meiz *= ME[itype][jtype]; 
  
    fmi[0] += spj[1]*meiz - spj[2]*meiy;
    fmi[1] += spj[2]*meix - spj[0]*meiz;
    fmi[2] += spj[0]*meiy - spj[1]*meix;
  }
}

/* ---------------------------------------------------------------------- */

void PairSpinMe::compute_me_mech(int i, int j, double fi[3], double spi[3], double spj[3])
{
  int *type = atom->type;  
  int itype, jtype;
  itype = type[i];
  jtype = type[j];

  double meix,meiy,meiz;
  double vx, vy, vz;

  vx = v_mex[itype][jtype];
  vy = v_mey[itype][jtype];
  vz = v_mez[itype][jtype];

  meix = spi[1]*spi[2] - spi[2]*spj[1];
  meiy = spi[2]*spi[0] - spi[0]*spj[2];
  meiz = spi[0]*spi[1] - spi[1]*spj[0];

  meix *= ME_mech[itype][jtype]; 
  meiy *= ME_mech[itype][jtype]; 
  meiz *= ME_mech[itype][jtype]; 

  fi[0] = meiy*vz - meiz*vy;
  fi[1] = meiz*vx - meix*vz;
  fi[2] = meix*vy - meiy*vx;

}
  
/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairSpinMe::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;
 
  memory->create(cut_spin_me,n+1,n+1,"pair/spin/me:cut_spin_me");
  memory->create(ME,n+1,n+1,"pair/spin/me:ME");
  memory->create(ME_mech,n+1,n+1,"pair/spin/me:ME_mech");
  memory->create(v_mex,n+1,n+1,"pair/spin/me:ME_vector_x");
  memory->create(v_mey,n+1,n+1,"pair/spin/me:ME_vector_y");
  memory->create(v_mez,n+1,n+1,"pair/spin/me:ME_vector_z");
  memory->create(cutsq,n+1,n+1,"pair:cutsq");  
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinMe::write_restart(FILE *fp)
{
  write_restart_settings(fp);
  
  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) {
        fwrite(&ME[i][j],sizeof(double),1,fp);
        fwrite(&v_mex[i][j],sizeof(double),1,fp);
        fwrite(&v_mey[i][j],sizeof(double),1,fp);
        fwrite(&v_mez[i][j],sizeof(double),1,fp);
        fwrite(&cut_spin_me[i][j],sizeof(double),1,fp);
      }
    }
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinMe::read_restart(FILE *fp)
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
          fread(&ME[i][j],sizeof(double),1,fp);
          fread(&v_mex[i][j],sizeof(double),1,fp);
          fread(&v_mey[i][j],sizeof(double),1,fp);
          fread(&v_mez[i][j],sizeof(double),1,fp);
          fread(&cut_spin_me[i][j],sizeof(double),1,fp);
        }
        MPI_Bcast(&ME[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_mex[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_mey[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&v_mez[i][j],1,MPI_DOUBLE,0,world);
        MPI_Bcast(&cut_spin_me[i][j],1,MPI_DOUBLE,0,world);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairSpinMe::write_restart_settings(FILE *fp)
{
  fwrite(&cut_spin_me_global,sizeof(double),1,fp);
  fwrite(&offset_flag,sizeof(int),1,fp);
  fwrite(&mix_flag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairSpinMe::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&cut_spin_me_global,sizeof(double),1,fp);
    fread(&offset_flag,sizeof(int),1,fp);
    fread(&mix_flag,sizeof(int),1,fp);
  }
  MPI_Bcast(&cut_spin_me_global,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&offset_flag,1,MPI_INT,0,world);
  MPI_Bcast(&mix_flag,1,MPI_INT,0,world); 
}
