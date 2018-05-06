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

/* ----------------------------------------------------------------------
   Contributing author: Mike Brown (ORNL)
------------------------------------------------------------------------- */

#ifndef LMP_GPU_EXTRA_H
#define LMP_GPU_EXTRA_H

#include "modify.h"
#include "error.h"

namespace GPU_EXTRA {

  inline void check_flag(int error_flag, LAMMPS_NS::Error *error,
                         MPI_Comm &world) {
    int all_success;
    MPI_Allreduce(&error_flag, &all_success, 1, MPI_INT, MPI_MIN, world);
    if (all_success != 0) {
      if (all_success == -1)
        error->all(FLERR,
                   "The package gpu command is required for gpu styles");
      else if (all_success == -2)
        error->all(FLERR,
                   "Could not find/initialize a specified accelerator device");
      else if (all_success == -3)
        error->all(FLERR,"Insufficient memory on accelerator");
      else if (all_success == -4)
        error->all(FLERR,"GPU library not compiled for this accelerator");
      else if (all_success == -5)
        error->all(FLERR,
                   "Double precision is not supported on this accelerator");
      else if (all_success == -6)
        error->all(FLERR,"Unable to initialize accelerator for use");
      else if (all_success == -7)
        error->all(FLERR,
                   "Accelerator sharing is not currently supported on system");
      else if (all_success == -8)
        error->all(FLERR,
                   "GPU particle split must be set to 1 for this pair style.");
      else if (all_success == -9)
        error->all(FLERR,
                   "CPU neighbor lists must be used for ellipsoid/sphere mix.");
      else if (all_success == -10)
        error->all(FLERR,
                   "Invalid threads_per_atom specified.");
      else if (all_success == -11)
        error->all(FLERR,
                   "Invalid custom OpenCL parameter string.");
      else
        error->all(FLERR,"Unknown error in GPU library");
    }
  }

  inline void gpu_ready(LAMMPS_NS::Modify *modify, LAMMPS_NS::Error *error) {
    int ifix = modify->find_fix("package_gpu");
    if (ifix < 0)
      error->all(FLERR,"The package gpu command is required for gpu styles");
  }
}

#endif

/* ERROR/WARNING messages:

E: The package gpu command is required for gpu styles

Self-explanatory.

E: Could not find/initialize a specified accelerator device

Could not initialize at least one of the devices specified for the gpu
package

E: Insufficient memory on accelerator

There is insufficient memory on one of the devices specified for the gpu
package

E: GPU library not compiled for this accelerator

Self-explanatory.

E: Double precision is not supported on this accelerator

Self-explanatory

E: Unable to initialize accelerator for use

There was a problem initializing an accelerator for the gpu package

E: Accelerator sharing is not currently supported on system

Multiple MPI processes cannot share the accelerator on your
system. For NVIDIA GPUs, see the nvidia-smi command to change this
setting.

E: GPU particle split must be set to 1 for this pair style.

For this pair style, you cannot run part of the force calculation on
the host.  See the package command.

E: CPU neighbor lists must be used for ellipsoid/sphere mix.

When using Gay-Berne or RE-squared pair styles with both ellipsoidal and
spherical particles, the neighbor list must be built on the CPU

E: Invalid threads_per_atom specified.

For 3-body potentials on the GPU, the threads_per_atom setting cannot be
greater than 4 for NVIDIA GPUs.

E: Invalid custom OpenCL parameter string.

There are not enough or too many parameters in the custom string for package
GPU.

E: Unknown error in GPU library

Self-explanatory.

*/
