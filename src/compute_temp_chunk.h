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

#ifdef COMPUTE_CLASS

ComputeStyle(temp/chunk,ComputeTempChunk)

#else

#ifndef LMP_COMPUTE_TEMP_CHUNK_H
#define LMP_COMPUTE_TEMP_CHUNK_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputeTempChunk : public Compute {
 public:
  ComputeTempChunk(class LAMMPS *, int, char **);
  ~ComputeTempChunk();
  void init();
  void compute_vector();

  void lock_enable();
  void lock_disable();
  int lock_length();
  void lock(class Fix *, bigint, bigint);
  void unlock(class Fix *);

  double memory_usage();

 private:
  int nchunk,maxchunk,comflag,biasflag;
  char *idchunk;
  class ComputeChunkAtom *cchunk;
  double adof,cdof;
  char *id_bias;
  class Compute *tbias;     // ptr to additional bias compute

  double *ke,*keall;
  int *count,*countall;
  double *massproc,*masstotal;
  double **vcm,**vcmall;

  void vcm_compute();
  void allocate();
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Compute com/molecule requires molecular atom style

Self-explanatory.

E: Molecule count changed in compute com/molecule

Number of molecules must remain constant over time.

*/
