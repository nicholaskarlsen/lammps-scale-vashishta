/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator

   Original Version:
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   See the README file in the top-level LAMMPS directory.

   -----------------------------------------------------------------------

   USER-CUDA Package and associated modifications:
   https://sourceforge.net/projects/lammpscuda/

   Christian Trott, christian.trott@tu-ilmenau.de
   Lars Winterfeld, lars.winterfeld@tu-ilmenau.de
   Theoretical Physics II, University of Technology Ilmenau, Germany

   See the README file in the USER-CUDA directory.

   This software is distributed under the GNU General Public License.
------------------------------------------------------------------------- */

#include <stdio.h>

#define _lj1 MY_AP(coeff1_gm)
#define _lj2 MY_AP(coeff2_gm)
#define _lj3 MY_AP(coeff3_gm)
#define _lj4 MY_AP(coeff4_gm)

#include "pair_lj_charmm_coul_charmm_cuda_cu.h"
#include "pair_lj_charmm_coul_charmm_cuda_kernel_nc.cu"

#include <time.h>

void Cuda_PairLJCharmmCoulCharmmCuda_Init(cuda_shared_data* sdata, F_CFLOAT cut_coul_innersq, F_CFLOAT denom_lj_inv, F_CFLOAT denom_coul_inv)
{
  Cuda_Pair_Init_AllStyles(sdata, 4, true, true, true);
  cudaMemcpyToSymbol(MY_AP(cut_coul_innersq_global) , &cut_coul_innersq  , sizeof(F_CFLOAT));
  cudaMemcpyToSymbol(MY_AP(denom_lj_inv) , &denom_lj_inv  , sizeof(F_CFLOAT));
  cudaMemcpyToSymbol(MY_AP(denom_coul_inv) , &denom_coul_inv  , sizeof(F_CFLOAT));

  return;
}



void Cuda_PairLJCharmmCoulCharmmCuda(cuda_shared_data* sdata, cuda_shared_neighlist* sneighlist, int eflag, int vflag,
                                     int eflag_atom, int vflag_atom, F_CFLOAT denom_lj, F_CFLOAT cut_coul_innersq, F_CFLOAT denom_coul)
{

  static  short init = 0;

  if(! init) {
    init = 1;
    Cuda_PairLJCharmmCoulCharmmCuda_Init(sdata, cut_coul_innersq, 1.0 / denom_lj, 1.0 / denom_coul);
  }

  dim3 grid, threads;
  int sharedperproc;

  Cuda_Pair_PreKernel_AllStyles(sdata, sneighlist, eflag, vflag, grid, threads, sharedperproc, true, 192);

  cudaStream_t* streams = (cudaStream_t*) CudaWrapper_returnStreams();

  if(sdata->pair.use_block_per_atom)
    Pair_Kernel_BpA<PAIR_LJ_CHARMM, COUL_CHARMM, DATA_NONE>
    <<< grid, threads, sharedperproc* sizeof(ENERGY_CFLOAT)*threads.x, streams[1]>>> (eflag, vflag, eflag_atom, vflag_atom);
  else
    Pair_Kernel_TpA<PAIR_LJ_CHARMM, COUL_CHARMM, DATA_NONE>
    <<< grid, threads, sharedperproc* sizeof(ENERGY_CFLOAT)*threads.x, streams[1]>>> (eflag, vflag, eflag_atom, vflag_atom);

  Cuda_Pair_PostKernel_AllStyles(sdata, grid, sharedperproc, eflag, vflag);
}

#undef _lj1
#undef _lj2
#undef _lj3
#undef _lj4
