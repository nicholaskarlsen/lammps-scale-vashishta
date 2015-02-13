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

#include "string.h"
#include "compute_msd_chunk.h"
#include "atom.h"
#include "update.h"
#include "modify.h"
#include "compute_chunk_atom.h"
#include "domain.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ComputeMSDChunk::ComputeMSDChunk(LAMMPS *lmp, int narg, char **arg) :
  Compute(lmp, narg, arg)
{
  if (narg != 4) error->all(FLERR,"Illegal compute msd/chunk command");

  array_flag = 1;
  size_array_cols = 4;
  size_array_rows = 0;
  size_array_rows_variable = 1;
  extarray = 0;

  // ID of compute chunk/atom

  int n = strlen(arg[6]) + 1;
  idchunk = new char[n];
  strcpy(idchunk,arg[6]);

  init();

  massproc = masstotal = NULL;
  com = comall = cominit = NULL;
  msd = NULL;

  firstflag = 1;
}

/* ---------------------------------------------------------------------- */

ComputeMSDChunk::~ComputeMSDChunk()
{
  delete [] idchunk;
  memory->destroy(massproc);
  memory->destroy(masstotal);
  memory->destroy(com);
  memory->destroy(comall);
  memory->destroy(cominit);
  memory->destroy(msd);
}

/* ---------------------------------------------------------------------- */

void ComputeMSDChunk::init()
{
  int icompute = modify->find_compute(idchunk);
  if (icompute < 0)
    error->all(FLERR,"Chunk/atom compute does not exist for compute msd/chunk");
  cchunk = (ComputeChunkAtom *) modify->compute[icompute];
  if (strcmp(cchunk->style,"chunk/atom") != 0)
    error->all(FLERR,"Compute msd/chunk does not use chunk/atom compute");
}

/* ----------------------------------------------------------------------
   compute initial COM for each chunk
   only once on timestep compute is defined, when firstflag = 1
------------------------------------------------------------------------- */

void ComputeMSDChunk::setup()
{
  if (!firstflag) return;
  firstflag = 0;

  compute_array();
  for (int i = 0; i < nchunk; i++) {
    cominit[i][0] = comall[i][0];
    cominit[i][1] = comall[i][1];
    cominit[i][2] = comall[i][2];
  }
}

/* ---------------------------------------------------------------------- */

void ComputeMSDChunk::compute_array()
{
  int index;
  double massone;
  double unwrap[3];

  invoked_array = update->ntimestep;

  // compute chunk/atom assigns atoms to chunk IDs
  // extract ichunk index vector from compute
  // ichunk = 1 to Nchunk for included atoms, 0 for excluded atoms

  int n = cchunk->setup_chunks();
  cchunk->compute_ichunk();
  int *ichunk = cchunk->ichunk;

  // first time call, allocate per-chunk arrays
  // thereafter, require nchunk remain the same

  if (firstflag) {
    nchunk = n;
    allocate();
  } else if (n != nchunk) 
    error->all(FLERR,"Compute msd/chunk nchunk is not static");

  // zero local per-chunk values

  for (int i = 0; i < nchunk; i++) {
    massproc[i] = 0.0;
    com[i][0] = com[i][1] = com[i][2] = 0.0;
  }

  // compute current COM for each chunk

  double **x = atom->x;
  int *mask = atom->mask;
  int *type = atom->type;
  imageint *image = atom->image;
  double *mass = atom->mass;
  double *rmass = atom->rmass;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit) {
      index = ichunk[i]-1;
      if (index < 0) continue;
      if (rmass) massone = rmass[i];
      else massone = mass[type[i]];
      domain->unmap(x[i],image[i],unwrap);
      massproc[index] += massone;
      com[index][0] += unwrap[0] * massone;
      com[index][1] += unwrap[1] * massone;
      com[index][2] += unwrap[2] * massone;
    }

  MPI_Allreduce(massproc,masstotal,nchunk,MPI_DOUBLE,MPI_SUM,world);
  MPI_Allreduce(&com[0][0],&comall[0][0],3*nchunk,MPI_DOUBLE,MPI_SUM,world);

  for (int i = 0; i < nchunk; i++) {
    comall[i][0] /= masstotal[i];
    comall[i][1] /= masstotal[i];
    comall[i][2] /= masstotal[i];
  }

  // MSD is difference between current and initial COM
  // cominit does not yet exist when called from constructor

  if (firstflag) return;

  double dx,dy,dz;

  for (int i = 0; i < nchunk; i++) {
    dx = comall[i][0] - cominit[i][0];
    dy = comall[i][1] - cominit[i][1];
    dz = comall[i][2] - cominit[i][2];
    msd[i][0] = dx*dx;
    msd[i][1] = dy*dy;
    msd[i][2] = dz*dz;
    msd[i][3] = dx*dx + dy*dy + dz*dz;
  }
}

/* ----------------------------------------------------------------------
   lock methods: called by fix ave/time
   these methods insure vector/array size is locked for Nfreq epoch
     by passing lock info along to compute chunk/atom
------------------------------------------------------------------------- */

/* ----------------------------------------------------------------------
   increment lock counter
------------------------------------------------------------------------- */

void ComputeMSDChunk::lock_enable()
{
  cchunk->lockcount++;
}

/* ----------------------------------------------------------------------
   decrement lock counter in compute chunk/atom, it if still exists
------------------------------------------------------------------------- */

void ComputeMSDChunk::lock_disable()
{
  int icompute = modify->find_compute(idchunk);
  if (icompute >= 0) {
    cchunk = (ComputeChunkAtom *) modify->compute[icompute];
    cchunk->lockcount--;
  }
}

/* ----------------------------------------------------------------------
   calculate and return # of chunks = length of vector/array
------------------------------------------------------------------------- */

int ComputeMSDChunk::lock_length()
{
  nchunk = cchunk->setup_chunks();
  return nchunk;
}

/* ----------------------------------------------------------------------
   set the lock from startstep to stopstep
------------------------------------------------------------------------- */

void ComputeMSDChunk::lock(Fix *fixptr, bigint startstep, bigint stopstep)
{
  cchunk->lock(fixptr,startstep,stopstep);
}

/* ----------------------------------------------------------------------
   unset the lock
------------------------------------------------------------------------- */

void ComputeMSDChunk::unlock(Fix *fixptr)
{
  cchunk->unlock(fixptr);
}

/* ----------------------------------------------------------------------
   one-time allocate of per-chunk arrays
------------------------------------------------------------------------- */

void ComputeMSDChunk::allocate()
{
  memory->create(massproc,nchunk,"msd/chunk:massproc");
  memory->create(masstotal,nchunk,"msd/chunk:masstotal");
  memory->create(com,nchunk,3,"msd/chunk:com");
  memory->create(comall,nchunk,3,"msd/chunk:comall");
  memory->create(cominit,nchunk,3,"msd/chunk:cominit");
  memory->create(msd,nchunk,4,"msd/chunk:msd");
  array = msd;
}

/* ----------------------------------------------------------------------
   memory usage of local data
------------------------------------------------------------------------- */

double ComputeMSDChunk::memory_usage()
{
  double bytes = (bigint) nchunk * 2 * sizeof(double);
  bytes += (bigint) nchunk * 3*3 * sizeof(double);
  bytes += (bigint) nchunk * 4 * sizeof(double);
  return bytes;
}
