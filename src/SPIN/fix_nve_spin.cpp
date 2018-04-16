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

/* ------------------------------------------------------------------------
   Contributing authors: Julien Tranchida (SNL)
                         Aidan Thompson (SNL)
   
   Please cite the related publication:
   Tranchida, J., Plimpton, S. J., Thibaudeau, P., & Thompson, A. P. (2018). 
   Massively parallel symplectic algorithm for coupled magnetic spin dynamics 
   and molecular dynamics. arXiv preprint arXiv:1801.10233.
------------------------------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "atom.h"
#include "atom_vec.h"
#include "error.h"
#include "fix_precession_spin.h"
#include "fix_nve_spin.h"
#include "fix_langevin_spin.h"
#include "force.h"
#include "math_vector.h"
#include "math_extra.h"
#include "math_const.h"
#include "memory.h"
#include "modify.h" 
#include "neighbor.h"
#include "neigh_list.h"
#include "pair.h"
#include "pair_hybrid.h"
#include "pair_spin.h"
#include "respa.h"
#include "update.h"

using namespace LAMMPS_NS;
using namespace FixConst;
using namespace MathConst;
using namespace MathExtra;

enum{NONE};

/* ---------------------------------------------------------------------- */

FixNVESpin::FixNVESpin(LAMMPS *lmp, int narg, char **arg) :
  Fix(lmp, narg, arg), 
  rsec(NULL), stack_head(NULL), stack_foot(NULL), 
  backward_stacks(NULL), forward_stacks(NULL),
  lockpairspin(NULL)
{
  
  if (narg < 4) error->all(FLERR,"Illegal fix/NVE/spin command");  
  
  time_integrate = 1;
  
  sector_flag = NONE;
  lattice_flag = 1;

  nlocal_max = 0;

  // checking if map array or hash is defined

  if (atom->map_style == 0)
    error->all(FLERR,"Fix NVE/spin requires an atom map, see atom_modify");

  // defining sector_flag 

  int nprocs_tmp = comm->nprocs;
  if (nprocs_tmp == 1) {
    sector_flag = 0;
  } else if (nprocs_tmp >= 1) {
    sector_flag = 1;
  } else error->all(FLERR,"Illegal fix/NVE/spin command");

  // defining lattice_flag 

  int iarg = 3;
  while (iarg < narg) { 
    if (strcmp(arg[iarg],"lattice") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal fix/NVE/spin command");
      if (strcmp(arg[iarg+1],"no") == 0) lattice_flag = 0;
      else if (strcmp(arg[iarg+1],"yes") == 0) lattice_flag = 1;
      else error->all(FLERR,"Illegal fix/NVE/spin command");
      iarg += 2;
    } else error->all(FLERR,"Illegal fix/NVE/spin command");
  }

  // check if the atom/spin style is defined

  if (!atom->sp_flag)
    error->all(FLERR,"Fix NVE/spin requires atom/spin style");

  // check if sector_flag is correctly defined

  if (sector_flag == 0 && nprocs_tmp > 1)
    error->all(FLERR,"Illegal fix/NVE/spin command");

  // initialize the magnetic interaction flags

  magpair_flag = 0;
  magprecession_flag = 0;
  zeeman_flag = aniso_flag = 0;
  maglangevin_flag = 0;
  tdamp_flag = temp_flag = 0;

}

/* ---------------------------------------------------------------------- */

FixNVESpin::~FixNVESpin()
{
  memory->destroy(rsec);
  memory->destroy(stack_head);
  memory->destroy(stack_foot);
  memory->destroy(forward_stacks);
  memory->destroy(backward_stacks);
  delete lockpairspin;
}

/* ---------------------------------------------------------------------- */

int FixNVESpin::setmask()
{
  int mask = 0;
  mask |= INITIAL_INTEGRATE;
  mask |= PRE_NEIGHBOR;
  mask |= FINAL_INTEGRATE;
  return mask;  
}

/* ---------------------------------------------------------------------- */

void FixNVESpin::init()
{

  // set timesteps

  dtv = update->dt;
  dtf = 0.5 * update->dt * force->ftm2v;
  dts = 0.25 * update->dt;

  // ptrs on PairSpin classes

  lockpairspin = new PairSpin(lmp);
  magpair_flag = lockpairspin->init_pair();
  if (magpair_flag != 0 && magpair_flag != 1)
    error->all(FLERR,"Incorrect value of magpair_flag");

  // ptrs FixPrecessionSpin classes

  int iforce;
  for (iforce = 0; iforce < modify->nfix; iforce++) {
    if (strstr(modify->fix[iforce]->style,"precession/spin")) {
      magprecession_flag = 1;
      lockprecessionspin = (FixPrecessionSpin *) modify->fix[iforce]; 
    }
  }

  if (magprecession_flag) { 
    if (lockprecessionspin->zeeman_flag == 1) zeeman_flag = 1;
    if (lockprecessionspin->aniso_flag == 1) aniso_flag = 1;
  }

  // ptrs on the FixLangevinSpin class
  
  for (iforce = 0; iforce < modify->nfix; iforce++) {
    if (strstr(modify->fix[iforce]->style,"langevin/spin")) {
      maglangevin_flag = 1;
      locklangevinspin = (FixLangevinSpin *) modify->fix[iforce]; 
    } 
  }

  if (maglangevin_flag) {
   if (locklangevinspin->tdamp_flag == 1) tdamp_flag = 1;
   if (locklangevinspin->temp_flag == 1) temp_flag = 1;
  }
 
  // setting the sector variables/lists

  nsectors = 0;
  memory->create(rsec,3,"NVE/spin:rsec");
  
  // perform the sectoring operation

  if (sector_flag) sectoring();

  // init. size of stacking lists (sectoring)

  nlocal_max = atom->nlocal;
  stack_head = memory->grow(stack_head,nsectors,"NVE/spin:stack_head");
  stack_foot = memory->grow(stack_foot,nsectors,"NVE/spin:stack_foot");
  forward_stacks = memory->grow(forward_stacks,nlocal_max,"NVE/spin:forward_stacks");
  backward_stacks = memory->grow(backward_stacks,nlocal_max,"NVE/spin:backward_stacks");
  if (nlocal_max == 0)
    error->all(FLERR,"Incorrect value of nlocal_max");

}

/* ---------------------------------------------------------------------- */

void FixNVESpin::initial_integrate(int vflag)
{
  double dtfm,msq,scale,fm2,fmsq,sp2,spsq,energy;
  double spi[3], fmi[3];
	
  double **x = atom->x;	
  double **v = atom->v;
  double **f = atom->f;
  double **sp = atom->sp;
  double **fm = atom->fm;
  double *rmass = atom->rmass; 
  double *mass = atom->mass;  
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;  
  int *type = atom->type;
  int *mask = atom->mask;  

  // update half v for all atoms
  
  if (lattice_flag) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
        if (rmass) dtfm = dtf / rmass[i];
        else dtfm = dtf / mass[type[i]]; 
        v[i][0] += dtfm * f[i][0];
        v[i][1] += dtfm * f[i][1];
        v[i][2] += dtfm * f[i][2];
      }
    }
  }

  // update half s for all atoms

  if (sector_flag) {				// sectoring seq. update
    for (int j = 0; j < nsectors; j++) {	// advance quarter s for nlocal
      comm->forward_comm();
      int i = stack_foot[j];
      while (i >= 0) {
        ComputeInteractionsSpin(i);
    	AdvanceSingleSpin(i);
	i = forward_stacks[i];
      }
    }
    for (int j = nsectors-1; j >= 0; j--) {	// advance quarter s for nlocal 
      comm->forward_comm();
      int i = stack_head[j];
      while (i >= 0) {
        ComputeInteractionsSpin(i);
    	AdvanceSingleSpin(i);
	i = backward_stacks[i];
      }
    }
  } else if (sector_flag == 0) {		// serial seq. update
    comm->forward_comm();			// comm. positions of ghost atoms
    for (int i = 0; i < nlocal; i++){		// advance quarter s for nlocal
      ComputeInteractionsSpin(i);
      AdvanceSingleSpin(i);
    }
    for (int i = nlocal-1; i >= 0; i--){	// advance quarter s for nlocal
      ComputeInteractionsSpin(i);
      AdvanceSingleSpin(i);
    }
  } else error->all(FLERR,"Illegal fix NVE/spin command");

  // update x for all particles

  if (lattice_flag) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
        x[i][0] += dtv * v[i][0];
        x[i][1] += dtv * v[i][1];
        x[i][2] += dtv * v[i][2];
      }
    }
  }

  // update half s for all particles

  if (sector_flag) {				// sectoring seq. update
    for (int j = 0; j < nsectors; j++) {	// advance quarter s for nlocal
      comm->forward_comm();
      int i = stack_foot[j];
      while (i >= 0) {
        ComputeInteractionsSpin(i);
    	AdvanceSingleSpin(i);
	i = forward_stacks[i];
      }
    }
    for (int j = nsectors-1; j >= 0; j--) {	// advance quarter s for nlocal 
      comm->forward_comm();
      int i = stack_head[j];
      while (i >= 0) {
        ComputeInteractionsSpin(i);
    	AdvanceSingleSpin(i);
	i = backward_stacks[i];
      }
    }
  } else if (sector_flag == 0) {			// serial seq. update
    comm->forward_comm();			// comm. positions of ghost atoms
    for (int i = 0; i < nlocal; i++){	// advance quarter s for nlocal-1
      ComputeInteractionsSpin(i);
      AdvanceSingleSpin(i);
    }
    for (int i = nlocal-2; i >= 0; i--){	// advance quarter s for nlocal-1
      ComputeInteractionsSpin(i);
      AdvanceSingleSpin(i);
    }
  } else error->all(FLERR,"Illegal fix NVE/spin command");

}

/* ---------------------------------------------------------------------- 
   setup pre_neighbor()
---------------------------------------------------------------------- */

void FixNVESpin::setup_pre_neighbor()
{
  pre_neighbor();
}

/* ---------------------------------------------------------------------- 
   store in two linked lists the advance order of the spins (sectoring)
---------------------------------------------------------------------- */

void FixNVESpin::pre_neighbor()
{
  double **x = atom->x;	
  int nlocal = atom->nlocal;

  if (nlocal_max < nlocal) {			// grow linked lists if necessary
    nlocal_max = nlocal;
    forward_stacks = memory->grow(forward_stacks,nlocal_max,"NVE/spin:forward_stacks");
    backward_stacks = memory->grow(backward_stacks,nlocal_max,"NVE/spin:backward_stacks");
  }

  for (int j = 0; j < nsectors; j++) {
    stack_head[j] = -1;
    stack_foot[j] = -1;
  }

  int nseci;
  for (int j = 0; j < nsectors; j++) {		// stacking backward order
    for (int i = 0; i < nlocal; i++) {
      nseci = coords2sector(x[i]);
      if (j != nseci) continue;
      backward_stacks[i] = stack_head[j];
      stack_head[j] = i;
    }
  }
  for (int j = nsectors-1; j >= 0; j--) {	// stacking forward order
    for (int i = nlocal-1; i >= 0; i--) {
      nseci = coords2sector(x[i]);
      if (j != nseci) continue;
      forward_stacks[i] = stack_foot[j];
      stack_foot[j] = i;
    }
  }

}

/* ---------------------------------------------------------------------- 
   compute the magnetic torque for the spin ii
---------------------------------------------------------------------- */

void FixNVESpin::ComputeInteractionsSpin(int i)
{
  const int nlocal = atom->nlocal;
  double spi[3], fmi[3];

  double **sp = atom->sp;
  double **fm = atom->fm;
 
  int eflag = 1;
  int vflag = 0;
  int pair_compute_flag = 1;

  // force computation for spin i

  spi[0] = sp[i][0];
  spi[1] = sp[i][1];
  spi[2] = sp[i][2];
  
  fmi[0] = fmi[1] = fmi[2] = 0.0;

  // evaluate magnetic pair interactions

  if (magpair_flag) lockpairspin->compute_pair_single_spin(i,fmi);

  // evaluate magnetic precession interactions

  if (magprecession_flag) {		// magnetic precession
    if (zeeman_flag) {			// zeeman
      lockprecessionspin->compute_zeeman(i,fmi);
    }
    if (aniso_flag) {			// aniso
      lockprecessionspin->compute_anisotropy(i,spi,fmi);
    }
  }

  if (maglangevin_flag) {		// mag. langevin
    if (tdamp_flag) {			// transverse damping
      locklangevinspin->add_tdamping(spi,fmi);   
    }
    if (temp_flag) { 			// spin temperature
      locklangevinspin->add_temperature(fmi);
    } 
  }

  // replace the magnetic force fm[i] by its new value

  fm[i][0] = fmi[0];
  fm[i][1] = fmi[1];
  fm[i][2] = fmi[2];

}

/* ---------------------------------------------------------------------- 
   divide each domain into 8 sectors
---------------------------------------------------------------------- */

void FixNVESpin::sectoring()
{
  int sec[3];
  double sublo[3],subhi[3];
  double* sublotmp = domain->sublo;
  double* subhitmp = domain->subhi;
  for (int dim = 0 ; dim < 3 ; dim++) {
    sublo[dim]=sublotmp[dim];
    subhi[dim]=subhitmp[dim];
  }

  const double rsx = subhi[0] - sublo[0];  
  const double rsy = subhi[1] - sublo[1];  
  const double rsz = subhi[2] - sublo[2];  

  // temp
  //const double rv = 2.0;
  //const double rv = lockpairspinexchange->cut_spin_exchange_global;
  const double rv = lockpairspin->larger_cutoff;

  double rax = rsx/rv;  
  double ray = rsy/rv;  
  double raz = rsz/rv;  
 
  sec[0] = 1;
  sec[1] = 1;
  sec[2] = 1;
  if (rax >= 2.0) sec[0] = 2;
  if (ray >= 2.0) sec[1] = 2;
  if (raz >= 2.0) sec[2] = 2;

  nsectors = sec[0]*sec[1]*sec[2];

  if (sector_flag == 1 && nsectors != 8) 
    error->all(FLERR,"Illegal sectoring operation");

  rsec[0] = rsx;
  rsec[1] = rsy;
  rsec[2] = rsz;
  if (sec[0] == 2) rsec[0] = rsx/2.0;
  if (sec[1] == 2) rsec[1] = rsy/2.0;
  if (sec[2] == 2) rsec[2] = rsz/2.0;

}

/* ---------------------------------------------------------------------- 
   define sector for an atom at a position x[i]
---------------------------------------------------------------------- */

int FixNVESpin::coords2sector(double *x)
{
  int nseci;
  int seci[3];
  double sublo[3];
  double* sublotmp = domain->sublo;
  for (int dim = 0 ; dim<3 ; dim++) {
    sublo[dim]=sublotmp[dim];
  }

  seci[0] = x[0] > (sublo[0] + rsec[0]);
  seci[1] = x[1] > (sublo[1] + rsec[1]);
  seci[2] = x[2] > (sublo[2] + rsec[2]);

  nseci = (seci[0] + 2*seci[1] + 4*seci[2]); 

  return nseci;
}

/* ---------------------------------------------------------------------- 
   advance the spin i of a timestep dts
---------------------------------------------------------------------- */

void FixNVESpin::AdvanceSingleSpin(int i)
{
  int j=0;
  int *sametag = atom->sametag;
  double **sp = atom->sp;
  double **fm = atom->fm;
  double dtfm,msq,scale,fm2,fmsq,sp2,spsq,energy,dts2;
  double cp[3],g[3]; 	

  cp[0] = cp[1] = cp[2] = 0.0;
  g[0] = g[1] = g[2] = 0.0;
  fm2 = (fm[i][0]*fm[i][0])+(fm[i][1]*fm[i][1])+(fm[i][2]*fm[i][2]);
  fmsq = sqrt(fm2);
  energy = (sp[i][0]*fm[i][0])+(sp[i][1]*fm[i][1])+(sp[i][2]*fm[i][2]);
  dts2 = dts*dts;		

  cp[0] = fm[i][1]*sp[i][2]-fm[i][2]*sp[i][1];
  cp[1] = fm[i][2]*sp[i][0]-fm[i][0]*sp[i][2];
  cp[2] = fm[i][0]*sp[i][1]-fm[i][1]*sp[i][0];

  g[0] = sp[i][0]+cp[0]*dts;
  g[1] = sp[i][1]+cp[1]*dts;
  g[2] = sp[i][2]+cp[2]*dts;
			  
  g[0] += (fm[i][0]*energy-0.5*sp[i][0]*fm2)*0.5*dts2;
  g[1] += (fm[i][1]*energy-0.5*sp[i][1]*fm2)*0.5*dts2;
  g[2] += (fm[i][2]*energy-0.5*sp[i][2]*fm2)*0.5*dts2;
			  
  g[0] /= (1+0.25*fm2*dts2);
  g[1] /= (1+0.25*fm2*dts2);
  g[2] /= (1+0.25*fm2*dts2);
			  
  sp[i][0] = g[0];
  sp[i][1] = g[1];
  sp[i][2] = g[2];			  

  // renormalization (check if necessary)

  msq = g[0]*g[0] + g[1]*g[1] + g[2]*g[2];
  scale = 1.0/sqrt(msq);
  sp[i][0] *= scale;
  sp[i][1] *= scale;
  sp[i][2] *= scale;

  // comm. sp[i] to atoms with same tag (for serial algo)

  if (sector_flag == 0) {
    if (sametag[i] >= 0) {
      j = sametag[i];
      while (j >= 0) {
        sp[j][0] = sp[i][0];
        sp[j][1] = sp[i][1];
        sp[j][2] = sp[i][2];
        j = sametag[j];
      }
    }
  }

}

/* ---------------------------------------------------------------------- */

void FixNVESpin::final_integrate()
{	
  double dtfm,msq,scale,fm2,fmsq,energy;
  double cp[3],g[3]; 	
	
  double **x = atom->x;	
  double **v = atom->v;
  double **f = atom->f;
  double *rmass = atom->rmass;
  double *mass = atom->mass;  
  int nlocal = atom->nlocal;
  if (igroup == atom->firstgroup) nlocal = atom->nfirst;  
  int *type = atom->type;
  int *mask = atom->mask; 

  // update half v for all particles

  if (lattice_flag) {
    for (int i = 0; i < nlocal; i++) {
      if (mask[i] & groupbit) {
        if (rmass) dtfm = dtf / rmass[i];
        else dtfm = dtf / mass[type[i]]; 
        v[i][0] += dtfm * f[i][0];
        v[i][1] += dtfm * f[i][1];
        v[i][2] += dtfm * f[i][2];  
      }
    }
  }

}
