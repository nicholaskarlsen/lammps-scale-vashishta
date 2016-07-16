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

/* ----------------------------------------------------------------------
   Contributing authors: Leo Silbert (SNL), Gary Grest (SNL)
------------------------------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pair_gran_hooke_history.h"
#include "atom.h"
#include "atom_vec.h"
#include "domain.h"
#include "force.h"
#include "update.h"
#include "modify.h"
#include "fix.h"
#include "fix_shear_history.h"
#include "comm.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

PairGranHookeHistory::PairGranHookeHistory(LAMMPS *lmp) : Pair(lmp)
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

PairGranHookeHistory::~PairGranHookeHistory()
{
  delete [] svector;
  if (fix_history) modify->delete_fix("SHEAR_HISTORY");

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);

    delete [] onerad_dynamic;
    delete [] onerad_frozen;
    delete [] maxrad_dynamic;
    delete [] maxrad_frozen;
  }

  memory->destroy(mass_rigid);
}

/* ---------------------------------------------------------------------- */

void PairGranHookeHistory::compute(int eflag, int vflag)
{
  int i,j,ii,jj,inum,jnum;
  double xtmp,ytmp,ztmp,delx,dely,delz,fx,fy,fz;
  double radi,radj,radsum,rsq,r,rinv,rsqinv;
  double vr1,vr2,vr3,vnnr,vn1,vn2,vn3,vt1,vt2,vt3;
  double wr1,wr2,wr3;
  double vtr1,vtr2,vtr3,vrel;
  double mi,mj,meff,damp,ccel,tor1,tor2,tor3;
  double fn,fs,fs1,fs2,fs3;
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

  if (fix_rigid && neighbor->ago == 0) {
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
  firsttouch = listgranhistory->firstneigh;
  firstshear = listgranhistory->firstdouble;

  // loop over neighbors of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
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
      rsq = delx*delx + dely*dely + delz*delz;
      radj = radius[j];
      radsum = radi + radj;

      if (rsq >= radsum*radsum) {

        // unset non-touching neighbors

        touch[jj] = 0;
        shear = &allshear[3*jj];
        shear[0] = 0.0;
        shear[1] = 0.0;
        shear[2] = 0.0;

      } else {
        r = sqrt(rsq);
        rinv = 1.0/r;
        rsqinv = 1.0/rsq;

        // relative translational velocity

        vr1 = v[i][0] - v[j][0];
        vr2 = v[i][1] - v[j][1];
        vr3 = v[i][2] - v[j][2];

        // normal component

        vnnr = vr1*delx + vr2*dely + vr3*delz;
        vn1 = delx*vnnr * rsqinv;
        vn2 = dely*vnnr * rsqinv;
        vn3 = delz*vnnr * rsqinv;

        // tangential component

        vt1 = vr1 - vn1;
        vt2 = vr2 - vn2;
        vt3 = vr3 - vn3;

        // relative rotational velocity

        wr1 = (radi*omega[i][0] + radj*omega[j][0]) * rinv;
        wr2 = (radi*omega[i][1] + radj*omega[j][1]) * rinv;
        wr3 = (radi*omega[i][2] + radj*omega[j][2]) * rinv;

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

        // normal forces = Hookian contact + normal velocity damping

        damp = meff*gamman*vnnr*rsqinv;
        ccel = kn*(radsum-r)*rinv - damp;

        // relative velocities

        vtr1 = vt1 - (delz*wr2-dely*wr3);
        vtr2 = vt2 - (delx*wr3-delz*wr1);
        vtr3 = vt3 - (dely*wr1-delx*wr2);
        vrel = vtr1*vtr1 + vtr2*vtr2 + vtr3*vtr3;
        vrel = sqrt(vrel);

        // shear history effects

        touch[jj] = 1;
        shear = &allshear[3*jj];

        if (shearupdate) {
          shear[0] += vtr1*dt;
          shear[1] += vtr2*dt;
          shear[2] += vtr3*dt;
        }
        shrmag = sqrt(shear[0]*shear[0] + shear[1]*shear[1] +
                      shear[2]*shear[2]);

        // rotate shear displacements

        rsht = shear[0]*delx + shear[1]*dely + shear[2]*delz;
        rsht *= rsqinv;
        if (shearupdate) {
          shear[0] -= rsht*delx;
          shear[1] -= rsht*dely;
          shear[2] -= rsht*delz;
        }

        // tangential forces = shear + tangential velocity damping

        fs1 = - (kt*shear[0] + meff*gammat*vtr1);
        fs2 = - (kt*shear[1] + meff*gammat*vtr2);
        fs3 = - (kt*shear[2] + meff*gammat*vtr3);

        // rescale frictional displacements and forces if needed

        fs = sqrt(fs1*fs1 + fs2*fs2 + fs3*fs3);
        fn = xmu * fabs(ccel*r);

        if (fs > fn) {
          if (shrmag != 0.0) {
            shear[0] = (fn/fs) * (shear[0] + meff*gammat*vtr1/kt) -
              meff*gammat*vtr1/kt;
            shear[1] = (fn/fs) * (shear[1] + meff*gammat*vtr2/kt) -
              meff*gammat*vtr2/kt;
            shear[2] = (fn/fs) * (shear[2] + meff*gammat*vtr3/kt) -
              meff*gammat*vtr3/kt;
            fs1 *= fn/fs;
            fs2 *= fn/fs;
            fs3 *= fn/fs;
          } else fs1 = fs2 = fs3 = 0.0;
        }

        // forces & torques

        fx = delx*ccel + fs1;
        fy = dely*ccel + fs2;
        fz = delz*ccel + fs3;
        f[i][0] += fx;
        f[i][1] += fy;
        f[i][2] += fz;

        tor1 = rinv * (dely*fs3 - delz*fs2);
        tor2 = rinv * (delz*fs1 - delx*fs3);
        tor3 = rinv * (delx*fs2 - dely*fs1);
        torque[i][0] -= radi*tor1;
        torque[i][1] -= radi*tor2;
        torque[i][2] -= radi*tor3;

        if (newton_pair || j < nlocal) {
          f[j][0] -= fx;
          f[j][1] -= fy;
          f[j][2] -= fz;
          torque[j][0] -= radj*tor1;
          torque[j][1] -= radj*tor2;
          torque[j][2] -= radj*tor3;
        }

        if (evflag) ev_tally_xyz(i,j,nlocal,newton_pair,
                                 0.0,0.0,fx,fy,fz,delx,dely,delz);
      }
    }
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ----------------------------------------------------------------------
   allocate all arrays
------------------------------------------------------------------------- */

void PairGranHookeHistory::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;

  memory->create(cutsq,n+1,n+1,"pair:cutsq");

  onerad_dynamic = new double[n+1];
  onerad_frozen = new double[n+1];
  maxrad_dynamic = new double[n+1];
  maxrad_frozen = new double[n+1];
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairGranHookeHistory::settings(int narg, char **arg)
{
  if (narg != 6) error->all(FLERR,"Illegal pair_style command");

  kn = force->numeric(FLERR,arg[0]);
  if (strcmp(arg[1],"NULL") == 0) kt = kn * 2.0/7.0;
  else kt = force->numeric(FLERR,arg[1]);

  gamman = force->numeric(FLERR,arg[2]);
  if (strcmp(arg[3],"NULL") == 0) gammat = 0.5 * gamman;
  else gammat = force->numeric(FLERR,arg[3]);

  xmu = force->numeric(FLERR,arg[4]);
  dampflag = force->inumeric(FLERR,arg[5]);
  if (dampflag == 0) gammat = 0.0;

  if (kn < 0.0 || kt < 0.0 || gamman < 0.0 || gammat < 0.0 ||
      xmu < 0.0 || xmu > 10000.0 || dampflag < 0 || dampflag > 1)
    error->all(FLERR,"Illegal pair_style command");
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairGranHookeHistory::coeff(int narg, char **arg)
{
  if (narg > 2) error->all(FLERR,"Incorrect args for pair coefficients");
  if (!allocated) allocate();

  int ilo,ihi,jlo,jhi;
  force->bounds(arg[0],atom->ntypes,ilo,ihi);
  force->bounds(arg[1],atom->ntypes,jlo,jhi);

  int count = 0;
  for (int i = ilo; i <= ihi; i++) {
    for (int j = MAX(jlo,i); j <= jhi; j++) {
      setflag[i][j] = 1;
      count++;
    }
  }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients");
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairGranHookeHistory::init_style()
{
  int i;

  // error and warning checks

  if (!atom->radius_flag || !atom->rmass_flag)
    error->all(FLERR,"Pair granular requires atom attributes radius, rmass");
  if (comm->ghost_velocity == 0)
    error->all(FLERR,"Pair granular requires ghost atoms store velocity");

  // need a granular neigh list and optionally a granular history neigh list

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->gran = 1;
  if (history) {
    irequest = neighbor->request(this,instance_me);
    neighbor->requests[irequest]->id = 1;
    neighbor->requests[irequest]->half = 0;
    neighbor->requests[irequest]->granhistory = 1;
    neighbor->requests[irequest]->dnum = 3;
  }

  dt = update->dt;

  // if shear history is stored:
  // if first init, create Fix needed for storing shear history

  if (history && fix_history == NULL) {
    char dnumstr[16];
    sprintf(dnumstr,"%d",3);
    char **fixarg = new char*[4];
    fixarg[0] = (char *) "SHEAR_HISTORY";
    fixarg[1] = (char *) "all";
    fixarg[2] = (char *) "SHEAR_HISTORY";
    fixarg[3] = dnumstr;
    modify->add_fix(4,fixarg,1);
    delete [] fixarg;
    fix_history = (FixShearHistory *) modify->fix[modify->nfix-1];
    fix_history->pair = this;
    neighbor->requests[irequest]->fix_history = fix_history;
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
    int ifix = modify->find_fix("SHEAR_HISTORY");
    if (ifix < 0) error->all(FLERR,"Could not find pair fix ID");
    fix_history = (FixShearHistory *) modify->fix[ifix];
  }
}

/* ----------------------------------------------------------------------
   neighbor callback to inform pair style of neighbor list to use
   optional granular history list
------------------------------------------------------------------------- */

void PairGranHookeHistory::init_list(int id, NeighList *ptr)
{
  if (id == 0) list = ptr;
  else if (id == 1) listgranhistory = ptr;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairGranHookeHistory::init_one(int i, int j)
{
  if (!allocated) allocate();

  // cutoff = sum of max I,J radii for
  // dynamic/dynamic & dynamic/frozen interactions, but not frozen/frozen

  double cutoff = maxrad_dynamic[i]+maxrad_dynamic[j];
  cutoff = MAX(cutoff,maxrad_frozen[i]+maxrad_dynamic[j]);
  cutoff = MAX(cutoff,maxrad_dynamic[i]+maxrad_frozen[j]);
  return cutoff;
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairGranHookeHistory::write_restart(FILE *fp)
{
  write_restart_settings(fp);

  int i,j;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++)
      fwrite(&setflag[i][j],sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairGranHookeHistory::read_restart(FILE *fp)
{
  read_restart_settings(fp);
  allocate();

  int i,j;
  int me = comm->me;
  for (i = 1; i <= atom->ntypes; i++)
    for (j = i; j <= atom->ntypes; j++) {
      if (me == 0) fread(&setflag[i][j],sizeof(int),1,fp);
      MPI_Bcast(&setflag[i][j],1,MPI_INT,0,world);
    }
}

/* ----------------------------------------------------------------------
  proc 0 writes to restart file
------------------------------------------------------------------------- */

void PairGranHookeHistory::write_restart_settings(FILE *fp)
{
  fwrite(&kn,sizeof(double),1,fp);
  fwrite(&kt,sizeof(double),1,fp);
  fwrite(&gamman,sizeof(double),1,fp);
  fwrite(&gammat,sizeof(double),1,fp);
  fwrite(&xmu,sizeof(double),1,fp);
  fwrite(&dampflag,sizeof(int),1,fp);
}

/* ----------------------------------------------------------------------
  proc 0 reads from restart file, bcasts
------------------------------------------------------------------------- */

void PairGranHookeHistory::read_restart_settings(FILE *fp)
{
  if (comm->me == 0) {
    fread(&kn,sizeof(double),1,fp);
    fread(&kt,sizeof(double),1,fp);
    fread(&gamman,sizeof(double),1,fp);
    fread(&gammat,sizeof(double),1,fp);
    fread(&xmu,sizeof(double),1,fp);
    fread(&dampflag,sizeof(int),1,fp);
  }
  MPI_Bcast(&kn,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&kt,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&gamman,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&gammat,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&xmu,1,MPI_DOUBLE,0,world);
  MPI_Bcast(&dampflag,1,MPI_INT,0,world);
}

/* ---------------------------------------------------------------------- */

void PairGranHookeHistory::reset_dt()
{
  dt = update->dt;
}

/* ---------------------------------------------------------------------- */

double PairGranHookeHistory::single(int i, int j, int itype, int jtype,
                                    double rsq,
                                    double factor_coul, double factor_lj,
                                    double &fforce)
{
  double radi,radj,radsum;
  double r,rinv,rsqinv,delx,dely,delz;
  double vr1,vr2,vr3,vnnr,vn1,vn2,vn3,vt1,vt2,vt3,wr1,wr2,wr3;
  double mi,mj,meff,damp,ccel;
  double vtr1,vtr2,vtr3,vrel,shrmag,rsht;
  double fs1,fs2,fs3,fs,fn;

  double *radius = atom->radius;
  radi = radius[i];
  radj = radius[j];
  radsum = radi + radj;

  if (rsq >= radsum*radsum) {
    fforce = 0.0;
    for (int m = 0; m < single_extra; m++) svector[m] = 0.0;
    return 0.0;
  }

  r = sqrt(rsq);
  rinv = 1.0/r;
  rsqinv = 1.0/rsq;

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

  vnnr = vr1*delx + vr2*dely + vr3*delz;
  vn1 = delx*vnnr * rsqinv;
  vn2 = dely*vnnr * rsqinv;
  vn3 = delz*vnnr * rsqinv;

  // tangential component

  vt1 = vr1 - vn1;
  vt2 = vr2 - vn2;
  vt3 = vr3 - vn3;

  // relative rotational velocity

  double **omega = atom->omega;
  wr1 = (radi*omega[i][0] + radj*omega[j][0]) * rinv;
  wr2 = (radi*omega[i][1] + radj*omega[j][1]) * rinv;
  wr3 = (radi*omega[i][2] + radj*omega[j][2]) * rinv;

  // meff = effective mass of pair of particles
  // if I or J part of rigid body, use body mass
  // if I or J is frozen, meff is other particle

  double *rmass = atom->rmass;
  int *mask = atom->mask;

  mi = rmass[i];
  mj = rmass[j];
  if (fix_rigid) {
    // NOTE: insure mass_rigid is current for owned+ghost atoms?
    if (mass_rigid[i] > 0.0) mi = mass_rigid[i];
    if (mass_rigid[j] > 0.0) mj = mass_rigid[j];
  }

  meff = mi*mj / (mi+mj);
  if (mask[i] & freeze_group_bit) meff = mj;
  if (mask[j] & freeze_group_bit) meff = mi;

  // normal forces = Hookian contact + normal velocity damping

  damp = meff*gamman*vnnr*rsqinv;
  ccel = kn*(radsum-r)*rinv - damp;

  // relative velocities

  vtr1 = vt1 - (delz*wr2-dely*wr3);
  vtr2 = vt2 - (delx*wr3-delz*wr1);
  vtr3 = vt3 - (dely*wr1-delx*wr2);
  vrel = vtr1*vtr1 + vtr2*vtr2 + vtr3*vtr3;
  vrel = sqrt(vrel);

  // shear history effects
  // neighprev = index of found neigh on previous call
  // search entire jnum list of neighbors of I for neighbor J
  // start from neighprev, since will typically be next neighbor
  // reset neighprev to 0 as necessary

  int jnum = list->numneigh[i];
  int *jlist = list->firstneigh[i];
  double *allshear = list->listgranhistory->firstdouble[i];

  for (int jj = 0; jj < jnum; jj++) {
    neighprev++;
    if (neighprev >= jnum) neighprev = 0;
    if (jlist[neighprev] == j) break;
  }

  double *shear = &allshear[3*neighprev];
  shrmag = sqrt(shear[0]*shear[0] + shear[1]*shear[1] +
                shear[2]*shear[2]);

  // rotate shear displacements

  rsht = shear[0]*delx + shear[1]*dely + shear[2]*delz;
  rsht *= rsqinv;

  // tangential forces = shear + tangential velocity damping

  fs1 = - (kt*shear[0] + meff*gammat*vtr1);
  fs2 = - (kt*shear[1] + meff*gammat*vtr2);
  fs3 = - (kt*shear[2] + meff*gammat*vtr3);

  // rescale frictional displacements and forces if needed

  fs = sqrt(fs1*fs1 + fs2*fs2 + fs3*fs3);
  fn = xmu * fabs(ccel*r);

  if (fs > fn) {
    if (shrmag != 0.0) {
      fs1 *= fn/fs;
      fs2 *= fn/fs;
      fs3 *= fn/fs;
      fs *= fn/fs;
    } else fs1 = fs2 = fs3 = fs = 0.0;
  }

  // set force and return no energy

  fforce = ccel;

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

int PairGranHookeHistory::pack_forward_comm(int n, int *list, double *buf,
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

void PairGranHookeHistory::unpack_forward_comm(int n, int first, double *buf)
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

double PairGranHookeHistory::memory_usage()
{
  double bytes = nmax * sizeof(double);
  return bytes;
}
