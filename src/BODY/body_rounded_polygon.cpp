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
   Contributing author: Trung Dac Nguyen (ndactrung@gmail.com)
------------------------------------------------------------------------- */

#include "body_rounded_polygon.h"
#include <cmath>
#include <cstring>
#include "my_pool_chunk.h"
#include "atom_vec_body.h"
#include "atom.h"
#include "force.h"
#include "domain.h"
#include "math_extra.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

#define EPSILON 1.0e-7
enum{SPHERE,LINE};           // also in DumpImage

/* ---------------------------------------------------------------------- */

BodyRoundedPolygon::BodyRoundedPolygon(LAMMPS *lmp, int narg, char **arg) :
  Body(lmp, narg, arg)
{
  if (narg != 3) error->all(FLERR,"Invalid body rounded/polygon command");

  if (domain->dimension != 2)
    error->all(FLERR,"Atom_style body rounded/polygon "
               "can only be used in 2d simulations");

  // nmin and nmax are minimum and maximum number of vertices

  int nmin = force->inumeric(FLERR,arg[1]);
  int nmax = force->inumeric(FLERR,arg[2]);
  if (nmin <= 0 || nmin > nmax)
    error->all(FLERR,"Invalid body rounded/polygon command");

  size_forward = 0;

  // 1 integer for number of vertices,
  // 3*nmax doubles for vertex coordinates + 2*nmax doubles for edge ends
  // 1 double for the enclosing radius
  // 1 double for the rounded radius

  size_border = 1 + 3*nmax + 2*nmax + 1 + 1;

  // NOTE: need to set appropriate nnbin param for dcp

  icp = new MyPoolChunk<int>(1,1);
  dcp = new MyPoolChunk<double>(3*nmin+2*nmin+1+1,3*nmax+2*nmax+1+1);
  maxexchange = 1 + 3*nmax+2*nmax+1+1;      // icp max + dcp max

  memory->create(imflag,nmax,"body/rounded/polygon:imflag");
  memory->create(imdata,nmax,7,"body/nparticle:imdata");
}

/* ---------------------------------------------------------------------- */

BodyRoundedPolygon::~BodyRoundedPolygon()
{
  delete icp;
  delete dcp;
  memory->destroy(imflag);
  memory->destroy(imdata);
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::nsub(AtomVecBody::Bonus *bonus)
{
  return bonus->ivalue[0];
}

/* ---------------------------------------------------------------------- */

double *BodyRoundedPolygon::coords(AtomVecBody::Bonus *bonus)
{
  return bonus->dvalue;
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::nedges(AtomVecBody::Bonus *bonus)
{
  int nvertices = bonus->ivalue[0];
  if (nvertices == 1) return 0;
  else if (nvertices == 2) return 1;
  return nvertices;
}

/* ---------------------------------------------------------------------- */

double *BodyRoundedPolygon::edges(AtomVecBody::Bonus *bonus)
{
  return bonus->dvalue+3*nsub(bonus);
}

/* ---------------------------------------------------------------------- */

double BodyRoundedPolygon::enclosing_radius(struct AtomVecBody::Bonus *bonus)
{
  int nvertices = bonus->ivalue[0];
  if (nvertices == 1 || nvertices == 2)
	return *(bonus->dvalue+3*nsub(bonus)+2);
  return *(bonus->dvalue + 3*nsub(bonus) + 2*nsub(bonus));
}

/* ---------------------------------------------------------------------- */

double BodyRoundedPolygon::rounded_radius(struct AtomVecBody::Bonus *bonus)
{
  int nvertices = bonus->ivalue[0];
  if (nvertices == 1 || nvertices == 2)
	return *(bonus->dvalue+3*nsub(bonus)+2+1);
  return *(bonus->dvalue + 3*nsub(bonus) + 2*nsub(bonus)+1);
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::pack_border_body(AtomVecBody::Bonus *bonus, double *buf)
{
  int nsub = bonus->ivalue[0];
  buf[0] = nsub;
  memcpy(&buf[1],bonus->dvalue,(3*nsub+2*nsub+1+1)*sizeof(double));
  return 1+(3*nsub+2*nsub+1+1);
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::unpack_border_body(AtomVecBody::Bonus *bonus,
                                           double *buf)
{
  int nsub = static_cast<int> (buf[0]);
  bonus->ivalue[0] = nsub;
  memcpy(bonus->dvalue,&buf[1],(3*nsub+2*nsub+1+1)*sizeof(double));
  return 1+(3*nsub+2*nsub+1+1);
}

/* ----------------------------------------------------------------------
   populate bonus data structure with data file values
------------------------------------------------------------------------- */

void BodyRoundedPolygon::data_body(int ibonus, int ninteger, int ndouble,
				   int *ifile, double *dfile)
{
  AtomVecBody::Bonus *bonus = &avec->bonus[ibonus];

  // set ninteger, ndouble in bonus and allocate 2 vectors of ints, doubles

  if (ninteger != 1)
    error->one(FLERR,"Incorrect # of integer values in "
               "Bodies section of data file");
  int nsub = ifile[0];
  if (nsub < 1)
    error->one(FLERR,"Incorrect integer value in "
               "Bodies section of data file");

  // nentries = number of double entries to be read from Body section:
  //   6 for inertia + 3*nsub for vertex coords + 1 for rounded radius

  int nentries = 6 + 3*nsub + 1;
  if (ndouble != nentries)
    error->one(FLERR,"Incorrect # of floating-point values in "
             "Bodies section of data file");

  bonus->ninteger = 1;
  bonus->ivalue = icp->get(bonus->iindex);
  bonus->ivalue[0] = nsub;
  bonus->ndouble = 3*nsub + 2*nsub + 1 + 1;
  bonus->dvalue = dcp->get(bonus->ndouble,bonus->dindex);

  // diagonalize inertia tensor

  double tensor[3][3];
  tensor[0][0] = dfile[0];
  tensor[1][1] = dfile[1];
  tensor[2][2] = dfile[2];
  tensor[0][1] = tensor[1][0] = dfile[3];
  tensor[0][2] = tensor[2][0] = dfile[4];
  tensor[1][2] = tensor[2][1] = dfile[5];

  double *inertia = bonus->inertia;
  double evectors[3][3];
  int ierror = MathExtra::jacobi(tensor,inertia,evectors);
  if (ierror) error->one(FLERR,
                         "Insufficient Jacobi rotations for body nparticle");

  // if any principal moment < scaled EPSILON, set to 0.0

  double max;
  max = MAX(inertia[0],inertia[1]);
  max = MAX(max,inertia[2]);

  if (inertia[0] < EPSILON*max) inertia[0] = 0.0;
  if (inertia[1] < EPSILON*max) inertia[1] = 0.0;
  if (inertia[2] < EPSILON*max) inertia[2] = 0.0;

  // exyz_space = principal axes in space frame

  double ex_space[3],ey_space[3],ez_space[3];

  ex_space[0] = evectors[0][0];
  ex_space[1] = evectors[1][0];
  ex_space[2] = evectors[2][0];
  ey_space[0] = evectors[0][1];
  ey_space[1] = evectors[1][1];
  ey_space[2] = evectors[2][1];
  ez_space[0] = evectors[0][2];
  ez_space[1] = evectors[1][2];
  ez_space[2] = evectors[2][2];

  // enforce 3 evectors as a right-handed coordinate system
  // flip 3rd vector if needed

  double cross[3];
  MathExtra::cross3(ex_space,ey_space,cross);
  if (MathExtra::dot3(cross,ez_space) < 0.0) MathExtra::negate3(ez_space);

  // create initial quaternion

  MathExtra::exyz_to_q(ex_space,ey_space,ez_space,bonus->quat);

  // bonus->dvalue = the first 3*nsub elements are sub-particle displacements
  // find the enclosing radius of the body from the maximum displacement

  int i,m;
  double delta[3], rsq, erad, rrad;
  double erad2 = 0;
  int j = 6;
  int k = 0;
  for (i = 0; i < nsub; i++) {
    delta[0] = dfile[j];
    delta[1] = dfile[j+1];
    delta[2] = dfile[j+2];
    MathExtra::transpose_matvec(ex_space,ey_space,ez_space,
                                delta,&bonus->dvalue[k]);
    rsq = delta[0] * delta[0] + delta[1] * delta[1] +
      delta[2] * delta[2];
    if (rsq > erad2) erad2 = rsq;
    j += 3;
    k += 3;
  }

  // .. the next 2*nsub elements are edge ends

  int nedges;
  if (nsub == 1) { // spheres
    nedges = 0;
    bonus->dvalue[k] = 0;
    *(&bonus->dvalue[k]+1) = 0;
    k += 2;

    // the last element of bonus->dvalue is the rounded & enclosing radius

    rrad = 0.5 * dfile[j];
    bonus->dvalue[k] = rrad;
    erad = rrad;

    k++;
    bonus->dvalue[k] = rrad;

    atom->radius[bonus->ilocal] = erad;

  } else if (nsub == 2) { // rods
    nedges = 1;
    for (i = 0; i < nedges; i++) {
      bonus->dvalue[k] = 0;
      *(&bonus->dvalue[k]+1) = 1;
      k += 2;
    }

    erad = sqrt(erad2);
    bonus->dvalue[k] = erad;

    // the last element of bonus->dvalue is the rounded radius

    rrad = 0.5 * dfile[j];
    k++;
    bonus->dvalue[k] = rrad;

    atom->radius[bonus->ilocal] = erad + rrad;

  } else { // polygons
    nedges = nsub;
    for (i = 0; i < nedges; i++) {
      bonus->dvalue[k] = i;
      m = i+1;
      if (m == nedges) m = 0;
      *(&bonus->dvalue[k]+1) = m;
      k += 2;
    }

    // the next to last element is the enclosing radius

    erad = sqrt(erad2);
    bonus->dvalue[k] = erad;

    // the last element of bonus->dvalue is the rounded radius

    rrad = 0.5 * dfile[j];
    k++;
    bonus->dvalue[k] = rrad;

    atom->radius[bonus->ilocal] = erad + rrad;
  }
}

/* ----------------------------------------------------------------------
   return radius of body particle defined by ifile/dfile params
   params are ordered as in data file
   called by Molecule class which needs single body size
------------------------------------------------------------------------- */

double BodyRoundedPolygon::radius_body(int /*ninteger*/, int ndouble,
				       int *ifile, double *dfile)
{
  int nsub = ifile[0];
  if (nsub < 1)
    error->one(FLERR,"Incorrect integer value in "
               "Bodies section of data file");
  if (ndouble != 6 + 3*nsub + 1)
    error->one(FLERR,"Incorrect # of floating-point values in "
               "Bodies section of data file");

  // sub-particle coords are relative to body center at (0,0,0)
  // offset = 6 for sub-particle coords

  double onerad;
  double maxrad = 0.0;
  double delta[3];

  int offset = 6;
  for (int i = 0; i < nsub; i++) {
    delta[0] = dfile[offset];
    delta[1] = dfile[offset+1];
    delta[2] = dfile[offset+2];
    offset += 3;
    onerad = MathExtra::len3(delta);
    maxrad = MAX(maxrad,onerad);
  }

  // add in radius of rounded corners

  return maxrad + 0.5*dfile[offset];
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::noutcol()
{
  // the number of columns for the vertex coordinates

  return 3;
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::noutrow(int ibonus)
{
  // only return the first nsub rows for the vertex coordinates

  return avec->bonus[ibonus].ivalue[0];
}

/* ---------------------------------------------------------------------- */

void BodyRoundedPolygon::output(int ibonus, int m, double *values)
{
  AtomVecBody::Bonus *bonus = &avec->bonus[ibonus];

  double p[3][3];
  MathExtra::quat_to_mat(bonus->quat,p);
  MathExtra::matvec(p,&bonus->dvalue[3*m],values);

  double *x = atom->x[bonus->ilocal];
  values[0] += x[0];
  values[1] += x[1];
  values[2] += x[2];
}

/* ---------------------------------------------------------------------- */

int BodyRoundedPolygon::image(int ibonus, double flag1, double /*flag2*/,
                              int *&ivec, double **&darray)
{
  int j;
  double p[3][3];
  double *x, rrad;

  AtomVecBody::Bonus *bonus = &avec->bonus[ibonus];
  int n = bonus->ivalue[0];

  if (n == 1) {
    for (int i = 0; i < n; i++) {
      imflag[i] = SPHERE;
      MathExtra::quat_to_mat(bonus->quat,p);
      MathExtra::matvec(p,&bonus->dvalue[3*i],imdata[i]);

      rrad = enclosing_radius(bonus);
      x = atom->x[bonus->ilocal];
      imdata[i][0] += x[0];
      imdata[i][1] += x[1];
      imdata[i][2] += x[2];
      if (flag1 <= 0) imdata[i][3] = 2*rrad;
      else imdata[i][3] = flag1;
    }

  } else {

    // first end pt of each line

    for (int i = 0; i < n; i++) {
      imflag[i] = LINE;
      MathExtra::quat_to_mat(bonus->quat,p);
      MathExtra::matvec(p,&bonus->dvalue[3*i],imdata[i]);

      rrad = rounded_radius(bonus);
      x = atom->x[bonus->ilocal];
      imdata[i][0] += x[0];
      imdata[i][1] += x[1];
      imdata[i][2] += x[2];
      if (flag1 <= 0) imdata[i][6] = 2*rrad;
      else imdata[i][6] = flag1;
    }

    // second end pt of each line

    for (int i = 0; i < n; i++) {
      j = i+1;
      if (j == n) j = 0;
      imdata[i][3] = imdata[j][0];
      imdata[i][4] = imdata[j][1];
      imdata[i][5] = imdata[j][2];
    }
  }

  ivec = imflag;
  darray = imdata;
  return n;
}
