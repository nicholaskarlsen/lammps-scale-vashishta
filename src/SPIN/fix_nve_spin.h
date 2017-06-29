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

#ifdef FIX_CLASS

FixStyle(nve/spin,FixNVESpin)

#else

#ifndef LMP_FIX_NVE_SPIN_H
#define LMP_FIX_NVE_SPIN_H

#include "fix_nve.h"

namespace LAMMPS_NS {

class FixNVESpin : public FixNVE {
	
 public:
  FixNVESpin(class LAMMPS *, int, char **);
  virtual ~FixNVESpin();
  void init();
  virtual void initial_integrate(int);
  void AdvanceSingleSpin(int, double, double **, double **);
  virtual void final_integrate();
  void ComputeSpinInteractions();   
  void ComputeSpinInteractionsNeigh(int);   
  
#define SECTORING
#if defined SECTORING
  void sectoring();
  int coords2sector(double *);
#endif  

 protected:
  int extra;
  double dts;
  
  int exch_flag, dmi_flag, me_flag;
  int zeeman_flag, aniso_flag;
  int tdamp_flag, temp_flag;

  class PairSpin *lockpairspin;
  class FixForceSpin *lockforcespin;
  class FixLangevinSpin *locklangevinspin; 

  double *spi, *spj, *fmi, *fmj; //Temp var. for compute
  double *xi;
 
#if defined SECTORING 
  int nsectors;
  int *sec;
  int *seci;
  double *rsec;
#endif

//#define SECTOR_PRINT
#if defined SECTOR_PRINT
  FILE* file_sect=NULL;
#endif
};

}

#endif
#endif

/* ERROR/WARNING messages:

E: Illegal ... command

Self-explanatory.  Check the input script syntax and compare to the
documentation for the command.  You can use -echo screen as a
command-line option when running LAMMPS to see the offending line.

E: Fix nve/sphere requires atom style sphere

Self-explanatory.

E: Fix nve/sphere update dipole requires atom attribute mu

An atom style with this attribute is needed.

E: Fix nve/sphere requires extended particles

This fix can only be used for particles of a finite size.
 
E: Fix nve/sphere dlm must be used with update dipole
 
The DLM algorithm can only be used in conjunction with update dipole.


*/
