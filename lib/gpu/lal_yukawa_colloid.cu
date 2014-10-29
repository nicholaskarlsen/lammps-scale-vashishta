// **************************************************************************
//                              yukawa_colloid.cu
//                             -------------------
//                           Trung Dac Nguyen (ORNL)
//
//  Device code for acceleration of the yukawa/colloid pair style
//
// __________________________________________________________________________
//    This file is part of the LAMMPS Accelerator Library (LAMMPS_AL)
// __________________________________________________________________________
//
//    begin                : 
//    email                : nguyentd@ornl.gov
// ***************************************************************************/

#ifdef NV_KERNEL

#include "lal_aux_fun1.h"
#ifndef _DOUBLE_DOUBLE
texture<float4> pos_tex;
texture<float> rad_tex;
#else
texture<int4,1> pos_tex;
texture<int2> rad_tex;
#endif

#else
#define pos_tex x_
#define rad_tex rad_
#endif

__kernel void k_yukawa_colloid(const __global numtyp4 *restrict x_, 
                               const __global numtyp *restrict rad_,
                               const __global numtyp4 *restrict coeff, 
                               const int lj_types, 
                               const __global numtyp *restrict sp_lj_in, 
                               const __global int *dev_nbor, 
                               const __global int *dev_packed, 
                               __global acctyp4 *restrict ans,
                               __global acctyp *restrict engv, 
                               const int eflag, const int vflag, const int inum,
                               const int nbor_pitch, const int t_per_atom,
                               const numtyp kappa) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);

  __local numtyp sp_lj[4];
  sp_lj[0]=sp_lj_in[0];
  sp_lj[1]=sp_lj_in[1];
  sp_lj[2]=sp_lj_in[2];
  sp_lj[3]=sp_lj_in[3];

  acctyp energy=(acctyp)0;
  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp virial[6];
  for (int i=0; i<6; i++)
    virial[i]=(acctyp)0;
  
  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    __local int n_stride;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);
  
    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    numtyp radi; fetch(radi,i,rad_tex);
    int itype=ix.w;

    numtyp factor_lj;
    for ( ; nbor<nbor_end; nbor+=n_stride) {
  
      int j=dev_packed[nbor];
      factor_lj = sp_lj[sbmask(j)];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      numtyp radj; fetch(radj,j,rad_tex);
      int jtype=jx.w;
  
      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;
        
      int mtype=itype*lj_types+jtype;
      if (rsq<coeff[mtype].z) {   
        numtyp r = ucl_sqrt(rsq);
        numtyp rinv = ucl_recip(r);
	      numtyp screening = ucl_exp(-kappa*(r-(radi+radj)));
	      numtyp force = coeff[mtype].x * screening;

	      force = factor_lj*force * rinv;
  
        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (eflag>0) {
          numtyp e=coeff[mtype].x/kappa * screening;
          energy+=factor_lj*(e-coeff[mtype].y); 
        }
        if (vflag>0) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
    store_answers(f,energy,virial,ii,inum,tid,t_per_atom,offset,eflag,vflag,
                  ans,engv);
  } // if ii
}

__kernel void k_yukawa_colloid_fast(const __global numtyp4 *restrict x_, 
                                    const __global numtyp *restrict rad_,
                                    const __global numtyp4 *restrict coeff_in, 
                                    const __global numtyp *restrict sp_lj_in,
                                    const __global int *dev_nbor, 
                                    const __global int *dev_packed, 
                                    __global acctyp4 *restrict ans, 
                                    __global acctyp *restrict engv, 
                                    const int eflag, const int vflag, 
                                    const int inum, const int nbor_pitch, 
                                    const int t_per_atom, const numtyp kappa) {
  int tid, ii, offset;
  atom_info(t_per_atom,ii,tid,offset);
  
  __local numtyp4 coeff[MAX_SHARED_TYPES*MAX_SHARED_TYPES];
  __local numtyp sp_lj[4];
  if (tid<4)
    sp_lj[tid]=sp_lj_in[tid];
  if (tid<MAX_SHARED_TYPES*MAX_SHARED_TYPES) {
    coeff[tid]=coeff_in[tid];
  }
  
  acctyp energy=(acctyp)0;
  acctyp4 f;
  f.x=(acctyp)0; f.y=(acctyp)0; f.z=(acctyp)0;
  acctyp virial[6];
  for (int i=0; i<6; i++)
    virial[i]=(acctyp)0;

  __syncthreads();
  
  if (ii<inum) {
    int nbor, nbor_end;
    int i, numj;
    __local int n_stride;
    nbor_info(dev_nbor,dev_packed,nbor_pitch,t_per_atom,ii,offset,i,numj,
              n_stride,nbor_end,nbor);

    numtyp4 ix; fetch4(ix,i,pos_tex); //x_[i];
    numtyp radi; fetch(radi,i,rad_tex);
    int iw=ix.w;
    int itype=fast_mul((int)MAX_SHARED_TYPES,iw);

    numtyp factor_lj;
    for ( ; nbor<nbor_end; nbor+=n_stride) {
  
      int j=dev_packed[nbor];
      factor_lj = sp_lj[sbmask(j)];
      j &= NEIGHMASK;

      numtyp4 jx; fetch4(jx,j,pos_tex); //x_[j];
      numtyp radj; fetch(radj,j,rad_tex);
      int mtype=itype+jx.w;

      // Compute r12
      numtyp delx = ix.x-jx.x;
      numtyp dely = ix.y-jx.y;
      numtyp delz = ix.z-jx.z;
      numtyp rsq = delx*delx+dely*dely+delz*delz;
        
      if (rsq<coeff[mtype].z) {
        numtyp r = ucl_sqrt(rsq);
        numtyp rinv = ucl_recip(r);
	      numtyp screening = ucl_exp(-kappa*(r-(radi+radj)));
	      numtyp force = coeff[mtype].x * screening;

	      force = factor_lj*force * rinv;
      
        f.x+=delx*force;
        f.y+=dely*force;
        f.z+=delz*force;

        if (eflag>0) {
          numtyp e=coeff[mtype].x/kappa * screening;
          energy+=factor_lj*(e-coeff[mtype].y); 
        }
        if (vflag>0) {
          virial[0] += delx*delx*force;
          virial[1] += dely*dely*force;
          virial[2] += delz*delz*force;
          virial[3] += delx*dely*force;
          virial[4] += delx*delz*force;
          virial[5] += dely*delz*force;
        }
      }

    } // for nbor
    store_answers(f,energy,virial,ii,inum,tid,t_per_atom,offset,eflag,vflag,
                  ans,engv);
  } // if ii
}

