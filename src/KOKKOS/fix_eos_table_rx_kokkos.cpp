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
   Contributing author: Stan Moore (Sandia)
------------------------------------------------------------------------- */

#include <stdlib.h>
#include <string.h>
#include "fix_eos_table_rx_kokkos.h"
#include "atom_kokkos.h"
#include "error.h"
#include "force.h"
#include "memory.h"
#include "comm.h"
#include <math.h>
#include "modify.h"
#include "atom_masks.h"

#define MAXLINE 1024

using namespace LAMMPS_NS;
using namespace FixConst;

/* ---------------------------------------------------------------------- */

template<class DeviceType>
FixEOStableRXKokkos<DeviceType>::FixEOStableRXKokkos(LAMMPS *lmp, int narg, char **arg) :
  FixEOStableRX(lmp, narg, arg)
{
  atomKK = (AtomKokkos *) atom;
  execution_space = ExecutionSpaceFromDevice<DeviceType>::space;
  datamask_read = X_MASK | F_MASK | TYPE_MASK | ENERGY_MASK | VIRIAL_MASK;
  datamask_modify = F_MASK | ENERGY_MASK | VIRIAL_MASK;

  k_error_flag = DAT::tdual_int_scalar("fix:error_flag");
  k_warning_flag = DAT::tdual_int_scalar("fix:warning_flag");
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
FixEOStableRXKokkos<DeviceType>::~FixEOStableRXKokkos()
{

}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::setup(int vflag)
{
  int nlocal = atom->nlocal;
  mask = atomKK->k_mask.view<DeviceType>();
  uCond = atomKK->k_uCond.view<DeviceType>();
  uMech = atomKK->k_uMech.view<DeviceType>();
  uChem = atomKK->k_uChem.view<DeviceType>();
  dpdTheta= atomKK->k_dpdTheta.view<DeviceType>();
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();

  Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXSetup>(0,nlocal),*this);

  // Communicate the updated momenta and velocities to all nodes
  comm->forward_comm_fix(this);

  Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXTemperatureLookup>(0,nlocal),*this);

  error_check();
}

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::operator()(TagFixEOStableRXSetup, const int &i) const {
  if (mask[i] & groupbit) {
    const double duChem = uCG[i] - uCGnew[i];
    uChem[i] += duChem;
    uCG[i] = 0.0;
    uCGnew[i] = 0.0;
  }
}

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::operator()(TagFixEOStableRXTemperatureLookup, const int &i) const {
  if (mask[i] & groupbit)
    temperature_lookup(i,uCond[i]+uMech[i]+uChem[i],dpdTheta[i]);
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::init()
{
  int nlocal = atom->nlocal;
  mask = atomKK->k_mask.view<DeviceType>();
  uCond = atomKK->k_uCond.view<DeviceType>();
  uMech = atomKK->k_uMech.view<DeviceType>();
  uChem = atomKK->k_uChem.view<DeviceType>();
  dpdTheta= atomKK->k_dpdTheta.view<DeviceType>();

  if (this->restart_reset)
    Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXTemperatureLookup>(0,nlocal),*this);
  else
    Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXInit>(0,nlocal),*this);

  error_check();
}

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::operator()(TagFixEOStableRXInit, const int &i) const {
  double tmp;
  if (mask[i] & groupbit) {
    if(dpdTheta[i] <= 0.0)
      k_error_flag.d_view() = 1;
    energy_lookup(i,dpdTheta[i],tmp);
    uCond[i] = tmp / 2.0;
    uMech[i] = tmp / 2.0;
    uChem[i] = 0.0;
  }
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::post_integrate()
{
  int nlocal = atom->nlocal;
  mask = atomKK->k_mask.view<DeviceType>();
  uCond = atomKK->k_uCond.view<DeviceType>();
  uMech = atomKK->k_uMech.view<DeviceType>();
  uChem = atomKK->k_uChem.view<DeviceType>();
  dpdTheta= atomKK->k_dpdTheta.view<DeviceType>();

  Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXTemperatureLookup2>(0,nlocal),*this);

  error_check();
}

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::operator()(TagFixEOStableRXTemperatureLookup2, const int &i) const {
  if (mask[i] & groupbit){
    temperature_lookup(i,uCond[i]+uMech[i]+uChem[i],dpdTheta[i]);
    if (dpdTheta[i] <= 0.0)
      k_error_flag.d_view() = 1;
  }
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::end_of_step()
{
  int nlocal = atom->nlocal;
  mask = atomKK->k_mask.view<DeviceType>();
  uCond = atomKK->k_uCond.view<DeviceType>();
  uMech = atomKK->k_uMech.view<DeviceType>();
  uChem = atomKK->k_uChem.view<DeviceType>();
  dpdTheta= atomKK->k_dpdTheta.view<DeviceType>();
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();
  double duChem;

  // Communicate the ghost uCGnew
  comm->reverse_comm_fix(this);

  Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXSetup>(0,nlocal),*this);

  // Communicate the updated momenta and velocities to all nodes
  comm->forward_comm_fix(this);

  Kokkos::parallel_for(Kokkos::RangePolicy<DeviceType, TagFixEOStableRXTemperatureLookup2>(0,nlocal),*this);

  error_check();
}

/* ----------------------------------------------------------------------
   calculate potential ui at temperature thetai
------------------------------------------------------------------------- */

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::energy_lookup(int id, double thetai, double &ui) const
{
  int itable;
  double fraction, uTmp, nTotal;

  ui = 0.0;
  nTotal = 0.0;
  for(int ispecies=0;ispecies<nspecies;ispecies++){
    Table *tb = &tables[ispecies];
    thetai = MAX(thetai,tb->lo);
    thetai = MIN(thetai,tb->hi);

    if (tabstyle == LINEAR) {
      itable = static_cast<int> ((thetai - tb->lo) * tb->invdelta);
      fraction = (thetai - tb->r[itable]) * tb->invdelta;
      uTmp = tb->e[itable] + fraction*tb->de[itable];

      uTmp += dHf[ispecies];
      // mol fraction form:
      ui += atom->dvector[ispecies][id]*uTmp;
      nTotal += atom->dvector[ispecies][id];
    }
  }
  ui = ui - double(nTotal+1.5)*force->boltz*thetai;
}

/* ----------------------------------------------------------------------
   calculate temperature thetai at energy ui
------------------------------------------------------------------------- */

template<class DeviceType>
KOKKOS_INLINE_FUNCTION
void FixEOStableRXKokkos<DeviceType>::temperature_lookup(int id, double ui, double &thetai) const
{
  Table *tb = &tables[0];

  int it;
  double t1,t2,u1,u2,f1,f2;
  double maxit = 100;
  double temp;
  double delta = 0.001;

  // Store the current thetai in t1
  t1 = MAX(thetai,tb->lo);
  t1 = MIN(t1,tb->hi);
  if(t1==tb->hi) delta = -delta;

  // Compute u1 at thetai
  energy_lookup(id,t1,u1);

  // Compute f1
  f1 = u1 - ui;

  // Compute guess of t2
  t2 = (1.0 + delta)*t1;

  // Compute u2 at t2
  energy_lookup(id,t2,u2);

  // Compute f1
  f2 = u2 - ui;

  // Apply the Secant Method
  for(it=0; it<maxit; it++){
    if(fabs(f2-f1)<1e-15){
      if(isnan(f1) || isnan(f2)) k_error_flag.d_view() = 2;
      temp = t1;
      temp = MAX(temp,tb->lo);
      temp = MIN(temp,tb->hi);
      k_warning_flag.d_view() = 1;
      break;
    }
    temp = t2 - f2*(t2-t1)/(f2-f1);
    if(fabs(temp-t2) < 1e-6) break;
    f1 = f2;
    t1 = t2;
    t2 = temp;
    energy_lookup(id,t2,u2);
    f2 = u2 - ui;
  }
  if(it==maxit){
    if(isnan(f1) || isnan(f2) || isnan(ui) || isnan(thetai) || isnan(t1) || isnan(t2))
      k_error_flag.d_view() = 2;
    k_error_flag.d_view() = 3;
  }
  thetai = temp;
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
int FixEOStableRXKokkos<DeviceType>::pack_forward_comm(int n, int *list, double *buf, int pbc_flag, int *pbc)
{
  int ii,jj,m;
  uChem = atomKK->k_uChem.view<DeviceType>();
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();

  m = 0;
  for (ii = 0; ii < n; ii++) {
    jj = list[ii];
    buf[m++] = uChem[jj];
    buf[m++] = uCG[jj];
    buf[m++] = uCGnew[jj];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::unpack_forward_comm(int n, int first, double *buf)
{
  int ii,m,last;
  uChem = atomKK->k_uChem.view<DeviceType>();
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();

  m = 0;
  last = first + n ;
  for (ii = first; ii < last; ii++){
    uChem[ii]  = buf[m++];
    uCG[ii]    = buf[m++];
    uCGnew[ii] = buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
int FixEOStableRXKokkos<DeviceType>::pack_reverse_comm(int n, int first, double *buf)
{
  int i,m,last;
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();

  m = 0;
  last = first + n;
  for (i = first; i < last; i++) {
    buf[m++] = uCG[i];
    buf[m++] = uCGnew[i];
  }
  return m;
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::unpack_reverse_comm(int n, int *list, double *buf)
{
  int i,j,m;
  uCG = atomKK->k_uCG.view<DeviceType>();
  uCGnew = atomKK->k_uCGnew.view<DeviceType>();

  m = 0;
  for (i = 0; i < n; i++) {
    j = list[i];

    uCG[j] += buf[m++];
    uCGnew[j] += buf[m++];
  }
}

/* ---------------------------------------------------------------------- */

template<class DeviceType>
void FixEOStableRXKokkos<DeviceType>::error_check()
{
  k_error_flag.template modify<DeviceType>();
  k_error_flag.template sync<LMPHostType>();
  if (k_error_flag.h_view() == 1)
    error->one(FLERR,"Internal temperature <= zero");
  else if (k_error_flag.h_view() == 2)
    error->one(FLERR,"NaN detected in secant solver.");
  else if (k_error_flag.h_view() == 3)
    error->one(FLERR,"Maxit exceeded in secant solver.");

  k_warning_flag.template modify<DeviceType>();
  k_warning_flag.template sync<LMPHostType>();
  if (k_warning_flag.h_view()) {
    error->warning(FLERR,"Secant solver did not converge because table bounds were exceeded.");
    k_warning_flag.h_view() = 0;
    k_warning_flag.template modify<LMPHostType>();
    k_warning_flag.template sync<DeviceType>();
  }
}

/* ---------------------------------------------------------------------- */

namespace LAMMPS_NS {
template class FixEOStableRXKokkos<LMPDeviceType>;
#ifdef KOKKOS_HAVE_CUDA
template class FixEOStableRXKokkos<LMPHostType>;
#endif
}
