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

PairStyle(eam/alloy/kk,PairEAMAlloyKokkos<LMPDeviceType>)
PairStyle(eam/alloy/kk/device,PairEAMAlloyKokkos<LMPDeviceType>)
PairStyle(eam/alloy/kk/host,PairEAMAlloyKokkos<LMPHostType>)

#else

#ifndef LMP_PAIR_EAM_ALLOY_KOKKOS_H
#define LMP_PAIR_EAM_ALLOY_KOKKOS_H

#include <stdio.h>
#include "pair_kokkos.h"
#include "pair_eam.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

struct TagPairEAMAlloyPackForwardComm{};
struct TagPairEAMAlloyUnpackForwardComm{};
struct TagPairEAMAlloyInitialize{};

template<int NEIGHFLAG, int NEWTON_PAIR>
struct TagPairEAMAlloyKernelA{};

template<int EFLAG>
struct TagPairEAMAlloyKernelB{};

template<int EFLAG>
struct TagPairEAMAlloyKernelAB{};

template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
struct TagPairEAMAlloyKernelC{};

// Cannot use virtual inheritance on the GPU

template<class DeviceType>
class PairEAMAlloyKokkos : public PairEAM {
 public:
  enum {EnabledNeighFlags=FULL|HALFTHREAD|HALF};
  enum {COUL_FLAG=0};
  typedef DeviceType device_type;
  typedef ArrayTypes<DeviceType> AT;
  typedef EV_FLOAT value_type;

  PairEAMAlloyKokkos(class LAMMPS *);
  virtual ~PairEAMAlloyKokkos();
  virtual void compute(int, int);
  void init_style();
  void *extract(const char *, int &) { return NULL; }
  void coeff(int, char **);

  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyPackForwardComm, const int&) const;

  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyUnpackForwardComm, const int&) const;

  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyInitialize, const int&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelA<NEIGHFLAG,NEWTON_PAIR>, const int&) const;

  template<int EFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelB<EFLAG>, const int&, EV_FLOAT&) const;

  template<int EFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelB<EFLAG>, const int&) const;

  template<int EFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelAB<EFLAG>, const int&, EV_FLOAT&) const;

  template<int EFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelAB<EFLAG>, const int&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelC<NEIGHFLAG,NEWTON_PAIR,EVFLAG>, const int&, EV_FLOAT&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR, int EVFLAG>
  KOKKOS_INLINE_FUNCTION
  void operator()(TagPairEAMAlloyKernelC<NEIGHFLAG,NEWTON_PAIR,EVFLAG>, const int&) const;

  template<int NEIGHFLAG, int NEWTON_PAIR>
  KOKKOS_INLINE_FUNCTION
  void ev_tally(EV_FLOAT &ev, const int &i, const int &j,
      const F_FLOAT &epair, const F_FLOAT &fpair, const F_FLOAT &delx,
                  const F_FLOAT &dely, const F_FLOAT &delz) const;

  virtual int pack_forward_comm_kokkos(int, DAT::tdual_int_2d, int, DAT::tdual_xfloat_1d&,
                               int, int *);
  virtual void unpack_forward_comm_kokkos(int, int, DAT::tdual_xfloat_1d&);
  virtual int pack_forward_comm(int, int *, double *, int, int *);
  virtual void unpack_forward_comm(int, int, double *);
  int pack_reverse_comm(int, int, double *);
  void unpack_reverse_comm(int, int *, double *);

 protected:
  void cleanup_copy();

  typename AT::t_x_array_randomread x;
  typename AT::t_f_array f;
  typename AT::t_int_1d_randomread type;
  typename AT::t_tagint_1d tag;

  DAT::tdual_efloat_1d k_eatom;
  DAT::tdual_virial_array k_vatom;
  DAT::t_efloat_1d d_eatom;
  DAT::t_virial_array d_vatom;

  DAT::tdual_ffloat_1d k_rho;
  DAT::tdual_ffloat_1d k_fp;
  DAT::t_ffloat_1d d_rho;
  typename AT::t_ffloat_1d v_rho;
  DAT::t_ffloat_1d d_fp;
  HAT::t_ffloat_1d h_rho;
  HAT::t_ffloat_1d h_fp;

  DAT::t_int_1d_randomread d_type2frho;
  DAT::t_int_2d_randomread d_type2rhor;
  DAT::t_int_2d_randomread d_type2z2r;

  typedef Kokkos::DualView<F_FLOAT**[7],Kokkos::LayoutRight,DeviceType> tdual_ffloat_2d_n7;
  typedef typename tdual_ffloat_2d_n7::t_dev_const_randomread t_ffloat_2d_n7_randomread;
  typedef typename tdual_ffloat_2d_n7::t_host t_host_ffloat_2d_n7;

  t_ffloat_2d_n7_randomread d_frho_spline;
  t_ffloat_2d_n7_randomread d_rhor_spline;
  t_ffloat_2d_n7_randomread d_z2r_spline;

  virtual void file2array();
  void file2array_alloy();
  void array2spline();
  void interpolate(int, double, double *, t_host_ffloat_2d_n7, int);
  void read_file(char *);

  typename AT::t_neighbors_2d d_neighbors;
  typename AT::t_int_1d_randomread d_ilist;
  typename AT::t_int_1d_randomread d_numneigh;
  //NeighListKokkos<DeviceType> k_list;

  int iswap;
  int first;
  typename AT::t_int_2d d_sendlist;
  typename AT::t_xfloat_1d_um v_buf;

  int neighflag,newton_pair;
  int nlocal,nall,eflag,vflag;

  friend void pair_virial_fdotr_compute<PairEAMAlloyKokkos>(PairEAMAlloyKokkos*);
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Cannot use chosen neighbor list style with pair eam/kk/alloy

Self-explanatory.

E: Incorrect args for pair coefficients

Self-explanatory.  Check the input script or data file.

E: No matching element in EAM potential file

The EAM potential file does not contain elements that match the
requested elements.

E: Cannot open EAM potential file %s

The specified EAM potential file cannot be opened.  Check that the
path and name are correct.

E: Incorrect element names in EAM potential file

The element names in the EAM file do not match those requested.

*/
