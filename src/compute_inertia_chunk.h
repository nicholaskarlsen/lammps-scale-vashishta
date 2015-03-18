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

ComputeStyle(inertia/chunk,ComputeInertiaChunk)

#else

#ifndef LMP_COMPUTE_INERTIA_CHUNK_H
#define LMP_COMPUTE_INERTIA_CHUNK_H

#include "compute.h"

namespace LAMMPS_NS {

class ComputeInertiaChunk : public Compute {
 public:
  ComputeInertiaChunk(class LAMMPS *, int, char **);
  ~ComputeInertiaChunk();
  void init();
  void compute_array();

  void lock_enable();
  void lock_disable();
  int lock_length();
  void lock(class Fix *, bigint, bigint);
  void unlock(class Fix *);

  double memory_usage();

 private:
  int nchunk,maxchunk;
  char *idchunk;
  class ComputeChunkAtom *cchunk;

  double *massproc,*masstotal;
  double **com,**comall;
  double **inertia,**inertiaall;

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

E: Chunk/atom compute does not exist for compute inertia/chunk

UNDOCUMENTED

E: Compute inertia/chunk does not use chunk/atom compute

UNDOCUMENTED

U: Compute inertia/molecule requires molecular atom style

Self-explanatory.

U: Molecule count changed in compute inertia/molecule

Number of molecules must remain constant over time.

*/
