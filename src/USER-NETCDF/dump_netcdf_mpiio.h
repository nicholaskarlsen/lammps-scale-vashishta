/* ======================================================================
   LAMMPS NetCDF dump style
   https://github.com/pastewka/lammps-netcdf
   Lars Pastewka, lars.pastewka@kit.edu

   Copyright (2011-2013) Fraunhofer IWM
   Copyright (2014) Karlsruhe Institute of Technology

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ====================================================================== */

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

#if defined(LMP_HAS_PNETCDF)

#ifdef DUMP_CLASS

DumpStyle(netcdf/mpiio,DumpNetCDFMPIIO)

#else

#ifndef LMP_DUMP_NETCDF_MPIIO_H
#define LMP_DUMP_NETCDF_MPIIO_H

#include "dump_custom.h"

namespace LAMMPS_NS {

const int NC_MPIIO_FIELD_NAME_MAX = 100;
const int DUMP_NC_MPIIO_MAX_DIMS  = 100;

class DumpNetCDFMPIIO : public DumpCustom {
 public:
  DumpNetCDFMPIIO(class LAMMPS *, int, char **);
  virtual ~DumpNetCDFMPIIO();
  virtual void write();

 private:
  // per-atoms quantities (positions, velocities, etc.)
  struct nc_perat_t {
    int dims;                           // number of dimensions
    int field[DUMP_NC_MPIIO_MAX_DIMS];  // field indices corresponding to the dim.
    char name[NC_MPIIO_FIELD_NAME_MAX]; // field name
    int var;                            // NetCDF variable
  };

  typedef void (DumpNetCDFMPIIO::*funcptr_t)(void *);

  int framei;                           // current frame index
  int blocki;                           // current block index
  int ndata;                            // number of data blocks to expect

  bigint ntotalgr;                      // # of atoms

  int n_perat;                          // # of netcdf per-atom properties
  nc_perat_t *perat;                    // per-atom properties

  int *thermovar;                       // NetCDF variables for thermo output

  bool double_precision;                // write everything as double precision
  bool thermo;                          // write thermo output to netcdf file

  bigint n_buffer;                      // size of buffer
  int *int_buffer;                      // buffer for passing data to netcdf
  double *double_buffer;                // buffer for passing data to netcdf

  int ncid;

  int frame_dim;
  int spatial_dim;
  int Voigt_dim;
  int atom_dim;
  int cell_spatial_dim;
  int cell_angular_dim;
  int label_dim;

  int spatial_var;
  int cell_spatial_var;
  int cell_angular_var;

  int time_var;
  int cell_origin_var;
  int cell_lengths_var;
  int cell_angles_var;

  virtual void openfile();
  void closefile();
  void write_time_and_cell();
  virtual void write_data(int, double *);
  void write_prmtop();

  virtual int modify_param(int, char **);

  void ncerr(int, const char *, int);
};

}

#endif
#endif
#endif /* defined(LMP_HAS_PNETCDF) */
