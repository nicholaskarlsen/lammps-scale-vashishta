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

#include "npair_half_respa_bin_newton_omp.h"
#include "neighbor.h"
#include "npair_omp.h"
#include "neigh_list.h"
#include "atom.h"
#include "atom_vec.h"
#include "molecule.h"
#include "domain.h"
#include "my_page.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

NPairHalfRespaBinNewtonOmp::NPairHalfRespaBinNewtonOmp(LAMMPS *lmp) : 
  NPair(lmp) {}

/* ----------------------------------------------------------------------
   multiple respa lists
   binned neighbor list construction with full Newton's 3rd law
   each owned atom i checks its own bin and other bins in Newton stencil
   every pair stored exactly once by some processor
------------------------------------------------------------------------- */

void NPairHalfRespaBinNewtonOmp::build(NeighList *list)
{
  const int nlocal = (includegroup) ? atom->nfirst : atom->nlocal;
  const int molecular = atom->molecular;
  const int moltemplate = (molecular == 2) ? 1 : 0;

  NPAIR_OMP_INIT;

  NeighList *listinner = list->listinner;
  NeighList *listmiddle = list->listmiddle;
  const int respamiddle = list->respamiddle;

#if defined(_OPENMP)
#pragma omp parallel default(none) shared(list,listinner,listmiddle)
#endif
  NPAIR_OMP_SETUP(nlocal);

  int i,j,k,n,itype,jtype,ibin,n_inner,n_middle,imol,iatom;
  tagint tagprev;
  double xtmp,ytmp,ztmp,delx,dely,delz,rsq;
  int *neighptr,*neighptr_inner,*neighptr_middle;

  // loop over each atom, storing neighbors

  double **x = atom->x;
  int *type = atom->type;
  int *mask = atom->mask;
  tagint *tag = atom->tag;
  tagint *molecule = atom->molecule;
  tagint **special = atom->special;
  int **nspecial = atom->nspecial;

  int *molindex = atom->molindex;
  int *molatom = atom->molatom;
  Molecule **onemols = atom->avec->onemols;

  int *ilist = list->ilist;
  int *numneigh = list->numneigh;
  int **firstneigh = list->firstneigh;

  int *ilist_inner = listinner->ilist;
  int *numneigh_inner = listinner->numneigh;
  int **firstneigh_inner = listinner->firstneigh;

  int *ilist_middle,*numneigh_middle,**firstneigh_middle;
  if (respamiddle) {
    ilist_middle = listmiddle->ilist;
    numneigh_middle = listmiddle->numneigh;
    firstneigh_middle = listmiddle->firstneigh;
  }

  // each thread has its own page allocator
  MyPage<int> &ipage = list->ipage[tid];
  MyPage<int> &ipage_inner = listinner->ipage[tid];
  ipage.reset();
  ipage_inner.reset();

  MyPage<int> *ipage_middle;
  if (respamiddle) {
    ipage_middle = listmiddle->ipage + tid;
    ipage_middle->reset();
  }

  int which = 0;
  int minchange = 0;

  for (i = ifrom; i < ito; i++) {

    n = n_inner = 0;
    neighptr = ipage.vget();
    neighptr_inner = ipage_inner.vget();
    if (respamiddle) {
      n_middle = 0;
      neighptr_middle = ipage_middle->vget();
    }

    itype = type[i];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    if (moltemplate) {
      imol = molindex[i];
      iatom = molatom[i];
      tagprev = tag[i] - iatom - 1;
    }

    // loop over rest of atoms in i's bin, ghosts are at end of linked list
    // if j is owned atom, store it, since j is beyond i in linked list
    // if j is ghost, only store if j coords are "above and to the right" of i

    for (j = bins[i]; j >= 0; j = bins[j]) {
      if (j >= nlocal) {
        if (x[j][2] < ztmp) continue;
        if (x[j][2] == ztmp) {
          if (x[j][1] < ytmp) continue;
          if (x[j][1] == ytmp && x[j][0] < xtmp) continue;
        }
      }

      jtype = type[j];
      if (exclude && exclusion(i,j,itype,jtype,mask,molecule)) continue;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;

      if (rsq <= cutneighsq[itype][jtype]) {
        if (molecular) {
          if (!moltemplate)
            which = find_special(special[i],nspecial[i],tag[j]);
          else if (imol >=0)
            which = find_special(onemols[imol]->special[iatom],
                                 onemols[imol]->nspecial[iatom],
                                 tag[j]-tagprev);
            else which = 0;
          if (which == 0) neighptr[n++] = j;
          else if ((minchange = domain->minimum_image_check(delx,dely,delz)))
            neighptr[n++] = j;
          else if (which > 0) neighptr[n++] = j ^ (which << SBBITS);
        } else neighptr[n++] = j;

        if (rsq < cut_inner_sq) {
          if (which == 0) neighptr_inner[n_inner++] = j;
          else if (minchange) neighptr_inner[n_inner++] = j;
          else if (which > 0) neighptr_inner[n_inner++] = j ^ (which << SBBITS);
        }

        if (respamiddle &&
            rsq < cut_middle_sq && rsq > cut_middle_inside_sq) {
          if (which == 0) neighptr_middle[n_middle++] = j;
          else if (minchange) neighptr_middle[n_middle++] = j;
          else if (which > 0)
            neighptr_middle[n_middle++] = j ^ (which << SBBITS);
        }
      }
    }

    // loop over all atoms in other bins in stencil, store every pair

    ibin = coord2bin(x[i]);
    for (k = 0; k < nstencil; k++) {
      for (j = binhead[ibin+stencil[k]]; j >= 0; j = bins[j]) {
        jtype = type[j];
        if (exclude && exclusion(i,j,itype,jtype,mask,molecule)) continue;

        delx = xtmp - x[j][0];
        dely = ytmp - x[j][1];
        delz = ztmp - x[j][2];
        rsq = delx*delx + dely*dely + delz*delz;

        if (rsq <= cutneighsq[itype][jtype]) {
          if (molecular) {
            if (!moltemplate)
              which = find_special(special[i],nspecial[i],tag[j]);
            else if (imol >=0)
              which = find_special(onemols[imol]->special[iatom],
                                   onemols[imol]->nspecial[iatom],
                                   tag[j]-tagprev);
            else which = 0;
            if (which == 0) neighptr[n++] = j;
            else if ((minchange = domain->minimum_image_check(delx,dely,delz)))
              neighptr[n++] = j;
            else if (which > 0) neighptr[n++] = j ^ (which << SBBITS);
          } else neighptr[n++] = j;

          if (rsq < cut_inner_sq) {
            if (which == 0) neighptr_inner[n_inner++] = j;
            else if (minchange) neighptr_inner[n_inner++] = j;
            else if (which > 0)
              neighptr_inner[n_inner++] = j ^ (which << SBBITS);
          }

          if (respamiddle &&
              rsq < cut_middle_sq && rsq > cut_middle_inside_sq) {
            if (which == 0) neighptr_middle[n_middle++] = j;
            else if (minchange) neighptr_middle[n_middle++] = j;
            else if (which > 0)
              neighptr_middle[n_middle++] = j ^ (which << SBBITS);
          }
        }
      }
    }

    ilist[i] = i;
    firstneigh[i] = neighptr;
    numneigh[i] = n;
    ipage.vgot(n);
    if (ipage.status())
      error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");

    ilist_inner[i] = i;
    firstneigh_inner[i] = neighptr_inner;
    numneigh_inner[i] = n_inner;
    ipage.vgot(n_inner);
    if (ipage_inner.status())
      error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");

    if (respamiddle) {
      ilist_middle[i] = i;
      firstneigh_middle[i] = neighptr_middle;
      numneigh_middle[i] = n_middle;
      ipage_middle->vgot(n_middle);
      if (ipage_middle->status())
        error->one(FLERR,"Neighbor list overflow, boost neigh_modify one");
    }
  }
  NPAIR_OMP_CLOSE;
  list->inum = nlocal;
  listinner->inum = nlocal;
  if (respamiddle) listmiddle->inum = nlocal;
}
