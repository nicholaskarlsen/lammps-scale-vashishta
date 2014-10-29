/***************************************************************************
                             yukawa_colloid_ext.cpp
                             -------------------
                            Trung Dac Nguyen (ORNL)

  Functions for LAMMPS access to colloid acceleration routines.

 __________________________________________________________________________
    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
 __________________________________________________________________________

    begin                : 
    email                : nguyentd@ornl.gov
 ***************************************************************************/

#include <iostream>
#include <cassert>
#include <math.h>

#include "lal_yukawa_colloid.h"

using namespace std;
using namespace LAMMPS_AL;

static YukawaColloid<PRECISION,ACC_PRECISION> YKCOLLMF;

// ---------------------------------------------------------------------------
// Allocate memory on host and device and copy constants to device
// ---------------------------------------------------------------------------
int ykcolloid_gpu_init(const int ntypes, double **cutsq, double **host_a, 
                       double **host_offset, double *special_lj, const int inum,
                       const int nall, const int max_nbors,  const int maxspecial,
                       const double cell_size, int &gpu_mode, FILE *screen, 
                       const double kappa) {
  YKCOLLMF.clear();
  gpu_mode=YKCOLLMF.device->gpu_mode();
  double gpu_split=YKCOLLMF.device->particle_split();
  int first_gpu=YKCOLLMF.device->first_device();
  int last_gpu=YKCOLLMF.device->last_device();
  int world_me=YKCOLLMF.device->world_me();
  int gpu_rank=YKCOLLMF.device->gpu_rank();
  int procs_per_gpu=YKCOLLMF.device->procs_per_gpu();

  YKCOLLMF.device->init_message(screen,"yukawa/colloid",first_gpu,last_gpu);

  bool message=false;
  if (YKCOLLMF.device->replica_me()==0 && screen)
    message=true;

  if (message) {
    fprintf(screen,"Initializing Device and compiling on process 0...");
    fflush(screen);
  }

  int init_ok=0;
  if (world_me==0)
    init_ok=YKCOLLMF.init(ntypes, cutsq, host_a, host_offset, special_lj, 
                          inum, nall, 300, maxspecial, cell_size, gpu_split, 
                          screen, kappa);

  YKCOLLMF.device->world_barrier();
  if (message)
    fprintf(screen,"Done.\n");

  for (int i=0; i<procs_per_gpu; i++) {
    if (message) {
      if (last_gpu-first_gpu==0)
        fprintf(screen,"Initializing Device %d on core %d...",first_gpu,i);
      else
        fprintf(screen,"Initializing Devices %d-%d on core %d...",first_gpu,
                last_gpu,i);
      fflush(screen);
    }
    if (gpu_rank==i && world_me!=0)
      init_ok=YKCOLLMF.init(ntypes, cutsq, host_a, host_offset, special_lj, 
                            inum, nall, 300, maxspecial, cell_size, gpu_split, 
                            screen, kappa);

    YKCOLLMF.device->gpu_barrier();
    if (message) 
      fprintf(screen,"Done.\n");
  }
  if (message)
    fprintf(screen,"\n");

  if (init_ok==0)
    YKCOLLMF.estimate_gpu_overhead();
  return init_ok;
}

void ykcolloid_gpu_clear() {
  YKCOLLMF.clear();
}

int ** ykcolloid_gpu_compute_n(const int ago, const int inum_full,
                               const int nall, double **host_x, int *host_type,
                               double *sublo, double *subhi, tagint *tag, int **nspecial,
                               tagint **special, const bool eflag, const bool vflag,
                               const bool eatom, const bool vatom, int &host_start,
                               int **ilist, int **jnum, const double cpu_time,
                               bool &success, double *host_rad) {
  return YKCOLLMF.compute(ago, inum_full, nall, host_x, host_type, sublo,
                          subhi, tag, nspecial, special, eflag, vflag, eatom,
                          vatom, host_start, ilist, jnum, cpu_time, success,
                          host_rad);
}  
			
void ykcolloid_gpu_compute(const int ago, const int inum_full, 
                           const int nall, double **host_x, int *host_type, 
                           int *ilist, int *numj, int **firstneigh, 
                           const bool eflag, const bool vflag,
                           const bool eatom, const bool vatom, int &host_start,
                           const double cpu_time, bool &success, double *host_rad) {
  YKCOLLMF.compute(ago,inum_full,nall,host_x,host_type,ilist,numj,
                   firstneigh,eflag,vflag,eatom,vatom,host_start,cpu_time,
                   success,host_rad);
}

double ykcolloid_gpu_bytes() {
  return YKCOLLMF.host_memory_usage();
}


