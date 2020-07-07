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
   Contributing author: Jacob Gissinger (jacob.gissinger@colorado.edu)
------------------------------------------------------------------------- */

#include "reset_mol_ids.h"
#include <mpi.h>
#include <string>
#include "atom.h"
#include "domain.h"
#include "comm.h"
#include "group.h"
#include "modify.h"
#include "compute_fragment_atom.h"
#include "compute_chunk_atom.h"
#include "compute_reduce.h"
#include "utils.h"
#include "error.h"
#include "fmt/format.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

ResetMolIDs::ResetMolIDs(LAMMPS *lmp) : Pointers(lmp) {}

/* ---------------------------------------------------------------------- */

void ResetMolIDs::command(int narg, char **arg)
{
  if (domain->box_exist == 0)
    error->all(FLERR,"Reset_mol_ids command before simulation box is defined");
  if (atom->tag_enable == 0)
    error->all(FLERR,"Cannot use reset_mol_ids unless atoms have IDs");
  if (atom->molecular != 1)
    error->all(FLERR,"Can only use reset_mol_ids on molecular systems");

  // process args

  if (narg < 1) error->all(FLERR,"Illegal reset_mol_ids command");
  int igroup = group->find(arg[0]);
  if (igroup == -1) error->all(FLERR,"Could not find reset_mol_ids group ID");
  int groupbit = group->bitmask[igroup];

  tagint offset = -1;
  int singleflag = 0;

  int iarg = 1;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"offset") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal reset_mol_ids command");
      if (strcmp(arg[iarg+1],"auto") == 0) {
        offset = -1;
      } else {
        offset = utils::tnumeric(FLERR,arg[iarg+1],1,lmp);
        if (offset < 0) error->all(FLERR,"Illegal reset_mol_ids command");
      }
      iarg += 2;
    } else if (strcmp(arg[iarg],"singlezero") == 0) {
      singleflag = 1;
      ++iarg;
    } else error->all(FLERR,"Illegal reset_mol_ids command");
  }

  if (comm->me == 0) utils::logmesg(lmp,"Resetting molecule IDs ...\n");

  // record wall time for resetting molecule IDs

  MPI_Barrier(world);
  double time1 = MPI_Wtime();

  // create instances of compute fragment/atom, compute reduce (if needed),
  // and compute chunk/atom.  all use the group-ID for this command

  const std::string idfrag = "reset_mol_ids_FRAGMENT_ATOM";
  if (singleflag)
    modify->add_compute(fmt::format("{} {} fragment/atom singlezero",idfrag,arg[0]));
  else
    modify->add_compute(fmt::format("{} {} fragment/atom",idfrag,arg[0]));

  const std::string idmin = "reset_mol_ids_COMPUTE_MINFRAG";
  if (singleflag)
    modify->add_compute(fmt::format("{} {} reduce min c_{}",idmin,arg[0],idfrag));

  const std::string idchunk = "reset_mol_ids_CHUNK_ATOM";
  modify->add_compute(fmt::format("{} {} chunk/atom molecule compress yes",
                                  idchunk,arg[0]));

  // initialize system since comm->borders() will be invoked

  lmp->init();

  // setup domain, communication
  // exchange will clear map, borders will reset
  // this is the map needed to lookup current global IDs for bond topology

  if (domain->triclinic) domain->x2lamda(atom->nlocal);
  domain->pbc();
  domain->reset_box();
  comm->setup();
  comm->exchange();
  comm->borders();
  if (domain->triclinic) domain->lamda2x(atom->nlocal+atom->nghost);

  // invoke peratom method of compute fragment/atom
  // walks bond connectivity and assigns each atom a fragment ID

  int icompute = modify->find_compute(idfrag);
  ComputeFragmentAtom *cfa = (ComputeFragmentAtom *) modify->compute[icompute];
  cfa->compute_peratom();
  double *ids = cfa->vector_atom;

  // when the lowest fragment ID is 0, in case the singlezero option is used
  // we need to remove adjust the chunk ID, so that the resuling molecule ID is correct.

  int adjid = 0;
  if (singleflag) {
    icompute = modify->find_compute(idmin);
    ComputeReduce *crd = (ComputeReduce *) modify->compute[icompute];
    if (crd->compute_scalar() == 0.0) adjid = 1;
  }

  // copy fragmentID to molecule ID
  // only for atoms in the group

  tagint *molecule = atom->molecule;
  int *mask = atom->mask;
  int nlocal = atom->nlocal;

  for (int i = 0; i < nlocal; i++)
    if (mask[i] & groupbit)
      molecule[i] = static_cast<tagint> (ids[i]);

  // invoke peratom method of compute chunk/atom
  // compress new molecule IDs to be contiguous 1 to Nmol
  // NOTE: use of compute chunk/atom limits # of molecules to a 32-bit int

  icompute = modify->find_compute(idchunk);
  ComputeChunkAtom *cca = (ComputeChunkAtom *) modify->compute[icompute];
  cca->compute_peratom();
  ids = cca->vector_atom;
  int nchunk = cca->nchunk;

  // if offset = -1 (i.e. "auto") and group != all,
  // reset offset to largest molID of non-group atoms
  // otherwise set to 0.

  if (offset == -1) {
    if (groupbit != 1) {
    tagint mymol = 0;
    for (int i = 0; i < nlocal; i++)
      if (!(mask[i] & groupbit))
        mymol = MAX(mymol,molecule[i]);
    MPI_Allreduce(&mymol,&offset,1,MPI_LMP_TAGINT,MPI_MAX,world);
    } else offset = 0;
  }

  // copy chunkID to molecule ID + offset
  // only for atoms in the group

  for (int i = 0; i < nlocal; i++) {
    if (mask[i] & groupbit) {
      tagint newid =  static_cast<tagint>(ids[i]);
      if (singleflag && adjid) {
        if (newid == 1) newid = 0;
        else newid += offset - 1;
        molecule[i] = newid;
      } else molecule[i] = newid + offset;
    }
  }

  // clean up

  modify->delete_compute(idchunk);
  modify->delete_compute(idfrag);
  if (singleflag) modify->delete_compute(idmin);

  // total time

  MPI_Barrier(world);

  if (comm->me == 0) {
    utils::logmesg(lmp,fmt::format("  number of new molecule IDs = {}\n",nchunk));
    utils::logmesg(lmp,fmt::format("  reset_mol_ids CPU = {:.3f} seconds\n",
                                   MPI_Wtime()-time1));
  }
}
