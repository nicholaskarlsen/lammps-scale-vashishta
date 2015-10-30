/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef PAIR_CLASS

PairStyle(lj/expand/kk,PairLJExpandKokkos<LMPDeviceType>)
PairStyle(lj/expand/kk/device,PairLJExpandKokkos<LMPDeviceType>)
PairStyle(lj/expand/kk/host,PairLJExpandKokkos<LMPHostType>)

#else

#ifndef LMP_PAIR_LJ_EXPAND_KOKKOS_H
#define LMP_PAIR_LJ_EXPAND_KOKKOS_H

#include "pair_kokkos.h"
#include "pair_lj_expand.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

template<class DeviceType>
class PairLJExpandKokkos : public PairLJExpand {
 public:
  enum {EnabledNeighFlags=FULL|HALFTHREAD|HALF|N2|FULLCLUSTER};
  enum {COUL_FLAG=0};
  typedef DeviceType device_type;
  PairLJExpandKokkos(class LAMMPS *);
  ~PairLJExpandKokkos();

  void compute(int, int);

  void settings(int, char **);
  void init_style();
  double init_one(int, int);

  struct params_lj{
    params_lj(){cutsq=0,lj1=0;lj2=0;lj3=0;lj4=0;offset=0;shift=0;};
    params_lj(int i){cutsq=0,lj1=0;lj2=0;lj3=0;lj4=0;offset=0;shift=0;};
    F_FLOAT cutsq,lj1,lj2,lj3,lj4,offset,shift;
  };

 protected:
  void cleanup_copy();

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  F_FLOAT compute_fpair(const F_FLOAT& rsq, const int& i, const int&j, const int& itype, const int& jtype) const;

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  F_FLOAT compute_evdwl(const F_FLOAT& rsq, const int& i, const int&j, const int& itype, const int& jtype) const;

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  F_FLOAT compute_ecoul(const F_FLOAT& rsq, const int& i, const int&j, const int& itype, const int& jtype) const {
    return 0.0;
  }

  template<bool STACKPARAMS, class Specialisation>
  KOKKOS_INLINE_FUNCTION
  F_FLOAT compute_fcoul(const F_FLOAT& rsq, const int& i, const int&j, const int& itype,
                        const int& jtype, const F_FLOAT& factor_coul, const F_FLOAT& qtmp) const {
    return 0.0;
  }

  Kokkos::DualView<params_lj**,Kokkos::LayoutRight,DeviceType> k_params;
  typename Kokkos::DualView<params_lj**,Kokkos::LayoutRight,DeviceType>::t_dev_const_um params;
  params_lj m_params[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];  // hardwired to space for 15 atom types
  F_FLOAT m_cutsq[MAX_TYPES_STACKPARAMS+1][MAX_TYPES_STACKPARAMS+1];
  typename ArrayTypes<DeviceType>::t_x_array_randomread x;
  typename ArrayTypes<DeviceType>::t_x_array c_x;
  typename ArrayTypes<DeviceType>::t_f_array f;
  typename ArrayTypes<DeviceType>::t_int_1d_randomread type;
  typename ArrayTypes<DeviceType>::t_efloat_1d d_eatom;
  typename ArrayTypes<DeviceType>::t_virial_array d_vatom;
  typename ArrayTypes<DeviceType>::t_tagint_1d tag;

  int newton_pair;
  double special_lj[4];

  typename ArrayTypes<DeviceType>::tdual_ffloat_2d k_cutsq;
  typename ArrayTypes<DeviceType>::t_ffloat_2d d_cutsq;


  int neighflag;
  int nlocal,nall,eflag,vflag;

  void allocate();
  friend class PairComputeFunctor<PairLJExpandKokkos,FULL,true>;
  friend class PairComputeFunctor<PairLJExpandKokkos,HALF,true>;
  friend class PairComputeFunctor<PairLJExpandKokkos,HALFTHREAD,true>;
  friend class PairComputeFunctor<PairLJExpandKokkos,N2,true>;
  friend class PairComputeFunctor<PairLJExpandKokkos,FULLCLUSTER,true >;
  friend class PairComputeFunctor<PairLJExpandKokkos,FULL,false>;
  friend class PairComputeFunctor<PairLJExpandKokkos,HALF,false>;
  friend class PairComputeFunctor<PairLJExpandKokkos,HALFTHREAD,false>;
  friend class PairComputeFunctor<PairLJExpandKokkos,N2,false>;
  friend class PairComputeFunctor<PairLJExpandKokkos,FULLCLUSTER,false >;
  friend EV_FLOAT pair_compute_neighlist<PairLJExpandKokkos,FULL,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJExpandKokkos,HALF,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJExpandKokkos,HALFTHREAD,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJExpandKokkos,N2,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_fullcluster<PairLJExpandKokkos,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute<PairLJExpandKokkos,void>(PairLJExpandKokkos*,NeighListKokkos<DeviceType>*);
  friend void pair_virial_fdotr_compute<PairLJExpandKokkos>(PairLJExpandKokkos*);
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
