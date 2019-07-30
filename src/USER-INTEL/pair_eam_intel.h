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

PairStyle(eam/intel,PairEAMIntel)

#else

#ifndef LMP_PAIR_EAM_INTEL_H
#define LMP_PAIR_EAM_INTEL_H

#include "pair_eam.h"
#include "fix_intel.h"

namespace LAMMPS_NS {


class PairEAMIntel : public PairEAM {
 public:
  friend class FixSemiGrandCanonicalMC;   // Alex Stukowski option

  PairEAMIntel(class LAMMPS *);
  virtual ~PairEAMIntel();
  virtual void compute(int, int);
  void init_style();
  int pack_forward_comm(int, int *, double *, int, int *);
  void unpack_forward_comm(int, int, double *);

 protected:

  FixIntel *fix;
  int _cop, _onetype, _ccache_stride;
  float *fp_float;

  template <class flt_t>
  int pack_forward_comm(int, int *, double *, flt_t *);
  template <class flt_t>
  void unpack_forward_comm(int, int, double *, flt_t *);

  template <class flt_t> class ForceConst;
  template <class flt_t, class acc_t>
  void compute(int eflag, int vflag, IntelBuffers<flt_t,acc_t> *buffers,
               const ForceConst<flt_t> &fc);
  template <int ONETYPE, int EFLAG, int NEWTON_PAIR, class flt_t,
            class acc_t>
  void eval(const int offload, const int vflag,
            IntelBuffers<flt_t,acc_t> * buffers,
            const ForceConst<flt_t> &fc, const int astart, const int aend);

  template <class flt_t, class acc_t>
  void pack_force_const(ForceConst<flt_t> &fc,
                        IntelBuffers<flt_t, acc_t> *buffers);

  // ----------------------------------------------------------------------

  template <class flt_t>
  class ForceConst {
  public:
    typedef struct { flt_t a, b, c, d; } fc_packed1;
    typedef struct { flt_t a, b, c, d, e, f, g, h; } fc_packed2;

    flt_t **scale_f;
    fc_packed1 *rhor_spline_f, *rhor_spline_e;
    fc_packed1 *frho_spline_f, *frho_spline_e;
    fc_packed2 *z2r_spline_t;

    ForceConst() : _ntypes(0), _nr(0)  {}
    ~ForceConst() { set_ntypes(0, 0, 0, NULL, _cop); }

    void set_ntypes(const int ntypes, const int nr, const int nrho,
                    Memory *memory, const int cop);
    inline int rhor_jstride() const { return _nr; }
    inline int rhor_istride() const { return _nr * _ntypes; }
    inline int frho_stride() const { return _nrho; }

  private:
    int _ntypes, _nr, _nrho, _cop;
    Memory *_memory;
  };
  ForceConst<float> force_const_single;
  ForceConst<double> force_const_double;
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: The 'package intel' command is required for /intel styles

Self-explanatory.

*/
