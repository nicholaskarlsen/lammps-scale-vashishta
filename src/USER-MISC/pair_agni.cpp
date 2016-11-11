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
   Contributing author: Axel Kohlmeyer (Temple U)
------------------------------------------------------------------------- */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pair_agni.h"
#include "atom.h"
#include "neighbor.h"
#include "neigh_request.h"
#include "force.h"
#include "comm.h"
#include "memory.h"
#include "neighbor.h"
#include "neigh_list.h"
#include "memory.h"
#include "error.h"
#include "citeme.h"

using namespace LAMMPS_NS;

#define AGNI_VERSION 1

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
  "}\n\n";


#define MAXLINE 10240
#define MAXWORD 40
#define DELTA 4

/* ---------------------------------------------------------------------- */

PairAGNI::PairAGNI(LAMMPS *lmp) : Pair(lmp)
{
  if (lmp->citeme) lmp->citeme->add(cite_pair_agni);

  single_enable = 0;
  restartinfo = 0;
  one_coeff = 1;
  manybody_flag = 1;

  nelements = 0;
  elements = NULL;
  nparams = 0;
  params = NULL;
  map = NULL;
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
      delete [] params[i].yU;
    }
    memory->destroy(params);
    params = NULL;
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
  int i,j,k,ii,jj,kk,inum,jnum,jnumm1;
  int itype,jtype,ktype,ijparam,ikparam,ijkparam;
  tagint itag,jtag;
  double xtmp,ytmp,ztmp,delx,dely,delz,evdwl,fpair;
  double rsq,rsq1,rsq2;
  double delr1[3],delr2[3],fj[3],fk[3];
  int *ilist,*jlist,*numneigh,**firstneigh;

  evdwl = 0.0;
  if (eflag || vflag) ev_setup(eflag,vflag);
  else evflag = vflag_fdotr = 0;

  double **x = atom->x;
  double **f = atom->f;
  tagint *tag = atom->tag;
  int *type = atom->type;
  int nlocal = atom->nlocal;
  int newton_pair = force->newton_pair;

  inum = list->inum;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  double fxtmp,fytmp,fztmp;

  // loop over full neighbor list of my atoms

  for (ii = 0; ii < inum; ii++) {
    i = ilist[ii];
    itag = tag[i];
    itype = map[type[i]];
    xtmp = x[i][0];
    ytmp = x[i][1];
    ztmp = x[i][2];
    fxtmp = fytmp = fztmp = 0.0;

    jlist = firstneigh[i];
    jnum = numneigh[i];

    for (jj = 0; jj < jnum; jj++) {
      j = jlist[jj];
      j &= NEIGHMASK;

      delx = xtmp - x[j][0];
      dely = ytmp - x[j][1];
      delz = ztmp - x[j][2];
      rsq = delx*delx + dely*dely + delz*delz;

      jtype = map[type[j]];
      // ijparam = elem2param[itype][jtype];
      if (rsq < params[ijparam].cutsq) {

        // XXX compute force

        fxtmp += delx*fpair;
        fytmp += dely*fpair;
        fztmp += delz*fpair;

        if (evflag) ev_tally(i,j,nlocal,newton_pair,
                             0,0.0,fpair,delx,dely,delz);
      }
    }
    f[i][0] += fxtmp;
    f[i][1] += fytmp;
    f[i][2] += fztmp;
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

void PairAGNI::settings(int narg, char **arg)
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
  // map[i] = which element the Ith atom type is, -1 if NULL
  // nelements = # of unique elements
  // elements = list of element names

  if (elements) {
    for (i = 0; i < nelements; i++) delete [] elements[i];
    delete [] elements;
  }
  elements = new char*[atom->ntypes];
  for (i = 0; i < atom->ntypes; i++) elements[i] = NULL;

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

void PairAGNI::read_file(char *file)
{
  memory->sfree(params);
  params = NULL;
  nparams = 0;

  // open file on proc 0

  FILE *fp;
  if (comm->me == 0) {
    fp = force->open_potential(file);
    if (fp == NULL) {
      char str[128];
      sprintf(str,"Cannot open AGNI potential file %s",file);
      error->one(FLERR,str);
    }
  }

  // read each set of params from potential file
  // one set of params can span multiple lines
  // store params if all 3 element tags are in element list

  int i,j,n,nwords,curparam,wantdata,numdata;
  char line[MAXLINE],*ptr;
  int eof = 0;
  char **words = new char*[MAXWORD+1];

  while (1) {
    n = 0;
    if (comm->me == 0) {
      ptr = fgets(line,MAXLINE,fp);
      if (ptr == NULL) {
        eof = 1;
        fclose(fp);
      } else n = strlen(line) + 1;
    }
    MPI_Bcast(&eof,1,MPI_INT,0,world);
    if (eof) break;
    MPI_Bcast(&n,1,MPI_INT,0,world);
    MPI_Bcast(line,n,MPI_CHAR,0,world);

    // strip comment, skip line if blank

    if ((ptr = strchr(line,'#'))) *ptr = '\0';
    nwords = atom->count_words(line);
    if (nwords == 0) continue;

    if (nwords > MAXWORD)
      error->all(FLERR,"Increase MAXWORD and recompile");

    // words = ptrs to all words in line

    nwords = 0;
    words[nwords++] = strtok(line," \t\n\r\f");
    while ((words[nwords++] = strtok(NULL," \t\n\r\f"))) continue;
    --nwords;

    if ((nwords == 2) && (strcmp(words[0],"generation") == 0)) {
      int ver = atoi(words[1]);
      if (ver != AGNI_VERSION)
        error->all(FLERR,"Incompatible AGNI potential file version");
      if ((ver == 1) && (nelements != 1))
        error->all(FLERR,"Cannot handle multi-element systems with this potential");

    } else if ((nwords == 2) && (strcmp(words[0],"n_elements") == 0)) {
      nparams = atoi(words[1]);
      if ((nparams < 1) || params) // sanity check
        error->all(FLERR,"Invalid AGNI potential file");
      params = memory->create(params,nparams,"pair:params");
      memset(params,0,nparams*sizeof(Param));
      curparam = -1;

    } else if (params && (nwords == nparams+1) && (strcmp(words[0],"element") == 0)) {
      wantdata = -1;
      for (i = 0; i < nparams; ++i) {
        for (j = 0; j < nelements; ++j)
          if (strcmp(words[i+1],elements[j]) == 0) break;
        if (j == nelements) params[nparams].ielement = params[nparams].jelement = -1;
        else params[nparams].ielement = params[nparams].jelement = j;
      }
    } else if (params && (nwords == 2) && (strcmp(words[0],"interaction") == 0)) {
      for (i = 0; i < nparams; ++i)
        if (strcmp(words[1],elements[params[i].ielement]) == 0) curparam = i;
    } else if ((curparam >=0) && (nwords == 1) && (strcmp(words[0],"endVar") == 0)) {
      int numtrain = params[curparam].numtrain;
      int numeta = params[curparam].numeta;
      params[curparam].alpha = new double[numtrain];
      params[curparam].yU = new double[numtrain];
      params[curparam].xU = new double*[numeta];
      for (i = 0; i < numeta; ++i)
        params[curparam].xU[i] = new double[numtrain];
      
      wantdata = curparam;
      curparam = -1;
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"Rc") == 0)) {
      params[curparam].cut = atof(words[1]);
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"Rs") == 0)) {
      ; // ignored
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"neighbors") == 0)) {
      ; // ignored
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"sigma") == 0)) {
      params[curparam].sigma = atof(words[1]);
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"lambda") == 0)) {
      params[curparam].lambda = atof(words[1]);
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"b") == 0)) {
      params[curparam].b = atof(words[1]);
    } else if ((curparam >=0) && (nwords == 2) && (strcmp(words[0],"n_train") == 0)) {
      params[curparam].numtrain = atoi(words[1]);
    } else if ((curparam >=0) && (nwords > 1) && (strcmp(words[0],"eta") == 0)) {
      params[curparam].numeta = nwords-1;
      params[curparam].eta = new double[nwords-1];
      for (i = 0, j = 1 ; j < nwords; ++i, ++j)
        params[curparam].eta[i] = atof(words[j]);
    } else if (params && (wantdata >=0) && (nwords == params[wantdata].numeta+3)) {
      n = (int) atof(words[0]);
      for (i = 0; i < params[wantdata].numeta; ++i) {
        params[wantdata].xU[i][n] = atof(words[i+1]);
      }
      params[wantdata].yU[n] = atof(words[params[wantdata].numeta+1]);
      params[wantdata].alpha[n] = atof(words[params[wantdata].numeta+2]);
      
    } else {
      if (comm->me == 0)
        error->warning(FLERR,"Ignoring unknown content in AGNI potential file.");
    }
  }

  delete [] words;
}

/* ---------------------------------------------------------------------- */

void PairAGNI::setup_params()
{
  int i,j,k,m,n;
  double rtmp;

#if 0
  // set elem2param for all triplet combinations
  // must be a single exact match to lines read from file
  // do not allow for ACB in place of ABC

  memory->destroy(elem2param);
  memory->create(elem2param,nelements,nelements,nelements,"pair:elem2param");

  for (i = 0; i < nelements; i++)
    for (j = 0; j < nelements; j++)
      for (k = 0; k < nelements; k++) {
        n = -1;
        for (m = 0; m < nparams; m++) {
          if (i == params[m].ielement && j == params[m].jelement &&
              k == params[m].kelement) {
            if (n >= 0) error->all(FLERR,"Potential file has duplicate entry");
            n = m;
          }
        }
        if (n < 0) error->all(FLERR,"Potential file is missing an entry");
        elem2param[i][j][k] = n;
      }


  // compute parameter values derived from inputs

  // set cutsq using shortcut to reduce neighbor list for accelerated
  // calculations. cut must remain unchanged as it is a potential parameter
  // (cut = a*sigma)

  for (m = 0; m < nparams; m++) {
    params[m].cut = params[m].sigma*params[m].littlea;

    rtmp = params[m].cut;
    if (params[m].tol > 0.0) {
      if (params[m].tol > 0.01) params[m].tol = 0.01;
      if (params[m].gamma < 1.0)
        rtmp = rtmp +
          params[m].gamma * params[m].sigma / log(params[m].tol);
      else rtmp = rtmp +
             params[m].sigma / log(params[m].tol);
    }
    params[m].cutsq = rtmp * rtmp;

    params[m].sigma_gamma = params[m].sigma*params[m].gamma;
    params[m].lambda_epsilon = params[m].lambda*params[m].epsilon;
    params[m].lambda_epsilon2 = 2.0*params[m].lambda*params[m].epsilon;
    params[m].c1 = params[m].biga*params[m].epsilon *
      params[m].powerp*params[m].bigb *
      pow(params[m].sigma,params[m].powerp);
    params[m].c2 = params[m].biga*params[m].epsilon*params[m].powerq *
      pow(params[m].sigma,params[m].powerq);
    params[m].c3 = params[m].biga*params[m].epsilon*params[m].bigb *
      pow(params[m].sigma,params[m].powerp+1.0);
    params[m].c4 = params[m].biga*params[m].epsilon *
      pow(params[m].sigma,params[m].powerq+1.0);
    params[m].c5 = params[m].biga*params[m].epsilon*params[m].bigb *
      pow(params[m].sigma,params[m].powerp);
    params[m].c6 = params[m].biga*params[m].epsilon *
      pow(params[m].sigma,params[m].powerq);
  }

  // set cutmax to max of all params

  cutmax = 0.0;
  for (m = 0; m < nparams; m++) {
    rtmp = sqrt(params[m].cutsq);
    if (rtmp > cutmax) cutmax = rtmp;
  }
#endif
}

