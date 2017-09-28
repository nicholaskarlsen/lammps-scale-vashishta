/*
 fix_rhoK_umbrella.cpp
 
 A fix to do bias potential on rho(k).
 
 The usage is as follows:
 
 fix [name] [groupID] rhoK [nx] [ny] [nz] [kappa = spring constant] [rhoK0]
 
 where k_i = (2 pi / L_i) * n_i
 
 Written by Ulf Pedersen and Patrick Varilly, 4 Feb 2010
 Tweaked for LAMMPS 15 Jan 2010 version by Ulf Pedersen, 19 Aug 2010
 Tweaked again March 4th   2012   by Ulf R. Pedersen,
               September   2016   by Ulf R. Pedersen
 */

#include "fix_rhok.h"
#include "error.h"
#include "update.h"
#include "respa.h"
#include "atom.h"
#include "domain.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

using namespace LAMMPS_NS;
using namespace FixConst;

// Constructor: all the parameters to this fix specified in
// the LAMMPS input get passed in
FixRhok::FixRhok( LAMMPS* inLMP, int inArgc, char** inArgv )
  : Fix( inLMP, inArgc, inArgv )
{
  // Check arguments
  if( inArgc != 8 ) 
	error->all(FLERR,"Illegal fix rhoKUmbrella command" );

  // Set up fix flags
  scalar_flag = 1;         // have compute_scalar
  vector_flag = 1;         // have compute_vector...
  size_vector = 3;         // ...with this many components
  global_freq = 1;         // whose value can be computed at every timestep
  //scalar_vector_freq = 1;  // OLD lammps: whose value can be computed at every timestep
  thermo_energy = 1;       // this fix changes system's potential energy
  extscalar = 0;           // but the deltaPE might not scale with # of atoms
  extvector = 0;           // neither do the components of the vector

  // Parse fix options
	int n[3];
	
  n[0]   = atoi( inArgv[3] );
  n[1]   = atoi( inArgv[4] );
  n[2]   = atoi( inArgv[5] );
	
	mK[0] = n[0]*(2*M_PI / (domain->boxhi[0] - domain->boxlo[0]));
	mK[1] = n[1]*(2*M_PI / (domain->boxhi[1] - domain->boxlo[1]));
	mK[2] = n[2]*(2*M_PI / (domain->boxhi[2] - domain->boxlo[2]));
	
  mKappa = atof( inArgv[6] );
  mRhoK0 = atof( inArgv[7] );
}

FixRhok::~FixRhok()
{
}

// Methods that this fix implements
// --------------------------------

// Tells LAMMPS where this fix should act
int
FixRhok::setmask()
{
  int mask = 0;

  // This fix modifies forces...
  mask |= POST_FORCE;
  mask |= POST_FORCE_RESPA;
  mask |= MIN_POST_FORCE;

  // ...and potential energies
  mask |= THERMO_ENERGY;

  return mask;
}

/*int FixRhok::setmask()
{
  int mask = 0;
  mask |= POST_FORCE;
  mask |= POST_FORCE_RESPA;
  mask |= MIN_POST_FORCE;
  return mask;
}*/


// Initializes the fix at the beginning of a run
void
FixRhok::init()
{
  // RESPA boilerplate
  if( strcmp( update->integrate_style, "respa" ) == 0 )
	mNLevelsRESPA = ((Respa *) update->integrate)->nlevels;
	
	// Count the number of affected particles
	int nThisLocal = 0;
	int *mask = atom->mask;
	int nlocal = atom->nlocal;
	for( int i = 0; i < nlocal; i++ ) {   // Iterate through all atoms on this CPU
		if( mask[i] & groupbit ) {          // ...only those affected by this fix
			nThisLocal++;
		}
	}
	MPI_Allreduce( &nThisLocal, &mNThis,
				  1, MPI_INT, MPI_SUM, world );
	mSqrtNThis = sqrt( mNThis );
}

// Initial application of the fix to a system (when doing MD)
void
FixRhok::setup( int inVFlag )
{
  if( strcmp( update->integrate_style, "verlet" ) == 0 )
	post_force( inVFlag );
  else
	{
	  ((Respa *) update->integrate)->copy_flevel_f( mNLevelsRESPA - 1 );
	  post_force_respa( inVFlag, mNLevelsRESPA - 1,0 );
	  ((Respa *) update->integrate)->copy_f_flevel( mNLevelsRESPA - 1 );
	}
}

// Initial application of the fix to a system (when doing minimization)
void
FixRhok::min_setup( int inVFlag )
{
  post_force( inVFlag );
}

// Modify the forces calculated in the main force loop of ordinary MD
void
FixRhok::post_force( int inVFlag )
{
  double **x = atom->x;
  double **f = atom->f;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  // Loop over locally-owned atoms affected by this fix and calculate the
  // partial rhoK's
	mRhoKLocal[0] = 0.0;
	mRhoKLocal[1] = 0.0;

  for( int i = 0; i < nlocal; i++ ) {   // Iterate through all atoms on this CPU
	if( mask[i] & groupbit ) {          // ...only those affected by this fix

		// rho_k = sum_i exp( - i k.r_i )
		mRhoKLocal[0] += cos( mK[0]*x[i][0] + mK[1]*x[i][1] + mK[2]*x[i][2] );
		mRhoKLocal[1] -= sin( mK[0]*x[i][0] + mK[1]*x[i][1] + mK[2]*x[i][2] );
	}
  }
	
	// Now calculate mRhoKGlobal
	MPI_Allreduce( mRhoKLocal, mRhoKGlobal,
				  2, MPI_DOUBLE, MPI_SUM, world );
	
	// Info:  < \sum_{i,j} e^{-ik.(r_i - r_j)} > ~ N, so
	// we define rho_k as (1 / sqrt(N)) \sum_i e^{-i k.r_i}, so that
	// <rho_k^2> is intensive.
	mRhoKGlobal[0] /= mSqrtNThis;
	mRhoKGlobal[1] /= mSqrtNThis;
	
	// We'll need magnitude of rho_k
	double rhoK = sqrt( mRhoKGlobal[0]*mRhoKGlobal[0]
					   + mRhoKGlobal[1]*mRhoKGlobal[1] );
	
	for( int i = 0; i < nlocal; i++ ) {   // Iterate through all atoms on this CPU
		if( mask[i] & groupbit ) {          // ...only those affected by this fix
			
			// Calculate forces
			// U = kappa/2 ( |rho_k| - rho_k^0 )^2
			// f_i = -grad_i U = -kappa ( |rho_k| - rho_k^0 ) grad_i |rho_k|
			// grad_i |rho_k| = Re( rho_k* (-i k e^{-i k . r_i} / sqrt(N)) ) / |rho_k|
			//
			// In terms of real and imag parts of rho_k,
			//
			// Re( rho_k* (-i k e^{-i k . r_i}) ) =
			//   (- Re[rho_k] * sin( k . r_i ) - Im[rho_k] * cos( k . r_i )) * k

			double sinKRi = sin( mK[0]*x[i][0] + mK[1]*x[i][1] + mK[2]*x[i][2] );
			double cosKRi = cos( mK[0]*x[i][0] + mK[1]*x[i][1] + mK[2]*x[i][2] );
			
			double prefactor = mKappa * ( rhoK - mRhoK0 ) / rhoK
				* (-mRhoKGlobal[0]*sinKRi - mRhoKGlobal[1]*cosKRi) / mSqrtNThis;
			f[i][0] -= prefactor * mK[0];
			f[i][1] -= prefactor * mK[1];
			f[i][2] -= prefactor * mK[2];
		}
	}
}

// Forces in RESPA loop
void
FixRhok::post_force_respa( int inVFlag, int inILevel, int inILoop )
{
  if( inILevel == mNLevelsRESPA - 1 )
	post_force( inVFlag );
}

// Forces in minimization loop
void
FixRhok::min_post_force( int inVFlag )
{
  post_force( inVFlag );
}

// Compute the change in the potential energy induced by this fix
double
FixRhok::compute_scalar()
{
	double rhoK = sqrt( mRhoKGlobal[0]*mRhoKGlobal[0]
					   + mRhoKGlobal[1]*mRhoKGlobal[1] );
	
  return 0.5 * mKappa * (rhoK - mRhoK0) * (rhoK - mRhoK0);
}

// Compute the ith component of the vector
double
FixRhok::compute_vector( int inI )
{
	if( inI == 0 )
		return mRhoKGlobal[0];   // Real part
	else if( inI == 1 )
		return mRhoKGlobal[1];   // Imagniary part
	else if( inI == 2 )
		return sqrt( mRhoKGlobal[0]*mRhoKGlobal[0]
					+ mRhoKGlobal[1]*mRhoKGlobal[1] );
	else
		return 12345.0;
}
