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

#include <string.h>
#include "npair_half_size_bin_newtoff_omp.h"
#include "npair_omp.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "atom.h"
#include "atom_vec.h"
#include "molecule.h"
#include "domain.h"
#include "fix_shear_history.h"
#include "my_page.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NPairHalfSizeBinNewtoffOmp::NPairHalfSizeBinNewtoffOmp(LAMMPS *lmp) : 
  NPair(lmp) {}

/* ----------------------------------------------------------------------
   size particles
   binned neighbor list construction with partial Newton's 3rd law
   shear history must be accounted for when a neighbor pair is added
   each owned atom i checks own bin and surrounding bins in non-Newton stencil
   pair stored once if i,j are both owned and i < j
   pair stored by me if j is ghost (also stored by proc owning j)
------------------------------------------------------------------------- */

void NPairHalfSizeBinNewtoffOmp::build(NeighList *list)
{
  const int nlocal = (includegroup) ? atom->nfirst : atom->nlocal;

  FixShearHistory * const fix_history = (FixShearHistory *) list->fix_history;
  NeighList * listhistory = list->listhistory;

  NPAIR_OMP_INIT;

#if defined(_OPENMP)
#pragma omp parallel default(none) shared(list,listhistory)
#endif
  NPAIR_OMP_SETUP(nlocal);

  int i,j,k,m,n,nn,ibin,dnum,dnumbytes;
  double xtmp,ytmp,ztmp,delx,dely,delz,rsq;
  double radi,radsum,cutsq;
  int *neighptr,*touchptr;
  double *shearptr;
  MyPage<int> *ipage_touch;
  MyPage<double> *dpage_shear;

  int *npartner;
  tagint **partner;
  double **shearpartner;
  int **firsttouch;
  double **firstshear;

  // loop over each atom, storing neighbors

  double **x = atom->x;
  double *radius = atom->radius;
  tagint *tag = atom->tag;
  int *type = atom->type;
  int *mask = atom->mask;
  tagint *molecule = atom->molecule;

  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;

  // each thread has its own page allocator
  MyPage<int> &ipage = list->ipage[tid];
  ipage.reset();

  if (fix_history) {
    npartner = fix_history->npartner;
    partner = fix_history->partner;
    shearpartner = fix_history->shearpartner;
    firsttouch = listhistory->firstneigh;
    firstshear = listhistory->firstdouble;
    ipage_touch = listhistory->ipage+tid;
    dpage_shear = listhistory->dpage+tid;
    dnum = listhistory->dnum;
    dnumbytes = dnum * sizeof(double);
    ipage_touch->reset();
    dpage_shear->reset();
  }

  for (i = ifrom; i < ito; i++) {

    n = 0;
    neighptr = ipage.vget();
    if (fix_history) {
      nn = 0;
      touchptr = ipage_touch->vget();
      shearptr = dpage_shear->vget();
    }

    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    radi = radius[i];
    ibin = atom2bin[i];

    // loop over all atoms in surrounding bins in stencil including self
    // only store pair if i < j
    // stores own/own pairs only once
    // stores own/ghost pairs on both procs

    for (k = 0; k < nstencil; k++) {
      for (j = binhead[ibin+stencil[k]]; j >= 0; j = bins[j]) {
        if (j <= i) continue;
        if (exclude && exclusion(i,j,type[i],type[j],mask,molecule)) continue;

        delx = xtmp - x[j][0];
        dely = ytmp - x[j][1];
        delz = ztmp - x[j][2];
        rsq = delx*delx + dely*dely + delz*delz;
        radsum = radi + radius[j];
        cutsq = (radsum+skin) * (radsum+skin);

        if (rsq <= cutsq) {
          neighptr[n] = j;

          if (fix_history) {
            if (rsq < radsum*radsum) {
              for (m = 0; m < npartner[i]; m++)
                if (partner[i][m] == tag[j]) break;
              if (m < npartner[i]) {
                touchptr[n] = 1;
                memcpy(&shearptr[nn],&shearpartner[i][dnum*m],dnumbytes);
                nn += dnum;
              } else {
                touchptr[n] = 0;
                memcpy(&shearptr[nn],zeroes,dnumbytes);
                nn += dnum;
              }
            } else {
              touchptr[n] = 0;
              memcpy(&shearptr[nn],zeroes,dnumbytes);
              nn += dnum;
            }
          }

          n++;
        }
      }
    }

    ilist[i] = i;
    firstneigh[i] = neighptr;
    numneigh[i] = n;
    ipage.vgot(n);
    if (ipage.status())
      error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");

    if (fix_history) {
      firsttouch[i] = touchptr;
      firstshear[i] = shearptr;
      ipage_touch->vgot(n);
      dpage_shear->vgot(nn);
    }
  }
  NPAIR_OMP_CLOSE;
  list->inum = nlocal;
}
