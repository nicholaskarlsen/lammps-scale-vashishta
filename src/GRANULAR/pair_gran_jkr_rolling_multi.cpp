/* ----------------------------------------------------------------------
http://lammps.sandia.gov, Sandia National Laboratories
Steve Plimpton, sjplimp@sandia.gov

Copyright (2003) Sandia Corporation.  Under the terms of Contract
DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
certain rights in this software.  This software is distributed under
the GNU General Public License.

See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
	 Contributing authors: Leo Silbert (SNL), Gary Grest (SNL)
	 ------------------------------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pair_gran_jkr_rolling_multi.h"
#include "atom.h"
#include "atom_vec.h"
#include "domain.h"
#include "force.h"
#include "update.h"
#include "modify.h"
#include "fix.h"
#include "fix_neigh_history.h"
#include "comm.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "memory.h"
#include "error.h"
#include "math_const.h"
//#include <fenv.h>

using namespace LAMMPS_NS;
using namespace MathConst;

#define ONETHIRD 0.33333333333333333
#define TWOTHIRDS 0.66666666666666666
#define POW6ONE 0.550321208149104 //6^(-1/3)
#define POW6TWO 0.30285343213869  //6^(-2/3)

#define EPSILON 1e-10

enum {TSUJI, BRILLIANTOV};
enum {INDEP, BRILLROLL};

/* ---------------------------------------------------------------------- */

PairGranJKRRollingMulti::PairGranJKRRollingMulti(LAMMPS *lmp) : Pair(lmp)
{
	single_enable = 1;
	no_virial_fdotr_compute = 1;
	history = 1;
	fix_history = NULL;

	single_extra = 10;
	svector = new double[10];

	neighprev = 0;

	nmax = 0;
	mass_rigid = NULL;

	// set comm size needed by this Pair if used with fix rigid

	comm_forward = 1;
}

/* ---------------------------------------------------------------------- */
PairGranJKRRollingMulti::~PairGranJKRRollingMulti()
{
	delete [] svector;
	if (fix_history) modify->delete_fix("NEIGH_HISTORY");

	if (allocated) {
		memory->destroy(setflag);
		memory->destroy(cutsq);

		memory->destroy(cut);
		memory->destroy(E);
		memory->destroy(G);
		memory->destroy(normaldamp);
		memory->destroy(rollingdamp);
		memory->destroy(alpha);
		memory->destroy(gamman);
		memory->destroy(muS);
		memory->destroy(Ecoh);
		memory->destroy(kR);
		memory->destroy(muR);
		memory->destroy(etaR);

		delete [] onerad_dynamic;
		delete [] onerad_frozen;
		delete [] maxrad_dynamic;
		delete [] maxrad_frozen;
	}
	memory->destroy(mass_rigid);
}
/* ---------------------------------------------------------------------- */

void PairGranJKRRollingMulti::compute(int eflag, int vflag)
{
//	feenableexcept(FE_INVALID | FE_OVERFLOW);
	int i,j,ii,jj,inum,jnum,itype,jtype;
	double xtmp,ytmp,ztmp,delx,dely,delz,fx,fy,fz,nx,ny,nz;
	double radi,radj,radsum,rsq,r,rinv,rsqinv,R,a;
	double vr1,vr2,vr3,vnnr,vn1,vn2,vn3,vt1,vt2,vt3;
	double wr1,wr2,wr3;
	double vtr1,vtr2,vtr3,vrel;
	double kn, kt, k_Q, k_R, eta_N, eta_T, eta_Q, eta_R;
	double Fne, Fdamp, Fntot, Fscrit, Frcrit, F_C, delta_C, delta_Cinv;
	double overlap, olapsq, olapcubed, sqrtterm, tmp, a0;
	double keyterm, keyterm2, keyterm3, aovera0, foverFc;
	double mi,mj,meff,damp,ccel,tor1,tor2,tor3;
	double relrot1,relrot2,relrot3,vrl1,vrl2,vrl3,vrlmag,vrlmaginv;
	double rollmag, rolldotn, scalefac;
	double fr, fr1, fr2, fr3;
	double signtwist, magtwist, magtortwist, Mtcrit;
	double fs,fs1,fs2,fs3,roll1,roll2,roll3,torroll1,torroll2,torroll3;
	double tortwist1, tortwist2, tortwist3;
	double shrmag,rsht;
	int *ilist,*jlist,*numneigh,**firstneigh;
	int *touch,**firsttouch;
	double *shear,*allshear,**firstshear;

	if (eflag || vflag) ev_setup(eflag,vflag);
	else evflag = vflag_fdotr = 0;

	int shearupdate = 1;
	if (update->setupflag) shearupdate = 0;

	// update rigid body info for owned & ghost atoms if using FixRigid masses
	// body[i] = which body atom I is in, -1 if none
	// mass_body = mass of each rigid body

	if (fix_rigid && neighbor->ago == 0){
		int tmp;
		int *body = (int *) fix_rigid->extract("body",tmp);
		double *mass_body = (double *) fix_rigid->extract("masstotal",tmp);
		if (atom->nmax > nmax) {
			memory->destroy(mass_rigid);
			nmax = atom->nmax;
			memory->create(mass_rigid,nmax,"pair:mass_rigid");
		}
		int nlocal = atom->nlocal;
		for (i = 0; i < nlocal; i++)
			if (body[i] >= 0) mass_rigid[i] = mass_body[body[i]];
			else mass_rigid[i] = 0.0;
		comm->forward_comm_pair(this);
	}

	double **x = atom->x;
	double **v = atom->v;
	double **f = atom->f;
	int *type = atom->type;
	double **omega = atom->omega;
	double **torque = atom->torque;
	double *radius = atom->radius;
	double *rmass = atom->rmass;
	int *mask = atom->mask;
	int nlocal = atom->nlocal;
	int newton_pair = force->newton_pair;

	inum = list->inum;
	ilist = list->ilist;
	numneigh = list->numneigh;
	firstneigh = list->firstneigh;
	firsttouch = fix_history->firstflag;
	firstshear = fix_history->firstvalue;

	// loop over neighbors of my atoms

	for (ii = 0; ii < inum; ii++) {
		i = ilist[ii];
		xtmp = x[i][0];
		ytmp = x[i][1];
		ztmp = x[i][2];
		itype = type[i];
		radi = radius[i];
		touch = firsttouch[i];
		allshear = firstshear[i];
		jlist = firstneigh[i];
		jnum = numneigh[i];

		for (jj = 0; jj < jnum; jj++) {
			j = jlist[jj];
			j &= NEIGHMASK;

			delx = xtmp - x[j][0];
			dely = ytmp - x[j][1];
			delz = ztmp - x[j][2];
			jtype = type[j];
			rsq = delx*delx + dely*dely + delz*delz;
			radj = radius[j];
			radsum = radi + radj;
			R = radi*radj/(radi+radj);
			a0 = pow(9.0*M_PI*Ecoh[itype][jtype]*R*R/E[itype][jtype],ONETHIRD);
			delta_C = 0.5*a0*a0*POW6ONE/R;

			if ((rsq >= radsum*radsum && touch[jj] == 0) ||
					(rsq >= (radsum+delta_C)*(radsum+delta_C))) {

				// unset non-touching neighbors

				touch[jj] = 0;
				shear = &allshear[3*jj];
				shear[0] = 0.0;
				shear[1] = 0.0;
				shear[2] = 0.0;

			} else {
				F_C = 3.0*R*M_PI*Ecoh[itype][jtype];
				r = sqrt(rsq);
				rinv = 1.0/r;
				rsqinv = 1.0/rsq;

				nx = delx*rinv;
				ny = dely*rinv;
				nz = delz*rinv;

				// relative translational velocity

				vr1 = v[i][0] - v[j][0];
				vr2 = v[i][1] - v[j][1];
				vr3 = v[i][2] - v[j][2];

				// normal component

				vnnr = vr1*nx + vr2*ny + vr3*nz; //v_R . n
				vn1 = nx*vnnr;
				vn2 = ny*vnnr;
				vn3 = nz*vnnr;

				// meff = effective mass of pair of particles
				// if I or J part of rigid body, use body mass
				// if I or J is frozen, meff is other particle

				mi = rmass[i];
				mj = rmass[j];
				if (fix_rigid) {
					if (mass_rigid[i] > 0.0) mi = mass_rigid[i];
					if (mass_rigid[j] > 0.0) mj = mass_rigid[j];
				}

				meff = mi*mj / (mi+mj);
				if (mask[i] & freeze_group_bit) meff = mj;
				if (mask[j] & freeze_group_bit) meff = mi;

				//****************************************
				//Normal force = JKR-adjusted Hertzian contact + damping
				//****************************************
				if (Ecoh[itype][jtype] != 0.0) delta_Cinv = 1.0/delta_C;
				else delta_Cinv = 1.0;
				overlap = (radsum - r)*delta_Cinv;
				olapsq = overlap*overlap;
				olapcubed = olapsq*overlap;
				sqrtterm = sqrt(1.0 + olapcubed);
				tmp = 2.0 + olapcubed + 2.0*sqrtterm;
				keyterm = pow(tmp,ONETHIRD);
				keyterm2 = olapsq/keyterm;
				keyterm3 = sqrt(overlap + keyterm2 + keyterm);
				aovera0 = POW6TWO * (keyterm3 +
						sqrt(2.0*overlap - keyterm2 - keyterm + 4.0/keyterm3));// eq 41
				a = aovera0*a0;
				foverFc = 4.0*((aovera0*aovera0*aovera0) - pow(aovera0,1.5));//F_ne/F_C (eq 40)

				Fne = F_C*foverFc;

				//Damping
				kn = 4.0/3.0*E[itype][jtype]*a;
				if (normaldamp[itype][jtype] == BRILLIANTOV) eta_N = a*meff*gamman[itype][jtype];
				else if (normaldamp[itype][jtype] == TSUJI) eta_N=alpha[itype][jtype]*sqrt(meff*kn);

				Fdamp = -eta_N*vnnr; //F_nd eq 23 and Zhao eq 19

				Fntot = Fne + Fdamp;
				//if (screen) fprintf(screen,"%d %d %16.16g %16.16g  \n",itype,jtype,Ecoh[itype][jtype],E[itype][jtype]);
				//if (logfile) fprintf(logfile,"%d %d %16.16g %16.16g \n",itype,jtype,Ecoh[itype][jtype],E[itype][jtype]);

				//****************************************
				//Tangential force, including shear history effects
				//****************************************

				// tangential component
				vt1 = vr1 - vn1;
				vt2 = vr2 - vn2;
				vt3 = vr3 - vn3;

				// relative rotational velocity
				// Luding Gran Matt 2008, v10,p235 suggests correcting radi and radj by subtracting
				// delta/2, i.e. instead of radi, use distance to center of contact point?
				wr1 = (radi*omega[i][0] + radj*omega[j][0]);
				wr2 = (radi*omega[i][1] + radj*omega[j][1]);
				wr3 = (radi*omega[i][2] + radj*omega[j][2]);

				// relative tangential velocities
				vtr1 = vt1 - (nz*wr2-ny*wr3);
				vtr2 = vt2 - (nx*wr3-nz*wr1);
				vtr3 = vt3 - (ny*wr1-nx*wr2);
				vrel = vtr1*vtr1 + vtr2*vtr2 + vtr3*vtr3;
				vrel = sqrt(vrel);

				// shear history effects
				touch[jj] = 1;
				shear = &allshear[3*jj];
				shrmag = sqrt(shear[0]*shear[0] + shear[1]*shear[1] +
						shear[2]*shear[2]);

				// Rotate and update shear displacements.
				// See e.g. eq. 17 of Luding, Gran. Matter 2008, v10,p235
				if (shearupdate) {
					rsht = shear[0]*nx + shear[1]*ny + shear[2]*nz;
					if (fabs(rsht) < EPSILON) rsht = 0;
					if (rsht > 0){
						scalefac = shrmag/(shrmag - rsht); //if rhst == shrmag, contacting pair has rotated 90 deg. in one step, in which case you deserve a crash!
						shear[0] -= rsht*nx;
						shear[1] -= rsht*ny;
						shear[2] -= rsht*nz;
						//Also rescale to preserve magnitude
						shear[0] *= scalefac;
						shear[1] *= scalefac;
						shear[2] *= scalefac;
					}
					//Update shear history
					shear[0] += vtr1*dt;
					shear[1] += vtr2*dt;
					shear[2] += vtr3*dt;
				}

				// tangential forces = shear + tangential velocity damping
				// following Zhao and Marshall Phys Fluids v20, p043302 (2008)
				kt=8.0*G[itype][jtype]*a;

				eta_T = eta_N; //Based on discussion in Marshall; eta_T can also be an independent parameter
				fs1 = -kt*shear[0] - eta_T*vtr1; //eq 26
				fs2 = -kt*shear[1] - eta_T*vtr2;
				fs3 = -kt*shear[2] - eta_T*vtr3;

				// rescale frictional displacements and forces if needed
				Fscrit = muS[itype][jtype] * fabs(Fne + 2*F_C);
				// For JKR, use eq 43 of Marshall. For DMT, use Fne instead

				fs = sqrt(fs1*fs1 + fs2*fs2 + fs3*fs3);
				if (fs > Fscrit) {
					if (shrmag != 0.0) {
						//shear[0] = (Fcrit/fs) * (shear[0] + eta_T*vtr1/kt) - eta_T*vtr1/kt;
						//shear[1] = (Fcrit/fs) * (shear[1] + eta_T*vtr1/kt) - eta_T*vtr1/kt;
						//shear[2] = (Fcrit/fs) * (shear[2] + eta_T*vtr1/kt) - eta_T*vtr1/kt;
						shear[0] = -1.0/kt*(Fscrit*fs1/fs + eta_T*vtr1); //Same as above, but simpler (check!)
						shear[1] = -1.0/kt*(Fscrit*fs2/fs + eta_T*vtr2);
						shear[2] = -1.0/kt*(Fscrit*fs3/fs + eta_T*vtr3);
						fs1 *= Fscrit/fs;
						fs2 *= Fscrit/fs;
						fs3 *= Fscrit/fs;
					} else fs1 = fs2 = fs3 = 0.0;
				}	

				//****************************************
				// Rolling force, including shear history effects
				//****************************************

				relrot1 = omega[i][0] - omega[j][0];
				relrot2 = omega[i][1] - omega[j][1];
				relrot3 = omega[i][2] - omega[j][2];

				// rolling velocity, see eq. 31 of Wang et al, Particuology v 23, p 49 (2015)
				// This is different from the Marshall papers, which use the Bagi/Kuhn formulation
				// for rolling velocity (see Wang et al for why the latter is wrong)
				vrl1 = R*(relrot2*nz - relrot3*ny); //- 0.5*((radj-radi)/radsum)*vtr1;
				vrl2 = R*(relrot3*nx - relrot1*nz); //- 0.5*((radj-radi)/radsum)*vtr2;
				vrl3 = R*(relrot1*ny - relrot2*nx); //- 0.5*((radj-radi)/radsum)*vtr3;
				vrlmag = sqrt(vrl1*vrl1+vrl2*vrl2+vrl3*vrl3);
				if (vrlmag != 0.0) vrlmaginv = 1.0/vrlmag;
				else vrlmaginv = 0.0;

				// Rolling displacement
				rollmag = sqrt(shear[3]*shear[3] + shear[4]*shear[4] + shear[5]*shear[5]);
				rolldotn = shear[3]*nx + shear[4]*ny + shear[5]*nz;

				if (shearupdate) {
					if (fabs(rolldotn) < EPSILON) rolldotn = 0;
					if (rolldotn > 0){ //Rotate into tangential plane
						scalefac = rollmag/(rollmag - rolldotn);
						shear[3] -= rolldotn*nx;
						shear[4] -= rolldotn*ny;
						shear[5] -= rolldotn*nz;
						//Also rescale to preserve magnitude
						shear[3] *= scalefac;
						shear[4] *= scalefac;
						shear[5] *= scalefac;
					}
					shear[3] += vrl1*dt;
					shear[4] += vrl2*dt;
					shear[5] += vrl3*dt;
				}

				k_R = kR[itype][jtype]*4.0*F_C*pow(aovera0,1.5);
				if (rollingdamp[itype][jtype] == INDEP) eta_R = etaR[itype][jtype];
				else if (rollingdamp[itype][jtype] == BRILLROLL) eta_R = muR[itype][jtype]*fabs(Fne);
				fr1 = -k_R*shear[3] - eta_R*vrl1;
				fr2 = -k_R*shear[4] - eta_R*vrl2;
				fr3 = -k_R*shear[5] - eta_R*vrl3;

				// rescale frictional displacements and forces if needed
				Frcrit = muR[itype][jtype] * fabs(Fne + 2*F_C);

				fr = sqrt(fr1*fr1 + fr2*fr2 + fr3*fr3);
				if (fr > Frcrit) {
					if (rollmag != 0.0) {
						shear[3] = -1.0/k_R*(Frcrit*fr1/fr + eta_R*vrl1);
						shear[4] = -1.0/k_R*(Frcrit*fr2/fr + eta_R*vrl2);
						shear[5] = -1.0/k_R*(Frcrit*fr3/fr + eta_R*vrl3);
						fr1 *= Frcrit/fr;
						fr2 *= Frcrit/fr;
						fr3 *= Frcrit/fr;
					} else fr1 = fr2 = fr3 = 0.0;
				}


				//****************************************
				// Twisting torque, including shear history effects
				//****************************************
				magtwist = relrot1*nx + relrot2*ny + relrot3*nz; //Omega_T (eq 29 of Marshall)
				shear[6] += magtwist*dt;
				k_Q = 0.5*kt*a*a;; //eq 32
				eta_Q = 0.5*eta_T*a*a;
				magtortwist = -k_Q*shear[6] - eta_Q*magtwist;//M_t torque (eq 30)

				signtwist = (magtwist > 0) - (magtwist < 0);
				Mtcrit=TWOTHIRDS*a*Fscrit;//critical torque (eq 44)
				if (fabs(magtortwist) > Mtcrit) {
					//shear[6] = Mtcrit/k_Q*magtwist/fabs(magtwist);
					shear[6] = 1.0/k_Q*(Mtcrit*signtwist - eta_Q*magtwist);
					magtortwist = -Mtcrit * signtwist; //eq 34
				}

				// Apply forces & torques

				fx = nx*Fntot + fs1;
				fy = ny*Fntot + fs2;
				fz = nz*Fntot + fs3;

				//if (screen) fprintf(screen,"%16.16g %16.16g %16.16g %16.16g %16.16g %16.16g %16.16g \n",fs1,fs2,fs3,Fntot,nx,ny,nz);
				//if (logfile) fprintf(logfile,"%16.16g %16.16g %16.16g %16.16g %16.16g %16.16g %16.16g \n",fs1,fs2,fs3,Fntot,nx,ny,nz);

				f[i][0] += fx;
				f[i][1] += fy;
				f[i][2] += fz;

				tor1 = ny*fs3 - nz*fs2;
				tor2 = nz*fs1 - nx*fs3;
				tor3 = nx*fs2 - ny*fs1;

				torque[i][0] -= radi*tor1;
				torque[i][1] -= radi*tor2;
				torque[i][2] -= radi*tor3;

				tortwist1 = magtortwist * nx;
				tortwist2 = magtortwist * ny;
				tortwist3 = magtortwist * nz;

				torque[i][0] += tortwist1;
				torque[i][1] += tortwist2;
				torque[i][2] += tortwist3;

				torroll1 = R*(ny*fr3 - nz*fr2); //n cross fr
				torroll2 = R*(nz*fr1 - nx*fr3);
				torroll3 = R*(nx*fr2 - ny*fr1);

				torque[i][0] += torroll1;
				torque[i][1] += torroll2;
				torque[i][2] += torroll3;

				if (force->newton_pair || j < nlocal) {
					f[j][0] -= fx;
					f[j][1] -= fy;
					f[j][2] -= fz;

					torque[j][0] -= radj*tor1;
					torque[j][1] -= radj*tor2;
					torque[j][2] -= radj*tor3;

					torque[j][0] -= tortwist1;
					torque[j][1] -= tortwist2;
					torque[j][2] -= tortwist3;

					torque[j][0] -= torroll1;
					torque[j][1] -= torroll2;
					torque[j][2] -= torroll3;
				}
				if (evflag) ev_tally_xyz(i,j,nlocal,0,
						0.0,0.0,fx,fy,fz,delx,dely,delz);
			} 
		}  
	}
}


/* ----------------------------------------------------------------------
	 allocate all arrays
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::allocate()
{
	allocated = 1;
	int n = atom->ntypes;

	memory->create(setflag,n+1,n+1,"pair:setflag");
	for (int i = 1; i <= n; i++)
		for (int j = i; j <= n; j++)
			setflag[i][j] = 0;

	memory->create(cutsq,n+1,n+1,"pair:cutsq");
	memory->create(cut,n+1,n+1,"pair:cut");
	memory->create(E,n+1,n+1,"pair:E");
	memory->create(G,n+1,n+1,"pair:G");
	memory->create(normaldamp,n+1,n+1,"pair:normaldamp");
	memory->create(rollingdamp,n+1,n+1,"pair:rollingdamp");
	memory->create(alpha,n+1,n+1,"pair:alpha");
	memory->create(gamman,n+1,n+1,"pair:gamman");
	memory->create(muS,n+1,n+1,"pair:muS");
	memory->create(Ecoh,n+1,n+1,"pair:Ecoh");
	memory->create(kR,n+1,n+1,"pair:kR");
	memory->create(muR,n+1,n+1,"pair:muR");
	memory->create(etaR,n+1,n+1,"pair:etaR");

	onerad_dynamic = new double[n+1];
	onerad_frozen = new double[n+1];
	maxrad_dynamic = new double[n+1];
	maxrad_frozen = new double[n+1];
}

/* ----------------------------------------------------------------------
	 global settings
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::settings(int narg, char **arg)
{
	if (narg != 1) error->all(FLERR,"Illegal pair_style command");

	if (strcmp(arg[0],"NULL") == 0 ) cut_global = -1.0;     
	else cut_global = force->numeric(FLERR,arg[0]);

	// reset cutoffs that have been explicitly set
	if (allocated) {
		int i,j;
		for (i = 1; i <= atom->ntypes; i++)
			for (j = i; j <= atom->ntypes; j++)
				if (setflag[i][j]) cut[i][j] = cut_global;
	}
}

/* ----------------------------------------------------------------------
	 set coeffs for one or more type pairs
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::coeff(int narg, char **arg)
{
	if (narg < 10 || narg > 15) 
		error->all(FLERR,"Incorrect args for pair coefficients2");

	if (!allocated) allocate();

	int ilo,ihi,jlo,jhi;
	force->bounds(FLERR,arg[0],atom->ntypes,ilo,ihi);
	force->bounds(FLERR,arg[1],atom->ntypes,jlo,jhi);

	double E_one = force->numeric(FLERR,arg[2]);
	double G_one = force->numeric(FLERR,arg[3]);
	double muS_one = force->numeric(FLERR,arg[4]);
	double cor_one = force->numeric(FLERR,arg[5]);
	double Ecoh_one = force->numeric(FLERR,arg[6]);
	double kR_one = force->numeric(FLERR,arg[7]);
	double muR_one = force->numeric(FLERR,arg[8]);
	double etaR_one = force->numeric(FLERR,arg[9]);

	//Defaults
	int normaldamp_one = TSUJI;
	int rollingdamp_one = INDEP;
	double cut_one = cut_global;

	int iarg = 10;
	while (iarg < narg) {
		if (strcmp(arg[iarg],"normaldamp") == 0){
			if (iarg+2 > narg) error->all(FLERR, "Invalid pair/gran/dmt/rolling entry");
			if (strcmp(arg[iarg+1],"tsuji") == 0) normaldamp_one = TSUJI;
			else if (strcmp(arg[iarg+1],"brilliantov") == 0) normaldamp_one = BRILLIANTOV;
			else error->all(FLERR, "Invalid normal damping model for pair/gran/dmt/rolling");
			iarg += 2;
		}
		else if (strcmp(arg[iarg],"rollingdamp") == 0){
			if (iarg+2 > narg) error->all(FLERR, "Invalid pair/gran/dmt/rolling entry");
			if (strcmp(arg[iarg+1],"independent") == 0) rollingdamp_one = INDEP;
			else if (strcmp(arg[iarg+1],"brilliantov") == 0) rollingdamp_one = BRILLROLL;
			else error->all(FLERR, "Invalid rolling damping model for pair/gran/dmt/rolling");
			iarg +=2;
		}
		else {
			if (strcmp(arg[iarg],"NULL") == 0) cut_one = -1.0;
			else cut_one = force->numeric(FLERR,arg[iarg]);
			iarg += 1;
		}
	}
	
	int count = 0;
	for (int i = ilo; i <= ihi; i++) {
		double pois = E_one/(2.0*G_one) - 1.0;
		double alpha_one = 1.2728-4.2783*cor_one+11.087*cor_one*cor_one-22.348*cor_one*cor_one*cor_one+27.467*cor_one*cor_one*cor_one*cor_one-18.022*cor_one*cor_one*cor_one*cor_one*cor_one+4.8218*cor_one*cor_one*cor_one*cor_one*cor_one*cor_one;

		for (int j = MAX(jlo,i); j <= jhi; j++) {
			E[i][j] = E_one;
			G[i][j] = G_one;
			if (normaldamp_one == TSUJI) {
				normaldamp[i][j] = TSUJI;
				alpha[i][j] = alpha_one;
			}
			else if (normaldamp_one == BRILLIANTOV) {
				normaldamp[i][j] = BRILLIANTOV;
				gamman[i][j] = cor_one;
			}
			if (rollingdamp_one == INDEP) {
				rollingdamp[i][j] = INDEP;
			}
			else if (rollingdamp_one == BRILLROLL) {
				rollingdamp[i][j] = BRILLROLL;
			}
			muS[i][j] = muS_one;
			Ecoh[i][j] = Ecoh_one;
			kR[i][j]  = kR_one;
			etaR[i][j]  = etaR_one;
			muR[i][j]  = muR_one;
			cut[i][j] = cut_one;
			setflag[i][j] = 1;
			count++;
		}
	}

	if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients1");
}

/* ----------------------------------------------------------------------
	 init specific to this pair style
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::init_style()
{
	int i;

	// error and warning checks

	if (!atom->radius_flag || !atom->rmass_flag)
		error->all(FLERR,"Pair granular requires atom attributes radius, rmass");
	if (comm->ghost_velocity == 0)
		error->all(FLERR,"Pair granular requires ghost atoms store velocity");

	// need a granular neigh list

	int irequest = neighbor->request(this,instance_me);
	neighbor->requests[irequest]->size = 1;
	if (history) neighbor->requests[irequest]->history = 1;

	dt = update->dt;

	// if shear history is stored:
	// if first init, create Fix needed for storing shear history

	if (history && fix_history == NULL) {
		char dnumstr[16];
		sprintf(dnumstr,"%d",3);
		char **fixarg = new char*[4];
		fixarg[0] = (char *) "NEIGH_HISTORY";
		fixarg[1] = (char *) "all";
		fixarg[2] = (char *) "NEIGH_HISTORY";
		fixarg[3] = dnumstr;
		modify->add_fix(4,fixarg,1);
		delete [] fixarg;
		fix_history = (FixNeighHistory *) modify->fix[modify->nfix-1];
		fix_history->pair = this;
	}

	// check for FixFreeze and set freeze_group_bit

	for (i = 0; i < modify->nfix; i++)
		if (strcmp(modify->fix[i]->style,"freeze") == 0) break;
	if (i < modify->nfix) freeze_group_bit = modify->fix[i]->groupbit;
	else freeze_group_bit = 0;

	// check for FixRigid so can extract rigid body masses

	fix_rigid = NULL;
	for (i = 0; i < modify->nfix; i++)
		if (modify->fix[i]->rigid_flag) break;
	if (i < modify->nfix) fix_rigid = modify->fix[i];

	// check for FixPour and FixDeposit so can extract particle radii

	int ipour;
	for (ipour = 0; ipour < modify->nfix; ipour++)
		if (strcmp(modify->fix[ipour]->style,"pour") == 0) break;
	if (ipour == modify->nfix) ipour = -1;

	int idep;
	for (idep = 0; idep < modify->nfix; idep++)
		if (strcmp(modify->fix[idep]->style,"deposit") == 0) break;
	if (idep == modify->nfix) idep = -1;

	// set maxrad_dynamic and maxrad_frozen for each type
	// include future FixPour and FixDeposit particles as dynamic

	int itype;
	for (i = 1; i <= atom->ntypes; i++) {
		onerad_dynamic[i] = onerad_frozen[i] = 0.0;
		if (ipour >= 0) {
			itype = i;
			onerad_dynamic[i] =
				*((double *) modify->fix[ipour]->extract("radius",itype));
		}
		if (idep >= 0) {
			itype = i;
			onerad_dynamic[i] =
				*((double *) modify->fix[idep]->extract("radius",itype));
		}
	}

	double *radius = atom->radius;
	int *mask = atom->mask;
	int *type = atom->type;
	int nlocal = atom->nlocal;

	for (i = 0; i < nlocal; i++) 
		if (mask[i] & freeze_group_bit)
			onerad_frozen[type[i]] = MAX(onerad_frozen[type[i]],radius[i]);
		else
			onerad_dynamic[type[i]] = MAX(onerad_dynamic[type[i]],radius[i]);

	MPI_Allreduce(&onerad_dynamic[1],&maxrad_dynamic[1],atom->ntypes,
			MPI_DOUBLE,MPI_MAX,world);
	MPI_Allreduce(&onerad_frozen[1],&maxrad_frozen[1],atom->ntypes,
			MPI_DOUBLE,MPI_MAX,world);

	// set fix which stores history info

	if (history) {
		int ifix = modify->find_fix("NEIGH_HISTORY");
		if (ifix < 0) error->all(FLERR,"Could not find pair fix neigh history ID");
		fix_history = (FixNeighHistory *) modify->fix[ifix];
	}
}

/* ----------------------------------------------------------------------
	 init for one type pair i,j and corresponding j,i
	 ------------------------------------------------------------------------- */

double PairGranJKRRollingMulti::init_one(int i, int j)
{
	if (setflag[i][j] == 0) {
		E[i][j] = mix_stiffnessE(E[i][i],E[j][j],G[i][i],G[j][j]);
		G[i][j] = mix_stiffnessG(G[i][i],E[j][j],G[i][i],G[j][j]);
		if (normaldamp[i][j] == TSUJI) {
			alpha[i][j] = mix_geom(alpha[i][i],alpha[j][j]);
		}
		else if (normaldamp[i][j] == BRILLIANTOV) {
			gamman[i][j] = mix_geom(gamman[i][i],gamman[j][j]);
		}
		muS[i][j] = mix_geom(muS[i][i],muS[j][j]);
		Ecoh[i][j] = mix_geom(Ecoh[i][i],Ecoh[j][j]);
		kR[i][j] = mix_geom(kR[i][i],kR[j][j]);
		etaR[i][j] = mix_geom(etaR[i][i],etaR[j][j]);
		muR[i][j] = mix_geom(muR[i][i],muR[j][j]);
	}

	E[j][i] = E[i][j];
	G[j][i] = G[i][j];
	normaldamp[j][i] = normaldamp[i][j];
	alpha[j][i] = alpha[i][j];
	gamman[j][i] = gamman[i][j];
	rollingdamp[j][i] = rollingdamp[i][j];
	muS[j][i] = muS[i][j];
	Ecoh[j][i] = Ecoh[i][j];
	kR[j][i] = kR[i][j];
	etaR[j][i] = etaR[i][j];
	muR[j][i] = muR[i][j];

	double cutoff = cut[i][j];

	// It is likely that cut[i][j] at this point is still 0.0. This can happen when 
	// there is a future fix_pour after the current run. A cut[i][j] = 0.0 creates
	// problems because neighbor.cpp uses min(cut[i][j]) to decide on the bin size
	// To avoid this issue,for cases involving  cut[i][j] = 0.0 (possible only
	// if there is no current information about radius/cutoff of type i and j).
	// we assign cutoff = min(cut[i][j]) for i,j such that cut[i][j] > 0.0.

	if (cut[i][j] < 0.0) {
		if (((maxrad_dynamic[i] > 0.0) && (maxrad_dynamic[j] > 0.0)) || ((maxrad_dynamic[i] > 0.0) && (maxrad_frozen[j] > 0.0)) ||
				((maxrad_frozen[i] > 0.0) && (maxrad_dynamic[j] > 0.0))) { // radius info about both i and j exist
			cutoff = maxrad_dynamic[i]+maxrad_dynamic[j];
			cutoff = MAX(cutoff,maxrad_frozen[i]+maxrad_dynamic[j]);
			cutoff = MAX(cutoff,maxrad_dynamic[i]+maxrad_frozen[j]);
		}
		else { // radius info about either i or j does not exist (i.e. not present and not about to get poured; set to largest value to not interfere with neighbor list)
			double cutmax = 0.0;
			for (int k = 1; k <= atom->ntypes; k++) {
				cutmax = MAX(cutmax,2.0*maxrad_dynamic[k]);
				cutmax = MAX(cutmax,2.0*maxrad_frozen[k]);
			}
			cutoff = cutmax;
		}
	}
	return cutoff;
}


/* ----------------------------------------------------------------------
	 proc 0 writes to restart file
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::write_restart(FILE *fp)
{
	write_restart_settings(fp);

	int i,j;
	for (i = 1; i <= atom->ntypes; i++) {
		for (j = i; j <= atom->ntypes; j++) {
			fwrite(&setflag[i][j],sizeof(int),1,fp);
	    if (setflag[i][j]) {
				fwrite(&E[i][j],sizeof(double),1,fp);
				fwrite(&G[i][j],sizeof(double),1,fp);
				fwrite(&normaldamp[i][j],sizeof(int),1,fp);
				fwrite(&rollingdamp[i][j],sizeof(int),1,fp);
				fwrite(&alpha[i][j],sizeof(double),1,fp);
				fwrite(&gamman[i][j],sizeof(double),1,fp);
				fwrite(&muS[i][j],sizeof(double),1,fp);
				fwrite(&Ecoh[i][j],sizeof(double),1,fp);
				fwrite(&kR[i][j],sizeof(double),1,fp);
				fwrite(&muR[i][j],sizeof(double),1,fp);
				fwrite(&etaR[i][j],sizeof(double),1,fp);
				fwrite(&cut[i][j],sizeof(double),1,fp);
			}
		}
	}
}

/* ----------------------------------------------------------------------
	 proc 0 reads from restart file, bcasts
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::read_restart(FILE *fp)
{
	read_restart_settings(fp);
	allocate();

	int i,j;
	int me = comm->me;
	for (i = 1; i <= atom->ntypes; i++) {
		for (j = i; j <= atom->ntypes; j++) {
			if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
			MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
			if (setflag[i][j]) {
				if (me == 0) {
					fread(&E[i][j],sizeof(double),1,fp);
					fread(&G[i][j],sizeof(double),1,fp);
					fread(&normaldamp[i][j],sizeof(int),1,fp);
					fread(&rollingdamp[i][j],sizeof(int),1,fp);
					fread(&alpha[i][j],sizeof(double),1,fp);
					fread(&gamman[i][j],sizeof(double),1,fp);
					fread(&muS[i][j],sizeof(double),1,fp);
					fread(&Ecoh[i][j],sizeof(double),1,fp);
					fread(&kR[i][j],sizeof(double),1,fp);
					fread(&muR[i][j],sizeof(double),1,fp);
					fread(&etaR[i][j],sizeof(double),1,fp);
					fread(&cut[i][j],sizeof(double),1,fp);
				}
				MPI_Bcast(&E[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&G[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&normaldamp[i][j],1,MPI_INT,0,world);
				MPI_Bcast(&rollingdamp[i][j],1,MPI_INT,0,world);
				MPI_Bcast(&alpha[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&gamman[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&muS[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&Ecoh[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&kR[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&muR[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&etaR[i][j],1,MPI_DOUBLE,0,world);
				MPI_Bcast(&cut[i][j],1,MPI_DOUBLE,0,world);
			}
		}
	}
}

/* ----------------------------------------------------------------------
	 proc 0 writes to restart file
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::write_restart_settings(FILE *fp)
{
	fwrite(&cut_global,sizeof(double),1,fp);
}

/* ----------------------------------------------------------------------
	 proc 0 reads from restart file, bcasts
	 ------------------------------------------------------------------------- */

void PairGranJKRRollingMulti::read_restart_settings(FILE *fp)
{
	if (comm->me == 0) {
		fread(&cut_global,sizeof(double),1,fp);
	}
	MPI_Bcast(&cut_global,1,MPI_DOUBLE,0,world);
}

/* ---------------------------------------------------------------------- */

void PairGranJKRRollingMulti::reset_dt()
{
	dt = update->dt;
}

/* ---------------------------------------------------------------------- */

double PairGranJKRRollingMulti::single(int i, int j, int itype, int jtype,
		double rsq, double factor_coul, double factor_lj, double &fforce) 
{
//	feenableexcept(FE_INVALID | FE_OVERFLOW);
	double radi,radj,radsum;
	double r,rinv,rsqinv,delx,dely,delz, nx, ny, nz, R;
	double vr1,vr2,vr3,vnnr,vn1,vn2,vn3,vt1,vt2,vt3,wr1,wr2,wr3;
	double overlap, a;
	double mi,mj,meff,damp,kn,kt;
	double Fdamp,Fne,Fntot,Fscrit;
	double eta_N,eta_T;
	double vtr1,vtr2,vtr3,vrel;
	double fs1,fs2,fs3,fs;
	double shrmag;
	double F_C, delta_C, olapsq, olapcubed, sqrtterm, tmp, a0;
	double keyterm, keyterm2, keyterm3, aovera0, foverFc;

	double *radius = atom->radius;
	radi = radius[i];
	radj = radius[j];
	radsum = radi + radj;

	r = sqrt(rsq);
	rinv = 1.0/r;
	rsqinv = 1.0/rsq;
	R = radi*radj/(radi+radj);
	a0 = pow(9.0*M_PI*Ecoh[itype][jtype]*R*R/E[itype][jtype],ONETHIRD);
	delta_C = 0.5*a0*a0*POW6ONE/R;

	int *touch = fix_history->firstflag[i];
	if ((rsq >= (radsum+delta_C)*(radsum+delta_C) )||
			(rsq >= radsum*radsum && touch[j])){
		fforce = 0.0;
		svector[0] = svector[1] = svector[2] = svector[3] = 0.0;
		return 0.0;
	}

	// relative translational velocity

	double **v = atom->v;
	vr1 = v[i][0] - v[j][0];
	vr2 = v[i][1] - v[j][1];
	vr3 = v[i][2] - v[j][2];

	// normal component

	double **x = atom->x;
	delx = x[i][0] - x[j][0];
	dely = x[i][1] - x[j][1];
	delz = x[i][2] - x[j][2];

	nx = delx*rinv;
	ny = dely*rinv;
	nz = delz*rinv;


	vnnr = vr1*nx + vr2*ny + vr3*nz;
	vn1 = nx*vnnr;
	vn2 = ny*vnnr;
	vn3 = nz*vnnr;

	// tangential component

	vt1 = vr1 - vn1;
	vt2 = vr2 - vn2;
	vt3 = vr3 - vn3;

	// relative rotational velocity

	double **omega = atom->omega;
	wr1 = (radi*omega[i][0] + radj*omega[j][0]);
	wr2 = (radi*omega[i][1] + radj*omega[j][1]);
	wr3 = (radi*omega[i][2] + radj*omega[j][2]);

	// meff = effective mass of pair of particles
	// if I or J part of rigid body, use body mass
	// if I or J is frozen, meff is other particle

	double *rmass = atom->rmass;
	int *type = atom->type;
	int *mask = atom->mask;

	mi = rmass[i];
	mj = rmass[j];
	if (fix_rigid) {
		// NOTE: ensure mass_rigid is current for owned+ghost atoms?
		if (mass_rigid[i] > 0.0) mi = mass_rigid[i];
		if (mass_rigid[j] > 0.0) mj = mass_rigid[j];
	}

	meff = mi*mj / (mi+mj);
	if (mask[i] & freeze_group_bit) meff = mj;
	if (mask[j] & freeze_group_bit) meff = mi;


	// normal force = JKR
	F_C = 3.0*R*M_PI*Ecoh[itype][jtype];
	overlap = radsum - r;
	olapsq = overlap*overlap;
	olapcubed = olapsq*olapsq;
	sqrtterm = sqrt(1.0 + olapcubed);
	tmp = 2.0 + olapcubed + 2.0*sqrtterm;
	keyterm = pow(tmp,ONETHIRD);
	keyterm2 = olapsq/keyterm;
	keyterm3 = sqrt(overlap + keyterm2 + keyterm);
	aovera0 = POW6TWO * (keyterm3 +
			sqrt(2.0*overlap - keyterm2 - keyterm + 4.0/keyterm3));// eq 41
	a = aovera0*a0;
	foverFc = 4.0*((aovera0*aovera0*aovera0) - pow(aovera0,1.5));//F_ne/F_C (eq 40)

	Fne = F_C*foverFc;

	//Damping
	kn = 4.0/3.0*E[itype][jtype]*a;
	if (normaldamp[itype][jtype] == BRILLIANTOV) eta_N = a*meff*gamman[itype][jtype];
	else if (normaldamp[itype][jtype] == TSUJI) eta_N=alpha[itype][jtype]*sqrt(meff*kn);

	Fdamp = -eta_N*vnnr; //F_nd eq 23 and Zhao eq 19

	Fntot = Fne + Fdamp;

	// relative velocities

	vtr1 = vt1 - (nz*wr2-ny*wr3);
	vtr2 = vt2 - (nx*wr3-nz*wr1);
	vtr3 = vt3 - (ny*wr1-nx*wr2);
	vrel = vtr1*vtr1 + vtr2*vtr2 + vtr3*vtr3;
	vrel = sqrt(vrel);

	// shear history effects
	// neighprev = index of found neigh on previous call
	// search entire jnum list of neighbors of I for neighbor J
	// start from neighprev, since will typically be next neighbor
	// reset neighprev to 0 as necessary

	int jnum = list->numneigh[i];
	int *jlist = list->firstneigh[i];
	double *allshear = fix_history->firstvalue[i];

	for (int jj = 0; jj < jnum; jj++) {
		neighprev++;
		if (neighprev >= jnum) neighprev = 0;
		if (jlist[neighprev] == j) break;
	}

	double *shear = &allshear[3*neighprev];
	shrmag = sqrt(shear[0]*shear[0] + shear[1]*shear[1] +
			shear[2]*shear[2]);

	// tangential forces = shear + tangential velocity damping 
	kt=8.0*G[itype][jtype]*a;

	eta_T = eta_N; 
	fs1 = -kt*shear[0] - eta_T*vtr1;
	fs2 = -kt*shear[1] - eta_T*vtr2;
	fs3 = -kt*shear[2] - eta_T*vtr3;

	// rescale frictional displacements and forces if needed

	fs = sqrt(fs1*fs1 + fs2*fs2 + fs3*fs3);
	Fscrit= muS[itype][jtype] * fabs(Fne + 2*F_C);

	if (fs > Fscrit) {
		if (shrmag != 0.0) {
			fs1 *= Fscrit/fs;
			fs2 *= Fscrit/fs;
			fs3 *= Fscrit/fs;
			fs *= Fscrit/fs;
		} else fs1 = fs2 = fs3 = fs = 0.0;
	}

	// set all forces and return no energy

	fforce = Fntot;

	// set single_extra quantities

	svector[0] = fs1;
	svector[1] = fs2;
	svector[2] = fs3;
	svector[3] = fs;
	svector[4] = vn1;
	svector[5] = vn2;
	svector[6] = vn3;
	svector[7] = vt1;
	svector[8] = vt2;
	svector[9] = vt3;
	return 0.0;
}

/* ---------------------------------------------------------------------- */

int PairGranJKRRollingMulti::pack_forward_comm(int n, int *list, double *buf,
		int pbc_flag, int *pbc)
{
	int i,j,m;

	m = 0;
	for (i = 0; i < n; i++) {
		j = list[i];
		buf[m++] = mass_rigid[j];
	}
	return m;
}

/* ---------------------------------------------------------------------- */

void PairGranJKRRollingMulti::unpack_forward_comm(int n, int first, double *buf)
{
	int i,m,last;

	m = 0;
	last = first + n;
	for (i = first; i < last; i++)
		mass_rigid[i] = buf[m++];
}

/* ----------------------------------------------------------------------
	 memory usage of local atom-based arrays
	 ------------------------------------------------------------------------- */

double PairGranJKRRollingMulti::memory_usage()
{
	double bytes = nmax * sizeof(double);
	return bytes;
}

/* ----------------------------------------------------------------------
	 mixing of stiffness (E)
	 ------------------------------------------------------------------------- */

double PairGranJKRRollingMulti::mix_stiffnessE(double Eii, double Ejj, double Gii, double Gjj)
{
	double poisii = Eii/(2.0*Gii) - 1.0;
	double poisjj = Ejj/(2.0*Gjj) - 1.0;
	return 1/((1-poisii*poisjj)/Eii+(1-poisjj*poisjj)/Ejj);
}

/* ----------------------------------------------------------------------
	 mixing of stiffness (G)
	 ------------------------------------------------------------------------- */

double PairGranJKRRollingMulti::mix_stiffnessG(double Eii, double Ejj, double Gii, double Gjj)
{
	double poisii = Eii/(2.0*Gii) - 1.0;
	double poisjj = Ejj/(2.0*Gjj) - 1.0;
	return 1/((2.0 -poisjj)/Gii+(2.0-poisjj)/Gjj);
}

/* ----------------------------------------------------------------------
	 mixing of everything else 
	 ------------------------------------------------------------------------- */

double PairGranJKRRollingMulti::mix_geom(double valii, double valjj)
{
	return sqrt(valii*valjj); 
}
