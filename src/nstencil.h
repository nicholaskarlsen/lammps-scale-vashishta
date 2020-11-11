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

#ifndef LMP_NSTENCIL_H
#define LMP_NSTENCIL_H

#include "pointers.h"

namespace LAMMPS_NS {

class NStencil : protected Pointers {
 public:
  int istyle;                      // 1-N index into binnames
  class NBin *nb;                  // ptr to NBin instance I depend on
  bigint last_stencil;             // last timestep stencil was created

  int nstencil;                    // # of bins in stencil
  int *stencil;                    // list of bin offsets
  int **stencilxyz;                // bin offsets in xyz dims
  int *nstencil_multi;             // # bins in each type-based multi stencil
  int **stencil_multi;             // list of bin offsets in each stencil
  double **distsq_multi;           // sq distances to bins in each stencil
  int ** nstencil_multi2;          // # bins bins in each itype-jtype multi2 stencil
  int *** stencil_multi2;          // list of bin offsets in each multi2 stencil
  int ** maxstencil_multi2;        // max stencil size for each multi2 stencil
  
  // Note distsq_multi is used in multi to quickly skip bins beyond interaction cutoff
  // Not quite sure why bins are beyond this distance? Have to think
  // Probably not needed for multi2 since bins are more efficiently chosen
  
  int sx,sy,sz;                    // extent of stencil in each dim
  int **sx_multi2;                 // analogs for multi tiered
  int **sy_multi2;
  int **sz_multi2;

  double cutoff_custom;            // cutoff set by requestor  
  
  // Arrays to store options for multi/tiered itype-jtype stencils
  bool **stencil_half;             // flag creation of a half stencil for itype-jtype
  bool **stencil_skip;             // skip creation of itype-jtype stencils (for newton on)  
  int **stencil_bin_type;          // what type to use for bin information
  double **stencil_cut;            // cutoff used for stencil size

  NStencil(class LAMMPS *);
  virtual ~NStencil();
  void post_constructor(class NeighRequest *);
  void copy_neighbor_info();
  virtual void create_setup();
  double memory_usage();

  virtual void create() = 0;

  inline int get_maxstencil() {return maxstencil;}

 protected:

  // data from Neighbor class

  int neighstyle;
  double cutneighmax;
  double cutneighmaxsq;
  double *cuttypesq;
  double **cutneighsq;

  // data from NBin class

  int mbinx,mbiny,mbinz;
  double binsizex,binsizey,binsizez;
  double bininvx,bininvy,bininvz;
  
  // analogs for multi-tiered
  
  int **mbinx_multi2;
  int **mbiny_multi2;
  int **mbinz_multi2;
  double **binsizex_multi2;
  double **binsizey_multi2;
  double **binsizez_multi2;

  // data common to all NStencil variants

  int xyzflag;                     // 1 if stencilxyz is allocated
  int maxstencil;                  // max size of stencil
  int maxstencil_multi;            // max sizes of stencils

  int dimension;

  // methods for all NStencil variants

  void copy_bin_info();                     // copy info from NBin class
  double bin_distance(int, int, int);       // distance between bin corners

  // methods for multi/tiered NStencil
  
  void copy_bin_info_multi2(int);     // copy mult/tiered info from NBin class
  void set_stencil_properties();            // determine which stencils to build and how 
};

}

#endif

/* ERROR/WARNING messages:

*/
