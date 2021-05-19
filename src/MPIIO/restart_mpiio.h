/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifndef LMP_RESTART_MPIIO_H
#define LMP_RESTART_MPIIO_H

#include "pointers.h"

namespace LAMMPS_NS {

class RestartMPIIO : protected Pointers {
 private:
  MPI_File mpifh;
  int nprocs, myrank;

 public:
  int mpiio_exists;

  RestartMPIIO(class LAMMPS *);
  ~RestartMPIIO() {}
  void openForRead(const char *);
  void openForWrite(const char *);
  void write(MPI_Offset, int, double *);
  void read(MPI_Offset, bigint, double *);
  void close();
};

}    // namespace LAMMPS_NS

#endif

/* ERROR/WARNING messages:

E: Cannot open restart file for reading - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

E: Cannot open restart file for writing - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

E: Cannot set restart file size - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

E: Cannot write to restart file - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

E: Cannot read from restart file - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

E: Cannot close restart file - MPI error: %s

This error was generated by MPI when reading/writing an MPI-IO restart
file.

*/
