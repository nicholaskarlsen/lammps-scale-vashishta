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

#ifndef LMP_COMM_BRICK_H
#define LMP_COMM_BRICK_H

#include "comm.h"

namespace LAMMPS_NS {

class CommBrick : public Comm {
 public:
  CommBrick(class LAMMPS *);
  CommBrick(class LAMMPS *, class Comm *);
  virtual ~CommBrick();

  virtual void init();
  virtual void setup();                        // setup 3d comm pattern
  virtual void forward_comm(int dummy = 0);    // forward comm of atom coords
  virtual void reverse_comm();                 // reverse comm of forces
  virtual void exchange();                     // move atoms to new procs
  virtual void borders();                      // setup list of atoms to comm

  virtual void forward_comm_pair(class Pair *);    // forward comm from a Pair
  virtual void reverse_comm_pair(class Pair *);    // reverse comm from a Pair
  virtual void forward_comm_fix(class Fix *, int size=0);  
                                                   // forward comm from a Fix
  virtual void reverse_comm_fix(class Fix *, int size=0);
                                                   // reverse comm from a Fix
  virtual void forward_comm_compute(class Compute *);  // forward from a Compute
  virtual void reverse_comm_compute(class Compute *);  // reverse from a Compute
  virtual void forward_comm_dump(class Dump *);    // forward comm from a Dump
  virtual void reverse_comm_dump(class Dump *);    // reverse comm from a Dump

  void forward_comm_array(int, double **);         // forward comm of array
  int exchange_variable(int, double *, double *&);  // exchange on neigh stencil
  virtual bigint memory_usage();

 protected:
  int nswap;                        // # of swaps to perform = sum of maxneed
  int recvneed[3][2];               // # of procs away I recv atoms from
  int sendneed[3][2];               // # of procs away I send atoms to
  int maxneed[3];                   // max procs away any proc needs, per dim
  int maxswap;                      // max # of swaps memory is allocated for
  int *sendnum,*recvnum;            // # of atoms to send/recv in each swap
  int *sendproc,*recvproc;          // proc to send/recv to/from at each swap
  int *size_forward_recv;           // # of values to recv in each forward comm
  int *size_reverse_send;           // # to send in each reverse comm
  int *size_reverse_recv;           // # to recv in each reverse comm
  double *slablo,*slabhi;           // bounds of slab to send at each swap
  double **multilo,**multihi;       // bounds of slabs for multi-type swap
  double **cutghostmulti;           // cutghost on a per-type basis
  int *pbc_flag;                    // general flag for sending atoms thru PBC
  int **pbc;                        // dimension flags for PBC adjustments

  int *firstrecv;                   // where to put 1st recv atom in each swap
  int **sendlist;                   // list of atoms to send in each swap
  int *maxsendlist;                 // max size of send list for each swap

  double *buf_send;                 // send buffer for all comm
  double *buf_recv;                 // recv buffer for all comm
  int maxsend,maxrecv;              // current size of send/recv buffer
  int bufextra;                     // extra space beyond maxsend in send buffer
  int smax,rmax;             // max size in atoms of single borders send/recv

  void init_buffers();
  int updown(int, int, int, double, int, double *);
                                            // compare cutoff to procs
  virtual void grow_send(int, int);         // reallocate send buffer
  virtual void grow_recv(int);              // free/allocate recv buffer
  virtual void grow_list(int, int);         // reallocate one sendlist
  virtual void grow_swap(int);              // grow swap and multi arrays
  virtual void allocate_swap(int);          // allocate swap arrays
  virtual void allocate_multi(int);         // allocate multi arrays
  virtual void free_swap();                 // free swap arrays
  virtual void free_multi();                // free multi arrays
};

}

#endif

/* ERROR/WARNING messages:

E: Cannot change to comm_style brick from tiled layout

UNDOCUMENTED

U: OMP_NUM_THREADS environment is not set.

This environment variable must be set appropriately to use the
USER-OMP pacakge.

U: Bad grid of processors

The 3d grid of processors defined by the processors command does not
match the number of processors LAMMPS is being run on.

U: Processor count in z must be 1 for 2d simulation

Self-explanatory.

U: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

U: Invalid group in communicate command

Self-explanatory.

U: Communicate group != atom_modify first group

Self-explanatory.

U: Invalid cutoff in communicate command

Specified cutoff must be >= 0.0.

U: Specified processors != physical processors

The 3d grid of processors defined by the processors command does not
match the number of processors LAMMPS is being run on.

U: Cannot use processors part command without using partitions

See the command-line -partition switch.

U: Invalid partitions in processors part command

Valid partitions are numbered 1 to N and the sender and receiver
cannot be the same partition.

U: Sending partition in processors part command is already a sender

Cannot specify a partition to be a sender twice.

U: Receiving partition in processors part command is already a receiver

Cannot specify a partition to be a receiver twice.

U: Processors grid numa and map style are incompatible

Using numa for gstyle in the processors command requires using
cart for the map option.

U: Processors part option and grid style are incompatible

Cannot use gstyle numa or custom with the part option.

*/
