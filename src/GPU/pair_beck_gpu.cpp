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

/* ----------------------------------------------------------------------
   Contributing author: Trung Dac Nguyen (ORNL)
------------------------------------------------------------------------- */

#include "pair_beck_gpu.h"
#include <cmath>
#include <cstdio>

#include <cstring>
#include "atom.h"
#include "atom_vec.h"
#include "comm.h"
#include "force.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "integrate.h"
#include "memory.h"
#include "error.h"
#include "neigh_request.h"
#include "universe.h"
#include "update.h"
#include "domain.h"
#include "gpu_extra.h"
#include "math_special.h"
#include "suffix.h"

using namespace LAMMPS_NS;
using namespace MathSpecial;

// External functions from cuda library for atom decomposition

int beck_gpu_init(const int ntypes, double **cutsq, double **host_aa,
                  double **alpha, double **beta, double **AA, double **BB,
                  double *special_lj, const int nlocal,
                  const int nall, const int max_nbors, const int maxspecial,
                  const double cell_size, int &gpu_mode, FILE *screen);
void beck_gpu_clear();
int ** beck_gpu_compute_n(const int ago, const int inum,
                          const int nall, double **host_x, int *host_type,
                          double *sublo, double *subhi, tagint *tag, int **nspecial,
                          tagint **special, const bool eflag, const bool vflag,
                          const bool eatom, const bool vatom, int &host_start,
                          int **ilist, int **jnum,
                          const double cpu_time, bool &success);
void beck_gpu_compute(const int ago, const int inum, const int nall,
                      double **host_x, int *host_type, int *ilist, int *numj,
                      int **firstneigh, const bool eflag, const bool vflag,
                      const bool eatom, const bool vatom, int &host_start,
                      const double cpu_time, bool &success);
double beck_gpu_bytes();

/* ---------------------------------------------------------------------- */

PairBeckGPU::PairBeckGPU(LAMMPS *lmp) : PairBeck(lmp), gpu_mode(GPU_FORCE)
{
  respa_enable = 0;
  reinitflag = 0;
  cpu_time = 0.0;
  suffix_flag |= Suffix::GPU;
  GPU_EXTRA::gpu_ready(lmp->modify, lmp->error);
}

/* ----------------------------------------------------------------------
   free all arrays
------------------------------------------------------------------------- */

PairBeckGPU::~PairBeckGPU()
{
  beck_gpu_clear();
}

/* ---------------------------------------------------------------------- */

void PairBeckGPU::compute(int eflag, int vflag)
{
  ev_init(eflag,vflag);

  int nall = atom->nlocal + atom->nghost;
  int inum, host_start;

  bool success = true;
  int *ilist, *numneigh, **firstneigh;
  if (gpu_mode != GPU_FORCE) {
    double sublo[3],subhi[3];
    if (domain->triclinic == 0) {
      sublo[0] = domain->sublo[0];
      sublo[1] = domain->sublo[1];
      sublo[2] = domain->sublo[2];
      subhi[0] = domain->subhi[0];
      subhi[1] = domain->subhi[1];
      subhi[2] = domain->subhi[2];
    } else {
      domain->bbox(domain->sublo_lamda,domain->subhi_lamda,sublo,subhi);
    }
    inum = atom->nlocal;
    firstneigh = beck_gpu_compute_n(neighbor->ago, inum, nall,
                                     atom->x, atom->type, sublo,
                                     subhi, atom->tag, atom->nspecial,
                                     atom->special, eflag, vflag, eflag_atom,
                                     vflag_atom, host_start,
                                     &ilist, &numneigh, cpu_time, success);
  } else {
    inum = list->inum;
    ilist = list->ilist;
    numneigh = list->numneigh;
    firstneigh = list->firstneigh;
    beck_gpu_compute(neighbor->ago, inum, nall, atom->x, atom->type,
                      ilist, numneigh, firstneigh, eflag, vflag, eflag_atom,
                      vflag_atom, host_start, cpu_time, success);
  }
  if (!success)
    error->one(FLERR,"Insufficient memory on accelerator");

  if (host_start<inum) {
    cpu_time = MPI_Wtime();
    cpu_compute(host_start, inum, eflag, vflag, ilist, numneigh, firstneigh);
    cpu_time = MPI_Wtime() - cpu_time;
  }
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairBeckGPU::init_style()
{
  if (force->newton_pair)
    error->all(FLERR,"Cannot use newton pair with beck/gpu pair style");

  // Repeat cutsq calculation because done after call to init_style
  double maxcut = -1.0;
  double cut;
  for (int i = 1; i <= atom->ntypes; i++) {
    for (int j = i; j <= atom->ntypes; j++) {
      if (setflag[i][j] != 0 || (setflag[i][i] != 0 && setflag[j][j] != 0)) {
        cut = init_one(i,j);
        cut *= cut;
        if (cut > maxcut)
          maxcut = cut;
        cutsq[i][j] = cutsq[j][i] = cut;
      } else
        cutsq[i][j] = cutsq[j][i] = 0.0;
    }
  }
  double cell_size = sqrt(maxcut) + neighbor->skin;

  int maxspecial=0;
  if (atom->molecular)
    maxspecial=atom->maxspecial;
  int success = beck_gpu_init(atom->ntypes+1, cutsq, aa, alpha, beta,
                              AA, BB, force->special_lj, atom->nlocal,
                              atom->nlocal+atom->nghost, 300, maxspecial,
                              cell_size, gpu_mode, screen);
  GPU_EXTRA::check_flag(success,error,world);

  if (gpu_mode == GPU_FORCE) {
    int irequest = neighbor->request(this,instance_me);
    neighbor->requests[irequest]->half = 0;
    neighbor->requests[irequest]->full = 1;
  }
}

/* ---------------------------------------------------------------------- */

double PairBeckGPU::memory_usage()
{
  double bytes = Pair::memory_usage();
  return bytes + beck_gpu_bytes();
}

/* ---------------------------------------------------------------------- */

void PairBeckGPU::cpu_compute(int start, int inum, int eflag,
                              int /* vflag */, int *ilist,
                              int *numneigh, int **firstneigh) {
  int i,j,ii,jj,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair;
  double rsq,r5,force_beck,factor_lj;
  double r,rinv;
  double aaij,alphaij,betaij;
  double term1,term1inv,term2,term3,term4,term5,term6;
  int *jlist;

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;
  double *special_lj = force->special_lj;

  // loop over neighbors of my atoms

  for (ii = start; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    itype = type[i];
    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      factor_lj = special_lj[sbmask(j)];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;
      jtype = type[j];

      if (rsq < cutsq[itype][jtype]) {
        r = sqrt(rsq);
        r5 = rsq*rsq*r;
        aaij = aa[itype][jtype];
        alphaij = alpha[itype][jtype];
        betaij = beta[itype][jtype];
        term1 = aaij*aaij + rsq;
        term2 = powint(term1,-5);
        term3 = 21.672 + 30.0*aaij*aaij + 6.0*rsq;
        term4 = alphaij + r5*betaij;
        term5 = alphaij + 6.0*r5*betaij;
        rinv  = 1.0/r;
        force_beck = AA[itype][jtype]*exp(-1.0*r*term4)*term5;
        force_beck -= BB[itype][jtype]*r*term2*term3;

        fpair = factor_lj*force_beck*rinv;

        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;

        if (eflag) {
          term6 = powint(term1,-3);
          term1inv = 1.0/term1;
          evdwl = AA[itype][jtype]*exp(-1.0*r*term4);
          evdwl -= BB[itype][jtype]*term6*(1.0+(2.709+3.0*aaij*aaij)*term1inv);
          evdwl *= factor_lj;
        }

        if (evflag) ev_tally_full(i,evdwl,0.0,fpair,delx,dely,delz);
      }
    }
  }
}
