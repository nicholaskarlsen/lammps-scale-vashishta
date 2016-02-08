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

PairStyle(lj/class2/kk,PairLJClass2Kokkos<LMPDeviceType>)
PairStyle(lj/class2/kk/device,PairLJClass2Kokkos<LMPDeviceType>)
PairStyle(lj/class2/kk/host,PairLJClass2Kokkos<LMPHostType>)

#else

#ifndef LMP_PAIR_LJ_CLASS2_KOKKOS_H
#define LMP_PAIR_LJ_CLASS2_KOKKOS_H

#include "pair_kokkos.h"
#include "pair_lj_class2.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

template<class DeviceType>
class PairLJClass2Kokkos : public PairLJClass2 {
 public:
  enum {EnabledNeighFlags=FULL|HALFTHREAD|HALF|N2|FULLCLUSTER};
  enum {COUL_FLAG=0};
  typedef DeviceType device_type;
  PairLJClass2Kokkos(class LAMMPS *);
  ~PairLJClass2Kokkos();

  void compute(int, int);

  void settings(int, char **);
  void init_style();
  double init_one(int, int);

  struct params_lj{
    params_lj(){cutsq=0,lj1=0;lj2=0;lj3=0;lj4=0;offset=0;};
    params_lj(int i){cutsq=0,lj1=0;lj2=0;lj3=0;lj4=0;offset=0;};
    F_FLOAT cutsq,lj1,lj2,lj3,lj4,offset;
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
  friend class PairComputeFunctor<PairLJClass2Kokkos,FULL,true>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,HALF,true>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,HALFTHREAD,true>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,N2,true>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,FULLCLUSTER,true >;
  friend class PairComputeFunctor<PairLJClass2Kokkos,FULL,false>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,HALF,false>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,HALFTHREAD,false>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,N2,false>;
  friend class PairComputeFunctor<PairLJClass2Kokkos,FULLCLUSTER,false >;
  friend EV_FLOAT pair_compute_neighlist<PairLJClass2Kokkos,FULL,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJClass2Kokkos,HALF,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJClass2Kokkos,HALFTHREAD,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_neighlist<PairLJClass2Kokkos,N2,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute_fullcluster<PairLJClass2Kokkos,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend EV_FLOAT pair_compute<PairLJClass2Kokkos,void>(PairLJClass2Kokkos*,NeighListKokkos<DeviceType>*);
  friend void pair_virial_fdotr_compute<PairLJClass2Kokkos>(PairLJClass2Kokkos*);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

UNDOCUMENTED

E: Cannot use Kokkos pair style with rRESPA inner/middle

UNDOCUMENTED

E: Cannot use chosen neighbor list style with lj/class2/kk

UNDOCUMENTED

*/
