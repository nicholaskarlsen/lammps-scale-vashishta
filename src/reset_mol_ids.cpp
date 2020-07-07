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

  if (narg != 1) error->all(FLERR,"Illegal reset_mol_ids command");
  int igroup = group->find(arg[0]);
  if (igroup == -1) error->all(FLERR,"Could not find reset_mol_ids group ID");

  if (comm->me == 0)
    utils::logmesg(lmp,"Resetting molecule IDs ...\n");

  // record wall time for resetting molecule IDs

  MPI_Barrier(world);
  double time1 = MPI_Wtime();

  // create instances of compute fragment/atom and compute chunk/atom

  const std::string idfrag = "reset_mol_ids_FRAGMENT_ATOM";
  modify->add_compute(fmt::format("{} {} fragment/atom",idfrag, arg[0]));

  const std::string idchunk = "reset_mol_ids_CHUNK_ATOM";
  modify->add_compute(fmt::format("{} all chunk/atom molecule compress yes",
                                  idchunk));

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

  // copy fragmentID to molecule ID

  tagint *molecule = atom->molecule;
  int nlocal = atom->nlocal;
  
  for (int i = 0; i < nlocal; i++)
    molecule[i] = static_cast<tagint> (ids[i]);

  // invoke peratom method of compute chunk/atom
  // compresses new molecule IDs to be contiguous 1 to Nmol

  icompute = modify->find_compute(idchunk);
  ComputeChunkAtom *cca = (ComputeChunkAtom *) modify->compute[icompute];
  cca->compute_peratom();
  ids = cca->vector_atom;
  int nchunk = cca->nchunk;
  
  // copy chunkID to molecule ID

  for (int i = 0; i < nlocal; i++)
    molecule[i] = static_cast<tagint> (ids[i]);

  // clean up

  modify->delete_compute(idchunk);
  modify->delete_compute(idfrag);

  // total time

  MPI_Barrier(world);
  
  if (comm->me == 0)
    utils::logmesg(lmp,fmt::format("  reset_mol_ids CPU = {:.3f} seconds\n",
                                   MPI_Wtime()-time1));
}
