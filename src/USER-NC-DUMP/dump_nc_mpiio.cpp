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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <pnetcdf.h>

#include "atom.h"
#include "comm.h"
#include "compute.h"
#include "domain.h"
#include "error.h"
#include "fix.h"
#include "group.h"
#include "input.h"
#include "math_const.h"
#include "memory.h"
#include "modify.h"
#include "update.h"
#include "universe.h"
#include "variable.h"
#include "force.h"

#include "dump_nc_mpiio.h"

using namespace LAMMPS_NS;
using namespace MathConst;

enum{INT,DOUBLE};  // same as in dump_custom.cpp

const char NC_FRAME_STR[]         = "frame";
const char NC_SPATIAL_STR[]       = "spatial";
const char NC_VOIGT_STR[]         = "Voigt";
const char NC_ATOM_STR[]          = "atom";
const char NC_CELL_SPATIAL_STR[]  = "cell_spatial";
const char NC_CELL_ANGULAR_STR[]  = "cell_angular";
const char NC_LABEL_STR[]         = "label";

const char NC_TIME_STR[]          = "time";
const char NC_CELL_ORIGIN_STR[]   = "cell_origin";
const char NC_CELL_LENGTHS_STR[]  = "cell_lengths";
const char NC_CELL_ANGLES_STR[]   = "cell_angles";

const char NC_UNITS_STR[]         = "units";
const char NC_SCALE_FACTOR_STR[]  = "scale_factor";

const int THIS_IS_A_FIX      = -1;
const int THIS_IS_A_COMPUTE  = -2;
const int THIS_IS_A_VARIABLE = -3;
const int THIS_IS_A_BIGINT   = -4;

/* ---------------------------------------------------------------------- */

#define NCERR(x) ncerr(x, NULL, __LINE__)
#define NCERRX(x, descr) ncerr(x, descr, __LINE__)

/* ---------------------------------------------------------------------- */

DumpNCMPIIO::DumpNCMPIIO(LAMMPS *lmp, int narg, char **arg) :
  DumpCustom(lmp, narg, arg)
{
  // arrays for data rearrangement

  sort_flag = 1;
  sortcol = 0;
  binary = 1;
  flush_flag = 0;

  if (multiproc)
    error->all(FLERR,"Multi-processor writes are not supported.");
  if (multifile)
    error->all(FLERR,"Multiple files are not supported.");

  perat = new nc_perat_t[nfield];

  for (int i = 0; i < nfield; i++) {
    perat[i].dims = 0;
  }

  n_perat = 0;
  for (int iarg = 5; iarg < narg; iarg++) {
    int i = iarg-5;
    int idim = 0;
    int ndims = 1;
    char mangled[1024];

    strcpy(mangled, arg[iarg]);

    // name mangling
    // in the AMBER specification
    if (!strcmp(mangled, "x") || !strcmp(mangled, "y") ||
        !strcmp(mangled, "z")) {
      idim = mangled[0] - 'x';
      ndims = 3;
      strcpy(mangled, "coordinates");
    }
    else if (!strcmp(mangled, "vx") || !strcmp(mangled, "vy") ||
             !strcmp(mangled, "vz")) {
      idim = mangled[1] - 'x';
      ndims = 3;
      strcpy(mangled, "velocities");
    }
    else if (!strcmp(mangled, "xs") || !strcmp(mangled, "ys") ||
             !strcmp(mangled, "zs")) {
      idim = mangled[0] - 'x';
      ndims = 3;
      strcpy(mangled, "scaled_coordinates");
    }
    else if (!strcmp(mangled, "xu") || !strcmp(mangled, "yu") ||
             !strcmp(mangled, "zu")) {
      idim = mangled[0] - 'x';
      ndims = 3;
      strcpy(mangled, "unwrapped_coordinates");
    }
    else if (!strcmp(mangled, "fx") || !strcmp(mangled, "fy") ||
             !strcmp(mangled, "fz")) {
      idim = mangled[1] - 'x';
      ndims = 3;
      strcpy(mangled, "forces");
    }
    else if (!strcmp(mangled, "mux") || !strcmp(mangled, "muy") ||
             !strcmp(mangled, "muz")) {
      idim = mangled[2] - 'x';
      ndims = 3;
      strcpy(mangled, "mu");
    }
    else if (!strncmp(mangled, "c_", 2)) {
      char *ptr = strchr(mangled, '[');
      if (ptr) {
        if (mangled[strlen(mangled)-1] != ']')
          error->all(FLERR,"Missing ']' in dump command");
        *ptr = '\0';
        idim = ptr[1] - '1';
        ndims = THIS_IS_A_COMPUTE;
      }
    }
    else if (!strncmp(mangled, "f_", 2)) {
      char *ptr = strchr(mangled, '[');
      if (ptr) {
        if (mangled[strlen(mangled)-1] != ']')
          error->all(FLERR,"Missing ']' in dump command");
        *ptr = '\0';
        idim = ptr[1] - '1';
        ndims = THIS_IS_A_FIX;
      }
    }

    // find mangled name
    int inc = -1;
    for (int j = 0; j < n_perat && inc < 0; j++) {
      if (!strcmp(perat[j].name, mangled)) {
        inc = j;
      }
    }

    if (inc < 0) {
      // this has not yet been defined
      inc = n_perat;
      perat[inc].dims = ndims;
      if (ndims < 0) ndims = DUMP_NC_MPIIO_MAX_DIMS;
      for (int j = 0; j < DUMP_NC_MPIIO_MAX_DIMS; j++) {
        perat[inc].field[j] = -1;
      }
      strcpy(perat[inc].name, mangled);
      n_perat++;
    }

    perat[inc].field[idim] = i;
  }

  n_perframe = 0;
  perframe = NULL;

  n_buffer = 0;
  int_buffer = NULL;
  double_buffer = NULL;

  double_precision = false;

  framei = 0;
}

/* ---------------------------------------------------------------------- */

DumpNCMPIIO::~DumpNCMPIIO()
{
  closefile();

  delete [] perat;
  if (n_perframe > 0)
    delete [] perframe;

  if (int_buffer) memory->sfree(int_buffer);
  if (double_buffer) memory->sfree(double_buffer);
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::openfile()
{
  // now the computes and fixes have been initialized, so we can query
  // for the size of vector quantities
  for (int i = 0; i < n_perat; i++) {
    if (perat[i].dims == THIS_IS_A_COMPUTE) {
      int j = -1;
      for (int k = 0; k < DUMP_NC_MPIIO_MAX_DIMS; k++) {
        if (perat[i].field[k] >= 0) {
          j = field2index[perat[i].field[0]];
        }
      }
      if (j < 0)
        error->all(FLERR,"Internal error.");
      if (!compute[j]->peratom_flag)
        error->all(FLERR,"compute does not provide per atom data");
      perat[i].dims = compute[j]->size_peratom_cols;
      if (perat[i].dims > DUMP_NC_MPIIO_MAX_DIMS)
        error->all(FLERR,"perat[i].dims > DUMP_NC_MPIIO_MAX_DIMS");
    }
    else if (perat[i].dims == THIS_IS_A_FIX) {
      int j = -1;
      for (int k = 0; k < DUMP_NC_MPIIO_MAX_DIMS; k++) {
        if (perat[i].field[k] >= 0) {
          j = field2index[perat[i].field[0]];
        }
      }
      if (j < 0)
        error->all(FLERR,"Internal error.");
      if (!fix[j]->peratom_flag)
        error->all(FLERR,"fix does not provide per atom data");
      perat[i].dims = fix[j]->size_peratom_cols;
      if (perat[i].dims > DUMP_NC_MPIIO_MAX_DIMS)
        error->all(FLERR,"perat[i].dims > DUMP_NC_MPIIO_MAX_DIMS");
    }
  }

  // get total number of atoms
  ntotalgr = group->count(igroup);

  if (append_flag && access(filename, F_OK) != -1) {
    // Fixme! Perform checks if dimensions and variables conform with
    // data structure standard.

    MPI_Offset index[NC_MAX_VAR_DIMS], count[NC_MAX_VAR_DIMS];
    double d[1];

    if (singlefile_opened) return;
    singlefile_opened = 1;

    NCERRX( ncmpi_open(MPI_COMM_WORLD, filename, NC_WRITE, MPI_INFO_NULL,
                       &ncid), filename );

    // dimensions
    NCERRX( ncmpi_inq_dimid(ncid, NC_FRAME_STR, &frame_dim), NC_FRAME_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_SPATIAL_STR, &spatial_dim),
            NC_SPATIAL_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_VOIGT_STR, &Voigt_dim), NC_VOIGT_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_ATOM_STR, &atom_dim), NC_ATOM_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_CELL_SPATIAL_STR, &cell_spatial_dim),
            NC_CELL_SPATIAL_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_CELL_ANGULAR_STR, &cell_angular_dim),
            NC_CELL_ANGULAR_STR );
    NCERRX( ncmpi_inq_dimid(ncid, NC_LABEL_STR, &label_dim), NC_LABEL_STR );

    // default variables
    NCERRX( ncmpi_inq_varid(ncid, NC_SPATIAL_STR, &spatial_var),
            NC_SPATIAL_STR );
    NCERRX( ncmpi_inq_varid(ncid, NC_CELL_SPATIAL_STR, &cell_spatial_var),
            NC_CELL_SPATIAL_STR);
    NCERRX( ncmpi_inq_varid(ncid, NC_CELL_ANGULAR_STR, &cell_angular_var),
            NC_CELL_ANGULAR_STR);

    NCERRX( ncmpi_inq_varid(ncid, NC_TIME_STR, &time_var), NC_TIME_STR );
    NCERRX( ncmpi_inq_varid(ncid, NC_CELL_ORIGIN_STR, &cell_origin_var),
            NC_CELL_ORIGIN_STR );
    NCERRX( ncmpi_inq_varid(ncid, NC_CELL_LENGTHS_STR, &cell_lengths_var),
            NC_CELL_LENGTHS_STR);
    NCERRX( ncmpi_inq_varid(ncid, NC_CELL_ANGLES_STR, &cell_angles_var),
            NC_CELL_ANGLES_STR);

    // variables specified in the input file
    for (int i = 0; i < n_perat; i++) {
      nc_type xtype;

      // Type mangling
      if (vtype[perat[i].field[0]] == INT) {
        xtype = NC_INT;
      }
      else {
        if (double_precision)
          xtype = NC_DOUBLE;
        else
          xtype = NC_FLOAT;
      }

      NCERRX( ncmpi_inq_varid(ncid, perat[i].name, &perat[i].var),
              perat[i].name );
    }

    // perframe variables
    for (int i = 0; i < n_perframe; i++) {
      NCERRX( ncmpi_inq_varid(ncid, perframe[i].name, &perframe[i].var),
              perframe[i].name );
    }

    MPI_Offset nframes;
    NCERR( ncmpi_inq_dimlen(ncid, frame_dim, &nframes) );
    // framei == -1 means append to file, == -2 means override last frame
    // Note that in the input file this translates to 'yes', '-1', etc.
    if (framei < 0 || (append_flag && framei == 0))  framei = nframes+framei+1;
    if (framei < 1)  framei = 1;
  }
  else {
    int dims[NC_MAX_VAR_DIMS];
    MPI_Offset index[NC_MAX_VAR_DIMS], count[NC_MAX_VAR_DIMS];
    double d[1];

    if (singlefile_opened) return;
    singlefile_opened = 1;

    NCERRX( ncmpi_create(MPI_COMM_WORLD, filename, NC_64BIT_OFFSET,
                         MPI_INFO_NULL, &ncid), filename );

    // dimensions
    NCERRX( ncmpi_def_dim(ncid, NC_FRAME_STR, NC_UNLIMITED, &frame_dim),
            NC_FRAME_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_SPATIAL_STR, 3, &spatial_dim),
            NC_SPATIAL_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_VOIGT_STR, 6, &Voigt_dim),
            NC_VOIGT_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_ATOM_STR, ntotalgr, &atom_dim),
            NC_ATOM_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_CELL_SPATIAL_STR, 3, &cell_spatial_dim),
            NC_CELL_SPATIAL_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_CELL_ANGULAR_STR, 3, &cell_angular_dim),
            NC_CELL_ANGULAR_STR );
    NCERRX( ncmpi_def_dim(ncid, NC_LABEL_STR, 10, &label_dim),
            NC_LABEL_STR );

    // default variables
    dims[0] = spatial_dim;
    NCERRX( ncmpi_def_var(ncid, NC_SPATIAL_STR, NC_CHAR, 1, dims, &spatial_var),
            NC_SPATIAL_STR );
    NCERRX( ncmpi_def_var(ncid, NC_CELL_SPATIAL_STR, NC_CHAR, 1, dims,
                          &cell_spatial_var), NC_CELL_SPATIAL_STR );
    dims[0] = spatial_dim;
    dims[1] = label_dim;
    NCERRX( ncmpi_def_var(ncid, NC_CELL_ANGULAR_STR, NC_CHAR, 2, dims,
                          &cell_angular_var), NC_CELL_ANGULAR_STR );

    dims[0] = frame_dim;
    NCERRX( ncmpi_def_var(ncid, NC_TIME_STR, NC_DOUBLE, 1, dims, &time_var),
            NC_TIME_STR);
    dims[0] = frame_dim;
    dims[1] = cell_spatial_dim;
    NCERRX( ncmpi_def_var(ncid, NC_CELL_ORIGIN_STR, NC_DOUBLE, 2, dims,
                          &cell_origin_var), NC_CELL_ORIGIN_STR );
    NCERRX( ncmpi_def_var(ncid, NC_CELL_LENGTHS_STR, NC_DOUBLE, 2, dims,
                          &cell_lengths_var), NC_CELL_LENGTHS_STR );
    dims[0] = frame_dim;
    dims[1] = cell_angular_dim;
    NCERRX( ncmpi_def_var(ncid, NC_CELL_ANGLES_STR, NC_DOUBLE, 2, dims,
                          &cell_angles_var), NC_CELL_ANGLES_STR );

    // variables specified in the input file
    dims[0] = frame_dim;
    dims[1] = atom_dim;
    dims[2] = spatial_dim;

    for (int i = 0; i < n_perat; i++) {
      nc_type xtype;

      // Type mangling
      if (vtype[perat[i].field[0]] == INT) {
        xtype = NC_INT;
      }
      else {
        if (double_precision)
          xtype = NC_DOUBLE;
        else
          xtype = NC_FLOAT;
      }

      if (perat[i].dims == 6) {
        // this is a tensor in Voigt notation
        dims[2] = Voigt_dim;
        NCERRX( ncmpi_def_var(ncid, perat[i].name, xtype, 3, dims,
                                &perat[i].var), perat[i].name );
      }
      else if (perat[i].dims == 3) {
        // this is a vector, we need to store x-, y- and z-coordinates
        dims[2] = spatial_dim;
        NCERRX( ncmpi_def_var(ncid, perat[i].name, xtype, 3, dims,
                              &perat[i].var), perat[i].name );
      }
      else if (perat[i].dims == 1) {
        NCERRX( ncmpi_def_var(ncid, perat[i].name, xtype, 2, dims,
                              &perat[i].var), perat[i].name );
      }
      else {
        char errstr[1024];
        sprintf(errstr, "%i dimensions for '%s'. Not sure how to write "
                "this to the NetCDF trajectory file.", perat[i].dims,
                perat[i].name);
        error->all(FLERR,errstr);
      }
    }

    // perframe variables
    for (int i = 0; i < n_perframe; i++) {
      if (perframe[i].type == THIS_IS_A_BIGINT) {
        NCERRX( ncmpi_def_var(ncid, perframe[i].name, NC_INT, 1, dims,
                              &perframe[i].var), perframe[i].name );
      }
      else {
        NCERRX( ncmpi_def_var(ncid, perframe[i].name, NC_DOUBLE, 1, dims,
                              &perframe[i].var), perframe[i].name );
      }
    }

    // attributes
    NCERR( ncmpi_put_att_text(ncid, NC_GLOBAL, "Conventions",
                              5, "AMBER") );
    NCERR( ncmpi_put_att_text(ncid, NC_GLOBAL, "ConventionVersion",
                              3, "1.0") );

    NCERR( ncmpi_put_att_text(ncid, NC_GLOBAL, "program",
                              6, "LAMMPS") );
    NCERR( ncmpi_put_att_text(ncid, NC_GLOBAL, "programVersion",
                              strlen(universe->version), universe->version) );

    // units
    if (!strcmp(update->unit_style, "lj")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                2, "lj") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                2, "lj") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                2, "lj") );
    }
    else if (!strcmp(update->unit_style, "real")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                11, "femtosecond") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                8, "Angstrom") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                8, "Angstrom") );
    }
    else if (!strcmp(update->unit_style, "metal")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                10, "picosecond") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                8, "Angstrom") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                8, "Angstrom") );
    }
    else if (!strcmp(update->unit_style, "si")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                6, "second") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                5, "meter") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                5, "meter") );
    }
    else if (!strcmp(update->unit_style, "cgs")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                6, "second") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                10, "centimeter") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                10, "centimeter") );
    }
    else if (!strcmp(update->unit_style, "electron")) {
      NCERR( ncmpi_put_att_text(ncid, time_var, NC_UNITS_STR,
                                11, "femtosecond") );
      NCERR( ncmpi_put_att_text(ncid, cell_origin_var, NC_UNITS_STR,
                                4, "Bohr") );
      NCERR( ncmpi_put_att_text(ncid, cell_lengths_var, NC_UNITS_STR,
                                4, "Bohr") );
    }
    else {
      char errstr[1024];
      sprintf(errstr, "Unsupported unit style '%s'", update->unit_style);
      error->all(FLERR,errstr);
    }

    NCERR( ncmpi_put_att_text(ncid, cell_angles_var, NC_UNITS_STR,
                              6, "degree") );

    d[0] = update->dt;
    NCERR( ncmpi_put_att_double(ncid, time_var, NC_SCALE_FACTOR_STR,
                                NC_DOUBLE, 1, d) );
    d[0] = 1.0;
    NCERR( ncmpi_put_att_double(ncid, cell_origin_var, NC_SCALE_FACTOR_STR,
                                NC_DOUBLE, 1, d) );
    d[0] = 1.0;
    NCERR( ncmpi_put_att_double(ncid, cell_lengths_var, NC_SCALE_FACTOR_STR,
                                NC_DOUBLE, 1, d) );

    /*
     * Finished with definition
     */

    NCERR( ncmpi_enddef(ncid) );

    /*
     * Write label variables
     */

    NCERR( ncmpi_begin_indep_data(ncid) );

    if (filewriter) {
      NCERR( ncmpi_put_var_text(ncid, spatial_var, "xyz") );
      NCERR( ncmpi_put_var_text(ncid, cell_spatial_var, "abc") );
      index[0] = 0;
      index[1] = 0;
      count[0] = 1;
      count[1] = 5;
      NCERR( ncmpi_put_vara_text(ncid, cell_angular_var, index, count,
                                 "alpha") );
      index[0] = 1;
      count[1] = 4;
      NCERR( ncmpi_put_vara_text(ncid, cell_angular_var, index, count,
                                 "beta") );
      index[0] = 2;
      count[1] = 5;
      NCERR( ncmpi_put_vara_text(ncid, cell_angular_var, index, count,
                                 "gamma") );
    }

    NCERR( ncmpi_end_indep_data(ncid) );

    framei = 1;
  }
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::closefile()
{
  if (singlefile_opened) {
    NCERR( ncmpi_close(ncid) );
    singlefile_opened = 0;
    // append next time DumpNCMPIIO::openfile is called
    append_flag = 1;
    // write to next frame upon next open
    framei++;
  }
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::write()
{
  // open file

  openfile();

  // need to write per-frame (global) properties here since they may come
  // from computes. write_header below is only called from the writing
  // processes, but modify->compute[j]->compute_* must be called from all
  // processes.

  MPI_Offset start[2];

  start[0] = framei-1;
  start[1] = 0;

  NCERR( ncmpi_begin_indep_data(ncid) );

  for (int i = 0; i < n_perframe; i++) {

    if (perframe[i].type == THIS_IS_A_BIGINT) {
      bigint data;
      (this->*perframe[i].compute)((void*) &data);

      if (filewriter)
#if defined(LAMMPS_SMALLBIG) || defined(LAMMPS_BIGBIG)
        NCERR( ncmpi_put_var1_long(ncid, perframe[i].var, start, &data) );
#else
        NCERR( ncmpi_put_var1_int(ncid, perframe[i].var, start, &data) );
#endif
    }
    else {
      double data;
      int j = perframe[i].index;
      int idim = perframe[i].dim;

      if (perframe[i].type == THIS_IS_A_COMPUTE) {
        if (idim >= 0) {
          modify->compute[j]->compute_vector();
          data = modify->compute[j]->vector[idim];
        }
        else
          data = modify->compute[j]->compute_scalar();
      }
      else if (perframe[i].type == THIS_IS_A_FIX) {
        if (idim >= 0) {
          data = modify->fix[j]->compute_vector(idim);
        }
        else
          data = modify->fix[j]->compute_scalar();
      }
      else if (perframe[i].type == THIS_IS_A_VARIABLE) {
        j = input->variable->find(perframe[i].id);
        data = input->variable->compute_equal(j);
      }

      if (filewriter)
        NCERR( ncmpi_put_var1_double(ncid, perframe[i].var, start, &data) );
    }
  }

  // write timestep header

  write_time_and_cell();

  NCERR( ncmpi_end_indep_data(ncid) );

  // nme = # of dump lines this proc contributes to dump

  nme = count();
  int *block_sizes = new int[comm->nprocs];
  MPI_Allgather(&nme, 1, MPI_INT, block_sizes, 1, MPI_INT, MPI_COMM_WORLD);
  blocki = 0;
  for (int i = 0; i < comm->me; i++)  blocki += block_sizes[i];
  delete [] block_sizes;

  // insure buf is sized for packing and communicating
  // use nme to insure filewriter proc can receive info from others
  // limit nme*size_one to int since used as arg in MPI calls

  if (nme > maxbuf) {
    if ((bigint) nme * size_one > MAXSMALLINT)
      error->all(FLERR,"Too much per-proc info for dump");
    maxbuf = nme;
    memory->destroy(buf);
    memory->create(buf,maxbuf*size_one,"dump:buf");
  }

  // pack my data into buf

  pack(NULL);

  // each process writes its data

  write_data(nme, buf);

  // close file. this ensures data is flushed and minimizes data corruption

  closefile();
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::write_time_and_cell()
{
  MPI_Offset start[2];

  start[0] = framei-1;
  start[1] = 0;

  MPI_Offset count[2];
  double time, cell_origin[3], cell_lengths[3], cell_angles[3];

  time = update->ntimestep;
  if (domain->triclinic == 0) {
    cell_origin[0] = domain->boxlo[0];
    cell_origin[1] = domain->boxlo[1];
    cell_origin[2] = domain->boxlo[2];

    cell_lengths[0] = domain->xprd;
    cell_lengths[1] = domain->yprd;
    cell_lengths[2] = domain->zprd;

    cell_angles[0] = 90;
    cell_angles[1] = 90;
    cell_angles[2] = 90;
  }
  else {
    double cosalpha, cosbeta, cosgamma;
    double *h = domain->h;

    cell_origin[0] = domain->boxlo[0];
    cell_origin[1] = domain->boxlo[1];
    cell_origin[2] = domain->boxlo[2];

    cell_lengths[0] = domain->xprd;
    cell_lengths[1] = sqrt(h[1]*h[1]+h[5]*h[5]);
    cell_lengths[2] = sqrt(h[2]*h[2]+h[3]*h[3]+h[4]*h[4]);

    cosalpha = (h[5]*h[4]+h[1]*h[3])/
      sqrt((h[1]*h[1]+h[5]*h[5])*(h[2]*h[2]+h[3]*h[3]+h[4]*h[4]));
    cosbeta = h[4]/sqrt(h[2]*h[2]+h[3]*h[3]+h[4]*h[4]);
    cosgamma = h[5]/sqrt(h[1]*h[1]+h[5]*h[5]);

    cell_angles[0] = acos(cosalpha)*180.0/MY_PI;
    cell_angles[1] = acos(cosbeta)*180.0/MY_PI;
    cell_angles[2] = acos(cosgamma)*180.0/MY_PI;
  }

  // Recent AMBER conventions say that nonperiodic boundaries should have
  // 'cell_lengths' set to zero.
  for (int dim = 0; dim < 3; dim++) {
    if (!domain->periodicity[dim])
      cell_lengths[dim] = 0.0;
  }

  count[0] = 1;
  count[1] = 3;
  if (filewriter) {
    NCERR( ncmpi_put_var1_double(ncid, time_var, start, &time) );
    NCERR( ncmpi_put_vara_double(ncid, cell_origin_var, start, count,
                                 cell_origin) );
    NCERR( ncmpi_put_vara_double(ncid, cell_lengths_var, start, count,
                                 cell_lengths) );
    NCERR( ncmpi_put_vara_double(ncid, cell_angles_var, start, count,
                                 cell_angles) );
  }
}


/* ----------------------------------------------------------------------
   write data lines to file in a block-by-block style
   write head of block (mass & element name) only if has atoms of the type
------------------------------------------------------------------------- */

void DumpNCMPIIO::write_data(int n, double *mybuf)
{
  MPI_Offset start[NC_MAX_VAR_DIMS], count[NC_MAX_VAR_DIMS];
  MPI_Offset stride[NC_MAX_VAR_DIMS];

  if (!int_buffer) {
    n_buffer = std::max(1, n);
    int_buffer = (int *)
      memory->smalloc(n_buffer*sizeof(int), "DumpNCMPIIO::int_buffer");
    double_buffer = (double *)
      memory->smalloc(n_buffer*sizeof(double), "DumpNCMPIIO::double_buffer");
  }

  if (n > n_buffer) {
    n_buffer = std::max(1, n);
    int_buffer = (int *)
      memory->srealloc(int_buffer, n_buffer*sizeof(int),
                       "DumpNCMPIIO::int_buffer");
    double_buffer = (double *)
      memory->srealloc(double_buffer, n_buffer*sizeof(double),
                       "DumpNCMPIIO::double_buffer");
  }

  start[0] = framei-1;
  start[1] = blocki;
  start[2] = 0;

  if (n == 0) {
    /* If there is no data, we need to make sure the start values don't exceed
       dimension bounds. Just set them to zero. */
    start[1] = 0;
  }

  count[0] = 1;
  count[1] = n;
  count[2] = 1;

  stride[0] = 1;
  stride[1] = 1;
  stride[2] = 3;

  for (int i = 0; i < n_perat; i++) {
    int iaux = perat[i].field[0];
    if (iaux < 0 || iaux >= size_one) {
      char errmsg[1024];
      sprintf(errmsg, "Internal error: name = %s, iaux = %i, "
              "size_one = %i", perat[i].name, iaux, size_one);
      error->one(FLERR,errmsg);
    }

    if (vtype[iaux] == INT) {
      // integers
      if (perat[i].dims > 1) {

        for (int idim = 0; idim < perat[i].dims; idim++) {
          iaux = perat[i].field[idim];

          if (iaux >= 0) {
            if (iaux >= size_one) {
              char errmsg[1024];
              sprintf(errmsg, "Internal error: name = %s, iaux = %i, "
                      "size_one = %i", perat[i].name, iaux, size_one);
              error->one(FLERR,errmsg);
            }

            for (int j = 0; j < n; j++, iaux+=size_one) {
              int_buffer[j] = mybuf[iaux];
            }

            start[2] = idim;
            NCERRX( ncmpi_put_vars_int_all(ncid, perat[i].var, start, count,
                                           stride, int_buffer), perat[i].name );
          }
        }
      }
      else {
        for (int j = 0; j < n; j++, iaux+=size_one) {
            int_buffer[j] = mybuf[iaux];
        }

        NCERRX( ncmpi_put_vara_int_all(ncid, perat[i].var, start, count,
                                       int_buffer), perat[i].name );
      }
    }
    else {
      // doubles
      if (perat[i].dims > 1) {

        for (int idim = 0; idim < perat[i].dims; idim++) {
          iaux = perat[i].field[idim];

          if (iaux >= 0) {
            if (iaux >= size_one) {
              char errmsg[1024];
              sprintf(errmsg, "Internal error: name = %s, iaux = %i, "
                      "size_one = %i", perat[i].name, iaux, size_one);
              error->one(FLERR,errmsg);
            }

            for (int j = 0; j < n; j++, iaux+=size_one) {
                double_buffer[j] = mybuf[iaux];
            }

            start[2] = idim;
            NCERRX( ncmpi_put_vars_double_all(ncid, perat[i].var, start, count,
                                              stride, double_buffer), perat[i].name );
          }
        }
      }
      else {
        for (int j = 0; j < n; j++, iaux+=size_one) {
            double_buffer[j] = mybuf[iaux];
        }

        NCERRX( ncmpi_put_vara_double_all(ncid, perat[i].var, start, count,
                                          double_buffer), perat[i].name );
      }
    }
  }
}

/* ---------------------------------------------------------------------- */

int DumpNCMPIIO::modify_param(int narg, char **arg)
{
  int iarg = 0;
  if (strcmp(arg[iarg],"double") == 0) {
    iarg++;
    if (iarg >= narg)
      error->all(FLERR,"expected 'yes' or 'no' after 'double' keyword.");
    if (strcmp(arg[iarg],"yes") == 0) {
      double_precision = true;
    }
    else if (strcmp(arg[iarg],"no") == 0) {
      double_precision = false;
    }
    else error->all(FLERR,"expected 'yes' or 'no' after 'double' keyword.");
    iarg++;
    return 2;
  }
  else if (strcmp(arg[iarg],"at") == 0) {
    iarg++;
    framei = force->inumeric(FLERR,arg[iarg]);
    if (framei < 0)  framei--;
    iarg++;
    return 2;
  }
  else if (strcmp(arg[iarg],"global") == 0) {
    // "perframe" quantities, i.e. not per-atom stuff

    iarg++;

    n_perframe = narg-iarg;
    perframe = new nc_perframe_t[n_perframe];

    for (int i = 0; iarg < narg; iarg++, i++) {
      int n;
      char *suffix;

      if (!strcmp(arg[iarg],"step")) {
        perframe[i].type = THIS_IS_A_BIGINT;
        perframe[i].compute = &DumpNCMPIIO::compute_step;
        strcpy(perframe[i].name, arg[iarg]);
      }
      else if (!strcmp(arg[iarg],"elapsed")) {
        perframe[i].type = THIS_IS_A_BIGINT;
        perframe[i].compute = &DumpNCMPIIO::compute_elapsed;
        strcpy(perframe[i].name, arg[iarg]);
      }
      else if (!strcmp(arg[iarg],"elaplong")) {
        perframe[i].type = THIS_IS_A_BIGINT;
        perframe[i].compute = &DumpNCMPIIO::compute_elapsed_long;
        strcpy(perframe[i].name, arg[iarg]);
      }
      else {

        n = strlen(arg[iarg]);

        if (n > 2) {
          suffix = new char[n-1];
          strcpy(suffix, arg[iarg]+2);
        }
        else {
          char errstr[1024];
          sprintf(errstr, "perframe quantity '%s' must thermo quantity or "
                  "compute, fix or variable", arg[iarg]);
          error->all(FLERR,errstr);
        }

        if (!strncmp(arg[iarg], "c_", 2)) {
          int idim = -1;
          char *ptr = strchr(suffix, '[');

          if (ptr) {
            if (suffix[strlen(suffix)-1] != ']')
              error->all(FLERR,"Missing ']' in dump modify command");
            *ptr = '\0';
            idim = ptr[1] - '1';
          }

          n = modify->find_compute(suffix);
          if (n < 0)
            error->all(FLERR,"Could not find dump modify compute ID");
          if (modify->compute[n]->peratom_flag != 0)
            error->all(FLERR,"Dump modify compute ID computes per-atom info");
          if (idim >= 0 && modify->compute[n]->vector_flag == 0)
            error->all(FLERR,"Dump modify compute ID does not compute vector");
          if (idim < 0 && modify->compute[n]->scalar_flag == 0)
            error->all(FLERR,"Dump modify compute ID does not compute scalar");

          perframe[i].type = THIS_IS_A_COMPUTE;
          perframe[i].dim = idim;
          perframe[i].index = n;
          strcpy(perframe[i].name, arg[iarg]);
        }
        else if (!strncmp(arg[iarg], "f_", 2)) {
          int idim = -1;
          char *ptr = strchr(suffix, '[');

          if (ptr) {
            if (suffix[strlen(suffix)-1] != ']')
              error->all(FLERR,"Missing ']' in dump modify command");
            *ptr = '\0';
            idim = ptr[1] - '1';
          }

          n = modify->find_fix(suffix);
          if (n < 0)
            error->all(FLERR,"Could not find dump modify fix ID");
          if (modify->fix[n]->peratom_flag != 0)
            error->all(FLERR,"Dump modify fix ID computes per-atom info");
          if (idim >= 0 && modify->fix[n]->vector_flag == 0)
            error->all(FLERR,"Dump modify fix ID does not compute vector");
          if (idim < 0 && modify->fix[n]->scalar_flag == 0)
            error->all(FLERR,"Dump modify fix ID does not compute vector");

          perframe[i].type = THIS_IS_A_FIX;
          perframe[i].dim = idim;
          perframe[i].index = n;
          strcpy(perframe[i].name, arg[iarg]);
        }
        else if (!strncmp(arg[iarg], "v_", 2)) {
          n = input->variable->find(suffix);
          if (n < 0)
            error->all(FLERR,"Could not find dump modify variable ID");
          if (!input->variable->equalstyle(n))
            error->all(FLERR,"Dump modify variable must be of style equal");

          perframe[i].type = THIS_IS_A_VARIABLE;
          perframe[i].dim = 1;
          perframe[i].index = n;
          strcpy(perframe[i].name, arg[iarg]);
          strcpy(perframe[i].id, suffix);
        }
        else {
          char errstr[1024];
          sprintf(errstr, "perframe quantity '%s' must be compute, fix or "
                  "variable", arg[iarg]);
          error->all(FLERR,errstr);
        }

        delete [] suffix;

      }
    }

    return narg;
  } else return 0;
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::ncerr(int err, const char *descr, int line)
{
  if (err != NC_NOERR) {
    char errstr[1024];
    if (descr) {
      sprintf(errstr, "NetCDF failed with error '%s' (while accessing '%s') "
              " in line %i of %s.", ncmpi_strerror(err), descr, line, __FILE__);
    }
    else {
      sprintf(errstr, "NetCDF failed with error '%s' in line %i of %s.",
              ncmpi_strerror(err), line, __FILE__);
    }
    error->one(FLERR,errstr);
  }
}

/* ----------------------------------------------------------------------
   one method for every keyword thermo can output
   called by compute() or evaluate_keyword()
   compute will have already been called
   set ivalue/dvalue/bivalue if value is int/double/bigint
   customize a new keyword by adding a method
------------------------------------------------------------------------- */

void DumpNCMPIIO::compute_step(void *r)
{
  *((bigint *) r) = update->ntimestep;
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::compute_elapsed(void *r)
{
  *((bigint *) r) = update->ntimestep - update->firststep;
}

/* ---------------------------------------------------------------------- */

void DumpNCMPIIO::compute_elapsed_long(void *r)
{
  *((bigint *) r) = update->ntimestep - update->beginstep;
}

#endif /* defined(LMP_HAS_PNETCDF) */
