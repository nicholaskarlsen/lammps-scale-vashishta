/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   https://lammps.sandia.gov/, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   Contributing authors: Axel Kohlmeyer (Temple U), Venkatesh Botu
------------------------------------------------------------------------- */

#include "pair_agni.h"

#include "atom.h"
#include "citeme.h"
#include "comm.h"
#include "force.h"
#include "error.h"
#include "math_const.h"
#include "math_special.h"
#include "memory.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "neighbor.h"
#include "tokenizer.h"
#include "potential_file_reader.h"

#include <cmath>
#include <cstring>

using namespace LAMMPS_NS;
using namespace MathSpecial;

static const char cite_pair_agni[] =
  "pair agni command:\n\n"
  "@article{botu2015adaptive,\n"
  " author    = {Botu, Venkatesh and Ramprasad, Rampi},\n"
  " title     = {Adaptive machine learning framework to"
                " accelerate ab initio molecular dynamics},\n"
  " journal   = {International Journal of Quantum Chemistry},\n"
  " volume    = {115},\n"
  " number    = {16},\n"
  " pages     = {1074--1083},\n"
  " year      = {2015},\n"
  " publisher = {Wiley Online Library}\n"
  "}\n\n"
  "@article{botu2015learning,\n"
  " author    = {Botu, Venkatesh and Ramprasad, Rampi},\n"
  " title     = {Learning scheme to predict atomic forces"
                " and accelerate materials simulations},\n"
  " journal   = {Physical Review B},\n"
  " volume    = {92},\n"
  " number    = {9},\n"
  " pages     = {094306},\n"
  " year      = {2015},\n"
  " publisher = {APS}\n"
  "}\n\n"
  "@article{botu2017jpc,\n"
  " author    = {Botu, V. and Batra, R. and Chapman, J. and Ramprasad, Rampi},\n"
  " journal   = {J. Phys. Chem. C},\n"
  " volume    = {121},\n"
  " number    = {1},\n"
  " pages     = {511},\n"
  " year      = {2017},\n"
  "}\n\n";

#define MAXLINE 10240
#define MAXWORD 40
enum { AGNI_VERSION_UNKNOWN, AGNI_VERSION_1, AGNI_VERSION_2 };

/* ---------------------------------------------------------------------- */

PairAGNI::PairAGNI(LAMMPS *lmp) : Pair(lmp)
{
  if (lmp->citeme) lmp->citeme->add(cite_pair_agni);

  single_enable = 0;
  restartinfo = 0;
  one_coeff = 1;
  manybody_flag = 1;
  atomic_feature_version = 0;

  centroidstressflag = CENTROID_NOTAVAIL;

  no_virial_fdotr_compute = 1;

  nelements = 0;
  elements = nullptr;
  elem2param = nullptr;
  nparams = 0;
  params = nullptr;
  map = nullptr;
  cutmax = 0.0;
}

/* ----------------------------------------------------------------------
   check if allocated, since class can be destructed when incomplete
------------------------------------------------------------------------- */

PairAGNI::~PairAGNI()
{
  if (elements)
    for (int i = 0; i < nelements; i++) delete [] elements[i];
  delete [] elements;
  if (params) {
    for (int i = 0; i < nparams; ++i) {
      int n = params[i].numeta;
      for (int j = 0; j < n; ++j) {
        delete [] params[i].xU[j];
      }
      delete [] params[i].eta;
      delete [] params[i].alpha;
      delete [] params[i].xU;
    }
    memory->destroy(params);
    params = nullptr;
  }

  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    delete [] map;
  }
}

/* ---------------------------------------------------------------------- */

void PairAGNI::compute(int eflag, int vflag)
{
  int i,j,k,ii,jj,inum,jnum,itype;
  double xtmp,ytmp,ztmp,delx,dely,delz;
  double rsq;
  int *ilist,*jlist,*numneigh,**firstneigh;

  ev_init(eflag,vflag);

  double **x = atom->x;
  double **f = atom->f;
  int *type = atom->type;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  double fxtmp,fytmp,fztmp;
  double *Vx, *Vy, *Vz;

  // loop over full neighbor list of my atoms
  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    itype = map[type[i]];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    fxtmp = fytmp = fztmp = 0.0;

    const Param &iparam = params[elem2param[itype]];
    Vx = new double[iparam.numeta];
    Vy = new double[iparam.numeta];
    Vz = new double[iparam.numeta];
    memset(Vx,0,iparam.numeta*sizeof(double));
    memset(Vy,0,iparam.numeta*sizeof(double));
    memset(Vz,0,iparam.numeta*sizeof(double));

    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;

      if ((rsq > 0.0) && (rsq < iparam.cutsq)) {
        const double r = sqrt(rsq);
        const double cF = 0.5*(cos((MathConst::MY_PI*r)/iparam.cut)+1.0);
        const double wX = cF*delx/r;
        const double wY = cF*dely/r;
        const double wZ = cF*delz/r;

        for (k = 0; k < iparam.numeta; ++k) {
          double e = 0.0;

		if(atomic_feature_version == AGNI_VERSION_1)
	    e = exp(-(iparam.eta[k]*rsq));
		else if(atomic_feature_version == AGNI_VERSION_2)
			e = (1.0 / (square(iparam.eta[k]) * iparam.gwidth * sqrt(MathConst::MY_2PI))) * exp(-(square(r - iparam.eta[k])) / (2.0 * square(iparam.gwidth)));
		
          Vx[k] += wX*e;
          Vy[k] += wY*e;
          Vz[k] += wZ*e;
        }
      }
    }

    for (j = 0; j < iparam.numtrain; ++j) {
      double kx = 0.0;
      double ky = 0.0;
      double kz = 0.0;

      for (int k = 0; k < iparam.numeta; ++k) {
        const double xu = iparam.xU[k][j];

        kx += square(Vx[k] - xu);
        ky += square(Vy[k] - xu);
        kz += square(Vz[k] - xu);
      }
      const double e = -0.5/(square(iparam.sigma));
      fxtmp += iparam.alpha[j]*exp(kx*e);
      fytmp += iparam.alpha[j]*exp(ky*e);
      fztmp += iparam.alpha[j]*exp(kz*e);
    }
    fxtmp += iparam.b;
    fytmp += iparam.b;
    fztmp += iparam.b;
    f[i][0] += fxtmp;
    f[i][1] += fytmp;
    f[i][2] += fztmp;

    if (evflag) ev_tally_xyz_full(i,0.0,0.0,fxtmp,fytmp,fztmp,delx,dely,delz);

    delete [] Vx;
    delete [] Vy;
    delete [] Vz;
  }

  if (vflag_fdotr) virial_fdotr_compute();
}

/* ---------------------------------------------------------------------- */

void PairAGNI::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  memory->create(cutsq,n+1,n+1,"pair:cutsq");
  map = new int[n+1];
}

/* ----------------------------------------------------------------------
   global settings
------------------------------------------------------------------------- */

void PairAGNI::settings(int narg, char ** /* arg */)
{
  if (narg != 0) error->all(FLERR,"Illegal pair_style command");
}

/* ----------------------------------------------------------------------
   set coeffs for one or more type pairs
------------------------------------------------------------------------- */

void PairAGNI::coeff(int narg, char **arg)
{
  int i,j,n;

  if (!allocated) allocate();

  if (narg != 3 + atom->ntypes)
    error->all(FLERR,"Incorrect args for pair coefficients");

  // insure I,J args are * *

  if (strcmp(arg[0],"*") != 0 || strcmp(arg[1],"*") != 0)
    error->all(FLERR,"Incorrect args for pair coefficients");

  // read args that map atom types to elements in potential file
  // map[i] = which element the Ith atom type is, -1 if "NULL"
  // nelements = # of unique elements
  // elements = list of element names

  if (elements) {
    for (i = 0; i < nelements; i++) delete [] elements[i];
    delete [] elements;
  }
  elements = new char*[atom->ntypes];
  for (i = 0; i < atom->ntypes; i++) elements[i] = nullptr;

  nelements = 0;
  for (i = 3; i < narg; i++) {
    if (strcmp(arg[i],"NULL") == 0) {
      map[i-2] = -1;
      continue;
    }
    for (j = 0; j < nelements; j++)
      if (strcmp(arg[i],elements[j]) == 0) break;
    map[i-2] = j;
    if (j == nelements) {
      n = strlen(arg[i]) + 1;
      elements[j] = new char[n];
      strcpy(elements[j],arg[i]);
      nelements++;
    }
  }
  if (nelements != 1)
        error->all(FLERR,"Cannot handle multi-element systems with this potential");

  // read potential file and initialize potential parameters

  read_file(arg[2]);
  setup_params();

  // clear setflag since coeff() called once with I,J = * *

  n = atom->ntypes;
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      setflag[i][j] = 0;
  
  // set setflag i,j for type pairs where both are mapped to elements

  int count = 0;
  for (int i = 1; i <= n; i++)
    for (int j = i; j <= n; j++)
      if (map[i] >= 0 && map[j] >= 0) {
        setflag[i][j] = 1;
        count++;
      }

  if (count == 0) error->all(FLERR,"Incorrect args for pair coefficients");
}

/* ----------------------------------------------------------------------
   init specific to this pair style
------------------------------------------------------------------------- */

void PairAGNI::init_style()
{
  // need a full neighbor list

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->half = 0;
  neighbor->requests[irequest]->full = 1;
}

/* ----------------------------------------------------------------------
   init for one type pair i,j and corresponding j,i
------------------------------------------------------------------------- */

double PairAGNI::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  return cutmax;
}

/* ---------------------------------------------------------------------- */

void PairAGNI::read_file(char *filename)
{
  memory->sfree(params);
  params = nullptr;
  nparams = 0;

  int i,j,curparam,wantdata,fp_counter;

  fp_counter = 0;

  // read potential file
  if(comm->me == 0) {
    PotentialFileReader reader(lmp, filename, "agni", unit_convert_flag);

    try {
      ValueTokenizer values = reader.next_values(2);

      values.next_string(); // ignore
      nparams = values.next_int();

      if ((nparams < 1) || params) // sanity check
        error->all(FLERR,"Invalid AGNI potential file");
      params = memory->create(params,nparams,"pair:params");
      memset(params,0,nparams*sizeof(Param));
      
      curparam = -1;
      wantdata = -1;

      values = reader.next_values(2);
      values.next_string(); // ignore
      for (i = 0; i < nparams; ++i) {
        std::string element = values.next_string();
        for (j = 0; j < nelements; ++j)
          if (strcmp(element.c_str(),elements[j]) == 0) break;
        if (j == nelements)
          error->all(FLERR,"No suitable parameters for requested element found");
        else params[i].ielement = j;
      }

      values = reader.next_values(2);
      values.next_string(); // ignore
      for (i = 0; i < nparams; ++i){
        std::string element = values.next_string();
        if (strcmp(element.c_str(),elements[params[i].ielement]) == 0) curparam = i;
      }

      values = reader.next_values(2);
      values.next_string(); // ignore
      atomic_feature_version = values.next_int();
      if (atomic_feature_version != AGNI_VERSION_1 && atomic_feature_version != AGNI_VERSION_2)
        error->all(FLERR,"Incompatible AGNI potential file version");

      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].numeta = values.next_int();
      params[curparam].eta = new double[params[curparam].numeta];
      params[curparam].xU = new double*[params[curparam].numeta];

      values = reader.next_values(params[curparam].numeta + 1);
      values.next_string(); // ignore
      for(i = 0; i < params[curparam].numeta; i++)
        params[curparam].eta[i] = values.next_double();
      
      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].gwidth = values.next_double();
      
      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].cut = values.next_double();
      
      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].numtrain = values.next_int();
      params[curparam].alpha = new double[params[curparam].numtrain];
      for (i = 0; i < params[curparam].numeta; ++i)
        params[curparam].xU[i] = new double[params[curparam].numtrain];

      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].sigma = values.next_double();

      values = reader.next_values(2);
      values.next_string(); // ignore
      values.next_double(); // ignore

      values = reader.next_values(2);
      values.next_string(); // ignore
      params[curparam].b = values.next_double();

      values = reader.next_values(1);
      values.next_string(); // ignore
      wantdata = curparam;
      curparam = -1;

      if (params && wantdata >=0){
        for(j = 0; j < params[wantdata].numtrain; j++){
          values = reader.next_values(params[wantdata].numeta + 2);
          for (i = 0; i < params[wantdata].numeta; ++i) 
            params[wantdata].xU[i][j] = values.next_double();
          values.next_double(); // ignore
          params[wantdata].alpha[j] = values.next_double();   
        } 
      }else
        error->all(FLERR,"Invalid AGNI potential file");
    } catch (TokenizerException &e) {
      error->one(FLERR, e.what());
    }
  }
  
  MPI_Bcast(&nparams, 1, MPI_INT, 0, world);
  MPI_Bcast(&atomic_feature_version, 1, MPI_INT, 0, world);
  if(comm->me != 0) {
    params = memory->create(params,nparams,"pair:params");
    memset(params,0,nparams*sizeof(Param));
  }
  MPI_Bcast(params, nparams*sizeof(Param), MPI_BYTE, 0, world);
}

/* ---------------------------------------------------------------------- */

void PairAGNI::setup_params()
{
  int i,m,n;
  double rtmp;

  // set elem2param for all elements

  memory->destroy(elem2param);
  memory->create(elem2param,nelements,"pair:elem2param");

  for (i = 0; i < nelements; i++) {
    n = -1;
    for (m = 0; m < nparams; m++) {
      if (i == params[m].ielement) {
        if (n >= 0) error->all(FLERR,"Potential file has duplicate entry");
        n = m;
      }
    }
    if (n < 0) error->all(FLERR,"Potential file is missing an entry");
    elem2param[i] = n;
  }

  // compute parameter values derived from inputs

  // set cutsq using shortcut to reduce neighbor list for accelerated
  // calculations. cut must remain unchanged as it is a potential parameter
  // (cut = a*sigma)

  cutmax = 0.0;
  for (m = 0; m < nparams; m++) {
    rtmp = params[m].cut;
    params[m].cutsq = rtmp * rtmp;
    if (rtmp > cutmax) cutmax = rtmp;
  }
}

