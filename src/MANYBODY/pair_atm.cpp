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
   Contributing author: Sergey Lishchuk
------------------------------------------------------------------------- */

#include "pair_atm.h"

#include <math.h>

#include "atom.h"
#include "citeme.h"
#include "comm.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "neighbor.h"

using namespace LAMMPS_NS;

static const char cite_atm_package[] =
  "ATM package:\n\n"
  "@Article{Lishchuk:2012:164501,\n"
  " author = {S. V. Lishchuk},\n"
  " title = {Role of three-body interactions in formation of bulk viscosity in liquid argon},\n"
  " journal = {J.~Chem.~Phys.},\n"
  " year =    2012,\n"
  " volume =  136,\n"
  " pages =   {164501}\n"
  "}\n\n";

/* ---------------------------------------------------------------------- */

PairATM::PairATM(LAMMPS *lmp) : Pair(lmp)
{
  if (lmp->citeme) lmp->citeme->add(cite_atm_package);

  single_enable = 0;
  restartinfo = 1;
  one_coeff = 0;
  manybody_flag = 1;
}

/* ----------------------------------------------------------------------
   check if allocated, since class can be destructed when incomplete
------------------------------------------------------------------------- */

PairATM::~PairATM()
{
  if (copymode) return;

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);

    memory->destroy(nu);
  }
}

/* ----------------------------------------------------------------------
   workhorse routine that computes pairwise interactions
------------------------------------------------------------------------- */

void PairATM::compute(int eflag, int vflag)
{
  int i,j,k,ii,jj,kk,inum,jnum,jnumm1;
  double xi,yi,zi,evdwl;
  double rij2,rik2,rjk2,r6;
  double rij[3],rik[3],rjk[3],fj[3],fk[3];
  double nu_local;
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;

  double cutoff_sixth = cut_global*cut_global;
  cutoff_sixth = cutoff_sixth*cutoff_sixth*cutoff_sixth;
  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xi = x[i][0];
    yi = x[i][1];
    zi = x[i][2];

    jlist = firstneigh[i];
    jnum = numneigh[i];
    jnumm1 = jnum - 1;

    for (jj = 0; jj < jnumm1; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;
      rij[0] = x[j][0] - xi;
      if (rij[0] < 0.0) continue;
      rij[1] = x[j][1] - yi;
      if (rij[0] == 0.0 and rij[1] < 0.0) continue;
      rij[2] = x[j][2] - zi;
      if (rij[0] == 0.0 and rij[1] == 0.0 and rij[2] < 0.0) continue;
      rij2 = rij[0]*rij[0] + rij[1]*rij[1] + rij[2]*rij[2];

      for (kk = jj+1; kk < jnum; kk++) {
        k = jlist[kk];
        k &= NEIGHMASK;

        rjk[0] = x[k][0] - x[j][0];
        if (rjk[0] < 0.0) continue;
        rjk[1] = x[k][1] - x[j][1];
        if (rjk[0] == 0.0 and rjk[1] < 0.0) continue;
        rjk[2] = x[k][2] - x[j][2];
        if (rjk[0] == 0.0 and rjk[1] == 0.0 and rjk[2] < 0.0) continue;
        rjk2 = rjk[0]*rjk[0] + rjk[1]*rjk[1] + rjk[2]*rjk[2];

        rik[0] = x[k][0] - xi;
        rik[1] = x[k][1] - yi;
        rik[2] = x[k][2] - zi;
        rik2 = rik[0]*rik[0] + rik[1]*rik[1] + rik[2]*rik[2];

        r6 = rij2*rik2*rjk2;
        if (r6 > cutoff_sixth) continue;

        nu_local = nu[type[i]][type[j]][type[k]];
        if (nu_local == 0.0) continue;

        interaction_ddd(nu_local,
                        r6,rij2,rik2,rjk2,rij,rik,rjk,fj,fk,eflag,evdwl);

        f[i][0] -= fj[0] + fk[0];
        f[i][1] -= fj[1] + fk[1];
        f[i][2] -= fj[2] + fk[2];
        f[j][0] += fj[0];
        f[j][1] += fj[1];
        f[j][2] += fj[2];
        f[k][0] += fk[0];
        f[k][1] += fk[1];
        f[k][2] += fk[2];

        if (evflag) ev_tally3(i,j,k,evdwl,0.0,fj,fk,rij,rik);
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ---------------------------------------------------------------------- */

void PairATM::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  memory->create(nu,n+1,n+1,n+1,"pair:nu");

  // initialize all nu values to 0.0

  for (int i = 1; i <= n; i++)
    for (int j = 1; j <= n; j++)
      for (int k = 1; k <= n; k++)
        nu[i][j][k] = 0.0;
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairATM::settings(int narg, char **arg)
{
  if (narg != 1) error->all(FLERR,"Illegal pair_style command");

  cut_global = force->numeric(FLERR,arg[0]);
}

/* ----------------------------------------------------------------------
   set coefficients for one I,J,K type triplet
------------------------------------------------------------------------- */

void PairATM::coeff(int narg, char **arg)
{
  if (narg != 4) error->all(FLERR,"Incorrect args for pair coefficients");
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi,klo,khi;
  force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
  force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);
  force->bounds(FLERR,arg[2],atom->ntypes,klo,khi);

  double nu_one = force->numeric(FLERR,arg[3]);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j<=jhi; j++) {
      for (int k = MAX(klo,j); k<=khi; k++) {
        nu[i][j][k] = nu_one;
        count++;
      }
      setflag[i][j] = 1;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients");
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairATM::init_style()
{
  if (force->newton_pair == 0)
    error->all(FLERR,"Pair style ATM requires newton pair on");

  // need a full neighbor list

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;
}

/* ----------------------------------------------------------------------
   init for one i,j type pair and corresponding j,i
   also for all k type permutations
------------------------------------------------------------------------- */

double PairATM::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  // set all 6 symmetric permutations of I,J,K types to same nu value

  int ntypes = atom->ntypes;
  for (int k = j; k <= ntypes; k++)
    nu[i][k][j] = nu[j][i][k] = nu[j][k][i] = nu[k][i][j] = nu[k][j][i] = 
      nu[i][j][k];

  return cut_global;
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

// 
// PLEASE HELP!
// 
// From the pair_hybrid documentation: 
// 
// "For the hybrid pair styles, the list of sub-styles and their respective
// settings are written to binary restart files, so a pair_style command
// does not need to specified in an input script that reads a restart file.
// However, the coefficient information is not stored in the restart file.
// Thus, pair_coeff commands need to be re-specified in the restart input
// script."
// 
// As a result, the function "PairATM::write_restart" is not called from
// "pair_hybrid.cpp". What is the best way to deal with this problem?
// 
void PairATM::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j,k;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      fwrite(&setflag[i][j],sizeof(int),1,fp);
      if (setflag[i][j]) 
        for (k = j; k <= atom->ntypes; k++) 
          fwrite(&nu[i][j][k],sizeof(double),1,fp);
    }
  }
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairATM::read_restart(FILE *fp)
{
  read_restart_settings(fp);
  allocate();

  int i,j,k;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++) {
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
      if (setflag[i][j]) for (k = j; k <= atom->ntypes; k++) {
        if (me == 0) fread(&nu[i][j][k],sizeof(double),1,fp);
        MPI_Bcast(&nu[i][j][k],1,MPI_DOUBLE,0,world);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairATM::write_restart_settings(FILE *fp)
{
  fwrite(&cut_global,sizeof(double),1,fp);
}

/* ----------------------------------------------------------------------
   proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairATM::read_restart_settings(FILE *fp)
{
  int me = comm->me;
  if (me == 0) fread(&cut_global,sizeof(double),1,fp);
  MPI_Bcast(&cut_global,1,MPI_DOUBLE,0,world);
}

/* ----------------------------------------------------------------------
   Axilrod-Teller-Muto (dipole-dipole-dipole) potential
------------------------------------------------------------------------- */

void PairATM::interaction_ddd(double nu,
                              double r6, double rij2, double rik2, double rjk2,
                              double *rij, double *rik, double *rjk,
                              double *fj, double *fk, int eflag, double &eng)
{
  double r5inv,rri,rrj,rrk,rrr;
  r5inv = nu / (r6*r6*sqrt(r6));
  rri = rik[0]*rij[0] + rik[1]*rij[1] + rik[2]*rij[2];
  rrj = rij[0]*rjk[0] + rij[1]*rjk[1] + rij[2]*rjk[2];
  rrk = rjk[0]*rik[0] + rjk[1]*rik[1] + rjk[2]*rik[2];
  rrr = 5.0*rri*rrj*rrk;
  for (int i = 0; i < 3; i++) {
    fj[i] = rrj*(rrk - rri)*rik[i] - 
      (rrk*rri - rjk2*rik2 + rrr/rij2) * rij[i] + 
      (rrk*rri - rik2*rij2 + rrr/rjk2) * rjk[i];
    fj[i] *= 3.0*r5inv;
    fk[i] = rrk*(rri + rrj)*rij[i] + 
      (rri*rrj + rik2*rij2 - rrr/rjk2) * rjk[i] + 
      (rri*rrj + rij2*rjk2 - rrr/rik2) * rik[i];
    fk[i] *= 3.0*r5inv;
  }
  if (eflag) eng = (r6 - 0.6*rrr)*r5inv;
}
