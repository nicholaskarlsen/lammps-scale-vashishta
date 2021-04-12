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
   Contributing author: Mike Brown (SNL)
------------------------------------------------------------------------- */

#include "pair_lj_cut_gpu.h"
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
#include "suffix.h"

using namespace LAMMPS_NS;

// External functions from cuda library for atom decomposition

int ljl_gpu_init(const int ntypes, double **cutsq, double **host_lj1,
                 double **host_lj2, double **host_lj3, double **host_lj4,
                 double **offset, double *special_lj, const int nlocal,
                 const int nall, const int max_nbors, const int maxspecial,
                 const double cell_size, int &gpu_mode, FILE *screen);

void ljl_gpu_reinit(const int ntypes, double **cutsq, double **host_lj1,
                    double **host_lj2, double **host_lj3, double **host_lj4,
                    double **offset);

void ljl_gpu_clear();
int ** ljl_gpu_compute_n(const int ago, const int inum, const int nall,
                         double **host_x, int *host_type, double *sublo,
                         double *subhi, tagint *tag, int **nspecial,
                         tagint **special, const bool eflag, const bool vflag,
                         const bool eatom, const bool vatom, int &host_start,
                         int **ilist, int **jnum,
                         const double cpu_time, bool &success);
void ljl_gpu_compute(const int ago, const int inum, const int nall,
                     double **host_x, int *host_type, int *ilist, int *numj,
                     int **firstneigh, const bool eflag, const bool vflag,
                     const bool eatom, const bool vatom, int &host_start,
                     const double cpu_time, bool &success);
double ljl_gpu_bytes();

/* ---------------------------------------------------------------------- */

PairLJCutGPU::PairLJCutGPU(LAMMPS *lmp) : PairLJCut(lmp), gpu_mode(GPU_FORCE)
{
  respa_enable = 0;
  cpu_time = 0.0;
  suffix_flag |= Suffix::GPU;
  GPU_EXTRA::gpu_ready(lmp->modify, lmp->error);
}

/* ----------------------------------------------------------------------
   free all arrays
------------------------------------------------------------------------- */

PairLJCutGPU::~PairLJCutGPU()
{
  ljl_gpu_clear();
}

/* ---------------------------------------------------------------------- */

void PairLJCutGPU::compute(int eflag, int vflag)
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
    firstneigh = ljl_gpu_compute_n(neighbor->ago, inum, nall,
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
    ljl_gpu_compute(neighbor->ago, inum, nall, atom->x, atom->type,
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

void PairLJCutGPU::init_style()
{
  cut_respa = nullptr;

  if (force->newton_pair)
    error->all(FLERR,"Cannot use newton pair with lj/cut/gpu pair style");

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
  if (atom->molecular != Atom::ATOMIC)
    maxspecial=atom->maxspecial;
  int mnf = 5e-2 * neighbor->oneatom;
  int success = ljl_gpu_init(atom->ntypes+1, cutsq, lj1, lj2, lj3, lj4,
                             offset, force->special_lj, atom->nlocal,
                             atom->nlocal+atom->nghost, mnf, maxspecial,
                             cell_size, gpu_mode, screen);
  GPU_EXTRA::check_flag(success,error,world);

  if (gpu_mode == GPU_FORCE) {
    int irequest = neighbor->request(this,instance_me);
    neighbor->requests[irequest]->half = 0;
    neighbor->requests[irequest]->full = 1;
  }
}

/* ---------------------------------------------------------------------- */

void PairLJCutGPU::reinit()
{
  Pair::reinit();

  ljl_gpu_reinit(atom->ntypes+1, cutsq, lj1, lj2, lj3, lj4, offset);
}

/* ---------------------------------------------------------------------- */

double PairLJCutGPU::memory_usage()
{
  double bytes = Pair::memory_usage();
  return bytes + ljl_gpu_bytes();
}

/* ---------------------------------------------------------------------- */

void PairLJCutGPU::cpu_compute(int start, int inum, int eflag, int /* vflag */,
                               int *ilist, int *numneigh, int **firstneigh) {
  int i,j,ii,jj,jnum,itype,jtype;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair;
  double rsq,r2inv,r6inv,forcelj,factor_lj;
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
        r2inv = 1.0/rsq;
        r6inv = r2inv*r2inv*r2inv;
        forcelj = r6inv * (lj1[itype][jtype]*r6inv - lj2[itype][jtype]);
        fpair = factor_lj*forcelj*r2inv;

        f[i][0] += delx*fpair;
        f[i][1] += dely*fpair;
        f[i][2] += delz*fpair;

        if (eflag) {
          evdwl = r6inv*(lj3[itype][jtype]*r6inv-lj4[itype][jtype]) -
            offset[itype][jtype];
          evdwl *= factor_lj;
        }

        if (evflag) ev_tally_full(i,evdwl,0.0,fpair,delx,dely,delz);
      }
    }
  }
}
