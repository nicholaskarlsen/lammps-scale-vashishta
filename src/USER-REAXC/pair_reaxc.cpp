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
   Contributing author: Hasan Metin Aktulga, Purdue University
   (now at Lawrence Berkeley National Laboratory, hmaktulga@lbl.gov)
   Per-atom energy/virial added by Ray Shan (Sandia)
   Fix reax/c/bonds and fix reax/c/species for pair_style reax/c added by
        Ray Shan (Sandia)
   Hybrid and hybrid/overlay compatibility added by Ray Shan (Sandia)
------------------------------------------------------------------------- */

#include "pair_reaxc.h"

#include "atom.h"
#include "citeme.h"
#include "comm.h"
#include "error.h"
#include "fix.h"
#include "fix_reaxc.h"
#include "force.h"
#include "memory.h"
#include "modify.h"
#include "neigh_list.h"
#include "neigh_request.h"
#include "neighbor.h"
#include "update.h"

#include <cmath>
#include <cstring>
#include <strings.h>    // for strcasecmp()

#include "reaxff_api.h"

using namespace LAMMPS_NS;
using namespace ReaxFF;

static const char cite_pair_reax_c[] =
  "pair reax/c command:\n\n"
  "@Article{Aktulga12,\n"
  " author = {H. M. Aktulga, J. C. Fogarty, S. A. Pandit, A. Y. Grama},\n"
  " title = {Parallel reactive molecular dynamics: Numerical methods and algorithmic techniques},\n"
  " journal = {Parallel Computing},\n"
  " year =    2012,\n"
  " volume =  38,\n"
  " pages =   {245--259}\n"
  "}\n\n";

/* ---------------------------------------------------------------------- */

PairReaxC::PairReaxC(LAMMPS *lmp) : Pair(lmp)
{
  if (lmp->citeme) lmp->citeme->add(cite_pair_reax_c);

  single_enable = 0;
  restartinfo = 0;
  one_coeff = 1;
  manybody_flag = 1;
  centroidstressflag = CENTROID_NOTAVAIL;
  ghostneigh = 1;

  fix_id = utils::strdup("REAXC_" + std::to_string(instance_me));

  api = new API;

  api->system = new reax_system;
  memset(api->system,0,sizeof(reax_system));
  api->control = new control_params;
  memset(api->control,0,sizeof(control_params));
  api->out_control = new output_controls;
  memset(api->out_control,0,sizeof(output_controls));
  api->data = new simulation_data;
  api->workspace = new storage;
  memory->create(api->lists, LIST_N,"reaxff:lists");
  memset(api->lists,0,LIST_N * sizeof(reax_list));

  api->control->me = api->system->my_rank = comm->me;

  api->system->num_nbrs = 0;
  api->system->n = 0;                // my atoms
  api->system->N = 0;                // mine + ghosts
  api->system->bigN = 0;             // all atoms in the system
  api->system->local_cap = 0;
  api->system->total_cap = 0;
  api->system->my_atoms = nullptr;
  api->system->pair_ptr = this;
  api->system->error_ptr = error;
  api->control->error_ptr = error;
  api->control->lmp_ptr = lmp;

  api->system->omp_active = 0;

  fix_reax = nullptr;
  tmpid = nullptr;
  tmpbo = nullptr;

  nextra = 14;
  pvector = new double[nextra];

  setup_flag = 0;
  fixspecies_flag = 0;

  nmax = 0;
}

/* ---------------------------------------------------------------------- */

PairReaxC::~PairReaxC()
{
  if (copymode) return;

  if (fix_reax) modify->delete_fix(fix_id);
  delete[] fix_id;

  if (setup_flag) {
    Close_Output_Files(api->system,api->out_control);

    // deallocate reax data-structures

    if (api->control->tabulate) Deallocate_Lookup_Tables(api->system);

    if (api->control->hbond_cut > 0)  Delete_List(api->lists+HBONDS);
    Delete_List(api->lists+BONDS);
    Delete_List(api->lists+THREE_BODIES);
    Delete_List(api->lists+FAR_NBRS);

    DeAllocate_Workspace(api->control, api->workspace);
    DeAllocate_System(api->system);
  }

  delete api->system;
  delete api->control;
  delete api->out_control;
  delete api->data;
  delete api->workspace;
  memory->destroy(api->lists);

  // deallocate interface storage
  if (allocated) {
    memory->destroy(setflag);
    memory->destroy(cutsq);
    memory->destroy(cutghost);

    delete [] chi;
    delete [] eta;
    delete [] gamma;
  }

  memory->destroy(tmpid);
  memory->destroy(tmpbo);

  delete [] pvector;

}

/* ---------------------------------------------------------------------- */

void PairReaxC::allocate()
{
  allocated = 1;
  int n = atom->ntypes;

  memory->create(setflag,n+1,n+1,"pair:setflag");
  memory->create(cutsq,n+1,n+1,"pair:cutsq");
  memory->create(cutghost,n+1,n+1,"pair:cutghost");
  map = new int[n+1];

  chi = new double[n+1];
  eta = new double[n+1];
  gamma = new double[n+1];
}

/* ---------------------------------------------------------------------- */

void PairReaxC::settings(int narg, char **arg)
{
  if (narg < 1) error->all(FLERR,"Illegal pair_style command");

  if (comm->me == 0) {
    // read name of control file or use default controls

    if (strcmp(arg[0],"NULL") == 0) {
      strcpy(api->control->sim_name, "simulate");
      api->out_control->energy_update_freq = 0;
      api->control->tabulate = 0;

      api->control->bond_cut = 5.;
      api->control->hbond_cut = 7.50;
      api->control->thb_cut = 0.001;
      api->control->thb_cutsq = 0.00001;
      api->control->bg_cut = 0.3;

      api->control->nthreads = 1;

      api->out_control->write_steps = 0;
      strcpy(api->out_control->traj_title, "default_title");
      api->out_control->atom_info = 0;
      api->out_control->bond_info = 0;
      api->out_control->angle_info = 0;
    } else Read_Control_File(arg[0], api->control, api->out_control);
  }
  MPI_Bcast(api->control,sizeof(control_params),MPI_CHAR,0,world);
  MPI_Bcast(api->out_control,sizeof(output_controls),MPI_CHAR,0,world);

  // must reset these to local values after broadcast
  api->control->me = comm->me;
  api->control->error_ptr = error;
  api->control->lmp_ptr = lmp;

  // default values

  qeqflag = 1;
  api->control->lgflag = 0;
  api->control->enobondsflag = 1;
  api->system->mincap = REAX_MIN_CAP;
  api->system->minhbonds = REAX_MIN_HBONDS;
  api->system->safezone = REAX_SAFE_ZONE;
  api->system->saferzone = REAX_SAFER_ZONE;

  // process optional keywords

  int iarg = 1;

  while (iarg < narg) {
    if (strcmp(arg[iarg],"checkqeq") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      if (strcmp(arg[iarg+1],"yes") == 0) qeqflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) qeqflag = 0;
      else error->all(FLERR,"Illegal pair_style reax/c command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"enobonds") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      if (strcmp(arg[iarg+1],"yes") == 0) api->control->enobondsflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) api->control->enobondsflag = 0;
      else error->all(FLERR,"Illegal pair_style reax/c command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"lgvdw") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      if (strcmp(arg[iarg+1],"yes") == 0) api->control->lgflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) api->control->lgflag = 0;
      else error->all(FLERR,"Illegal pair_style reax/c command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"safezone") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      api->system->safezone = utils::numeric(FLERR,arg[iarg+1],false,lmp);
      if (api->system->safezone < 0.0)
        error->all(FLERR,"Illegal pair_style reax/c safezone command");
      api->system->saferzone = api->system->safezone*1.2 + 0.2;
      iarg += 2;
    } else if (strcmp(arg[iarg],"mincap") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      api->system->mincap = utils::inumeric(FLERR,arg[iarg+1],false,lmp);
      if (api->system->mincap < 0)
        error->all(FLERR,"Illegal pair_style reax/c mincap command");
      iarg += 2;
    } else if (strcmp(arg[iarg],"minhbonds") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal pair_style reax/c command");
      api->system->minhbonds = utils::inumeric(FLERR,arg[iarg+1],false,lmp);
      if (api->system->minhbonds < 0)
        error->all(FLERR,"Illegal pair_style reax/c minhbonds command");
      iarg += 2;
    } else error->all(FLERR,"Illegal pair_style reax/c command");
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxC::coeff(int nargs, char **args)
{
  if (!allocated) allocate();

  if (nargs != 3 + atom->ntypes)
    error->all(FLERR,"Incorrect args for pair coefficients");

  // insure I,J args are * *

  if (strcmp(args[0],"*") != 0 || strcmp(args[1],"*") != 0)
    error->all(FLERR,"Incorrect args for pair coefficients");

  // read ffield file

  Read_Force_Field(args[2], &(api->system->reax_param), api->control);

  // read args that map atom types to elements in potential file
  // map[i] = which element the Ith atom type is, -1 if "NULL"

  int itmp = 0;
  int nreax_types = api->system->reax_param.num_atom_types;
  for (int i = 3; i < nargs; i++) {
    if (strcmp(args[i],"NULL") == 0) {
      map[i-2] = -1;
      itmp ++;
      continue;
    }
  }

  int n = atom->ntypes;

  // pair_coeff element map
  for (int i = 3; i < nargs; i++)
    for (int j = 0; j < nreax_types; j++)
      if (strcasecmp(args[i],api->system->reax_param.sbp[j].name) == 0) {
        map[i-2] = j;
        itmp ++;
      }

  // error check
  if (itmp != n)
    error->all(FLERR,"Non-existent ReaxFF type");

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

/* ---------------------------------------------------------------------- */

void PairReaxC::init_style()
{
  if (!atom->q_flag)
    error->all(FLERR,"Pair style reax/c requires atom attribute q");

  // firstwarn = 1;

  bool have_qeq = ((modify->find_fix_by_style("^qeq/reax") != -1)
                   || (modify->find_fix_by_style("^qeq/shielded") != -1));
  if (!have_qeq && qeqflag == 1)
    error->all(FLERR,"Pair reax/c requires use of fix qeq/reax or qeq/shielded");

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system
  api->system->wsize = comm->nprocs;

  if (atom->tag_enable == 0)
    error->all(FLERR,"Pair style reax/c requires atom IDs");
  if (force->newton_pair == 0)
    error->all(FLERR,"Pair style reax/c requires newton pair on");
  if ((atom->map_tag_max > 99999999) && (comm->me == 0))
    error->warning(FLERR,"Some Atom-IDs are too large. Pair style reax/c "
                   "native output files may get misformatted or corrupted");

  // because system->bigN is an int, we cannot have more atoms than MAXSMALLINT

  if (atom->natoms > MAXSMALLINT)
    error->all(FLERR,"Too many atoms for pair style reax/c");

  // need a half neighbor list w/ Newton off and ghost neighbors
  // built whenever re-neighboring occurs

  int irequest = neighbor->request(this,instance_me);
  neighbor->requests[irequest]->newton = 2;
  neighbor->requests[irequest]->ghost = 1;

  cutmax = MAX3(api->control->nonb_cut, api->control->hbond_cut, api->control->bond_cut);
  if ((cutmax < 2.0*api->control->bond_cut) && (comm->me == 0))
    error->warning(FLERR,"Total cutoff < 2*bond cutoff. May need to use an "
                   "increased neighbor list skin.");

  for (int i = 0; i < LIST_N; ++i)
    if (api->lists[i].allocated != 1)
      api->lists[i].allocated = 0;

  if (fix_reax == nullptr) {
    modify->add_fix(fmt::format("{} all REAXC",fix_id));
    fix_reax = (FixReaxC *) modify->fix[modify->nfix-1];
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxC::setup()
{
  int oldN;
  int mincap = api->system->mincap;
  double safezone = api->system->safezone;

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  oldN = api->system->N;
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system

  if (setup_flag == 0) {

    setup_flag = 1;

    int *num_bonds = fix_reax->num_bonds;
    int *num_hbonds = fix_reax->num_hbonds;

    // determine the local and total capacity

    api->system->local_cap = MAX((int)(api->system->n * safezone), mincap);
    api->system->total_cap = MAX((int)(api->system->N * safezone), mincap);

    // initialize my data structures

    PreAllocate_Space(api->system, api->control, api->workspace);
    write_reax_atoms();

    int num_nbrs = estimate_reax_lists();
    if (num_nbrs < 0)
      error->all(FLERR,"Too many neighbors for pair style reax/c");
    if (!Make_List(api->system->total_cap, num_nbrs, TYP_FAR_NEIGHBOR,
                  api->lists+FAR_NBRS))
      error->all(FLERR,"Pair reax/c problem in far neighbor list");
    (api->lists+FAR_NBRS)->error_ptr=error;

    write_reax_lists();
    api->system->wsize = comm->nprocs;
    Initialize(api->system, api->control, api->data, api->workspace, &api->lists, api->out_control, world);
    for (int k = 0; k < api->system->N; ++k) {
      num_bonds[k] = api->system->my_atoms[k].num_bonds;
      num_hbonds[k] = api->system->my_atoms[k].num_hbonds;
    }

  } else {

    // fill in reax datastructures

    write_reax_atoms();

    // reset the bond list info for new atoms

    for (int k = oldN; k < api->system->N; ++k)
      Set_End_Index(k, Start_Index(k, api->lists+BONDS), api->lists+BONDS);

    // check if I need to shrink/extend my data-structs

    ReAllocate(api->system, api->control, api->data, api->workspace, &api->lists);
  }

  bigint local_ngroup = list->inum;
  MPI_Allreduce(&local_ngroup, &ngroup, 1, MPI_LMP_BIGINT, MPI_SUM, world);
}

/* ---------------------------------------------------------------------- */

double PairReaxC::init_one(int i, int j)
{
  if (setflag[i][j] == 0) error->all(FLERR,"All pair coeffs are not set");

  cutghost[i][j] = cutghost[j][i] = cutmax;
  return cutmax;
}

/* ---------------------------------------------------------------------- */

void PairReaxC::compute(int eflag, int vflag)
{
  double evdwl,ecoul;

  // communicate num_bonds once every reneighboring
  // 2 num arrays stored by fix, grab ptr to them

  if (neighbor->ago == 0) comm->forward_comm_fix(fix_reax);
  int *num_bonds = fix_reax->num_bonds;
  int *num_hbonds = fix_reax->num_hbonds;

  evdwl = ecoul = 0.0;
  ev_init(eflag,vflag);

  if (vflag_global) api->control->virial = 1;
  else api->control->virial = 0;

  api->system->n = atom->nlocal; // my atoms
  api->system->N = atom->nlocal + atom->nghost; // mine + ghosts
  api->system->bigN = static_cast<int> (atom->natoms);  // all atoms in the system

  // setup data structures

  setup();

  Reset(api->system, api->control, api->data, api->workspace, &api->lists);
  api->workspace->realloc.num_far = write_reax_lists();

  // forces

  Compute_Forces(api->system,api->control,api->data,api->workspace,&api->lists,api->out_control);
  read_reax_forces(vflag);

  for (int k = 0; k < api->system->N; ++k) {
    num_bonds[k] = api->system->my_atoms[k].num_bonds;
    num_hbonds[k] = api->system->my_atoms[k].num_hbonds;
  }

  // energies and pressure

  if (eflag_global) {
    evdwl += api->data->my_en.e_bond;
    evdwl += api->data->my_en.e_ov;
    evdwl += api->data->my_en.e_un;
    evdwl += api->data->my_en.e_lp;
    evdwl += api->data->my_en.e_ang;
    evdwl += api->data->my_en.e_pen;
    evdwl += api->data->my_en.e_coa;
    evdwl += api->data->my_en.e_hb;
    evdwl += api->data->my_en.e_tor;
    evdwl += api->data->my_en.e_con;
    evdwl += api->data->my_en.e_vdW;

    ecoul += api->data->my_en.e_ele;
    ecoul += api->data->my_en.e_pol;

    // eng_vdwl += evdwl;
    // eng_coul += ecoul;

    // Store the different parts of the energy
    // in a list for output by compute pair command

    pvector[0] = api->data->my_en.e_bond;
    pvector[1] = api->data->my_en.e_ov + api->data->my_en.e_un;
    pvector[2] = api->data->my_en.e_lp;
    pvector[3] = 0.0;
    pvector[4] = api->data->my_en.e_ang;
    pvector[5] = api->data->my_en.e_pen;
    pvector[6] = api->data->my_en.e_coa;
    pvector[7] = api->data->my_en.e_hb;
    pvector[8] = api->data->my_en.e_tor;
    pvector[9] = api->data->my_en.e_con;
    pvector[10] = api->data->my_en.e_vdW;
    pvector[11] = api->data->my_en.e_ele;
    pvector[12] = 0.0;
    pvector[13] = api->data->my_en.e_pol;
  }

  if (vflag_fdotr) virial_fdotr_compute();

// Set internal timestep counter to that of LAMMPS

  api->data->step = update->ntimestep;

  Output_Results(api->system, api->control, api->data, &api->lists, api->out_control, world);

  // populate tmpid and tmpbo arrays for fix reax/c/species
  int i, j;

  if (fixspecies_flag) {
    if (api->system->N > nmax) {
      memory->destroy(tmpid);
      memory->destroy(tmpbo);
      nmax = api->system->N;
      memory->create(tmpid,nmax,MAXSPECBOND,"pair:tmpid");
      memory->create(tmpbo,nmax,MAXSPECBOND,"pair:tmpbo");
    }

    for (i = 0; i < api->system->N; i ++)
      for (j = 0; j < MAXSPECBOND; j ++) {
        tmpbo[i][j] = 0.0;
        tmpid[i][j] = 0;
      }
    FindBond();
  }

}

/* ---------------------------------------------------------------------- */

void PairReaxC::write_reax_atoms()
{
  int *num_bonds = fix_reax->num_bonds;
  int *num_hbonds = fix_reax->num_hbonds;

  if (api->system->N > api->system->total_cap)
    error->all(FLERR,"Too many ghost atoms");

  for (int i = 0; i < api->system->N; ++i) {
    api->system->my_atoms[i].orig_id = atom->tag[i];
    api->system->my_atoms[i].type = map[atom->type[i]];
    api->system->my_atoms[i].x[0] = atom->x[i][0];
    api->system->my_atoms[i].x[1] = atom->x[i][1];
    api->system->my_atoms[i].x[2] = atom->x[i][2];
    api->system->my_atoms[i].q = atom->q[i];
    api->system->my_atoms[i].num_bonds = num_bonds[i];
    api->system->my_atoms[i].num_hbonds = num_hbonds[i];
  }
}

/* ---------------------------------------------------------------------- */

void PairReaxC::get_distance(rvec xj, rvec xi, double *d_sqr, rvec *dvec)
{
  (*dvec)[0] = xj[0] - xi[0];
  (*dvec)[1] = xj[1] - xi[1];
  (*dvec)[2] = xj[2] - xi[2];
  *d_sqr = SQR((*dvec)[0]) + SQR((*dvec)[1]) + SQR((*dvec)[2]);
}

/* ---------------------------------------------------------------------- */

void PairReaxC::set_far_nbr(far_neighbor_data *fdest,
                            int j, double d, rvec dvec)
{
  fdest->nbr = j;
  fdest->d = d;
  rvec_Copy(fdest->dvec, dvec);
  ivec_MakeZero(fdest->rel_box);
}

/* ---------------------------------------------------------------------- */

int PairReaxC::estimate_reax_lists()
{
  int itr_i, itr_j, i, j;
  int num_nbrs, num_marked;
  int *ilist, *jlist, *numneigh, **firstneigh, *marked;
  double d_sqr;
  rvec dvec;
  double **x;

  int mincap = api->system->mincap;
  double safezone = api->system->safezone;

  x = atom->x;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  num_nbrs = 0;
  num_marked = 0;
  marked = (int*) calloc(api->system->N, sizeof(int));

  int numall = list->inum + list->gnum;

  for (itr_i = 0; itr_i < numall; ++itr_i) {
    i = ilist[itr_i];
    marked[i] = 1;
    ++num_marked;
    jlist = firstneigh[i];

    for (itr_j = 0; itr_j < numneigh[i]; ++itr_j) {
      j = jlist[itr_j];
      j &= NEIGHMASK;
      get_distance(x[j], x[i], &d_sqr, &dvec);

      if (d_sqr <= SQR(api->control->nonb_cut))
        ++num_nbrs;
    }
  }

  free(marked);

  return static_cast<int> (MAX(num_nbrs*safezone, mincap*REAX_MIN_NBRS));
}

/* ---------------------------------------------------------------------- */

int PairReaxC::write_reax_lists()
{
  int itr_i, itr_j, i, j;
  int num_nbrs;
  int *ilist, *jlist, *numneigh, **firstneigh;
  double d_sqr, cutoff_sqr;
  rvec dvec;
  double *dist, **x;
  reax_list *far_nbrs;
  far_neighbor_data *far_list;

  x = atom->x;
  ilist = list->ilist;
  numneigh = list->numneigh;
  firstneigh = list->firstneigh;

  far_nbrs = api->lists + FAR_NBRS;
  far_list = far_nbrs->select.far_nbr_list;

  num_nbrs = 0;
  int inum = list->inum;
  dist = (double*) calloc(api->system->N, sizeof(double));

  int numall = list->inum + list->gnum;

  for (itr_i = 0; itr_i < numall; ++itr_i) {
    i = ilist[itr_i];
    jlist = firstneigh[i];
    Set_Start_Index(i, num_nbrs, far_nbrs);

    if (i < inum)
      cutoff_sqr = SQR(api->control->nonb_cut);
    else
      cutoff_sqr = SQR(api->control->bond_cut);

    for (itr_j = 0; itr_j < numneigh[i]; ++itr_j) {
      j = jlist[itr_j];
      j &= NEIGHMASK;
      get_distance(x[j], x[i], &d_sqr, &dvec);

      if (d_sqr <= (cutoff_sqr)) {
        dist[j] = sqrt(d_sqr);
        set_far_nbr(&far_list[num_nbrs], j, dist[j], dvec);
        ++num_nbrs;
      }
    }
    Set_End_Index(i, num_nbrs, far_nbrs);
  }

  free(dist);

  return num_nbrs;
}

/* ---------------------------------------------------------------------- */

void PairReaxC::read_reax_forces(int /*vflag*/)
{
  for (int i = 0; i < api->system->N; ++i) {
    api->system->my_atoms[i].f[0] = api->workspace->f[i][0];
    api->system->my_atoms[i].f[1] = api->workspace->f[i][1];
    api->system->my_atoms[i].f[2] = api->workspace->f[i][2];

    atom->f[i][0] += -api->workspace->f[i][0];
    atom->f[i][1] += -api->workspace->f[i][1];
    atom->f[i][2] += -api->workspace->f[i][2];
  }

}

/* ---------------------------------------------------------------------- */

void *PairReaxC::extract(const char *str, int &dim)
{
  dim = 1;
  if (strcmp(str,"chi") == 0 && chi) {
    for (int i = 1; i <= atom->ntypes; i++)
      if (map[i] >= 0) chi[i] = api->system->reax_param.sbp[map[i]].chi;
      else chi[i] = 0.0;
    return (void *) chi;
  }
  if (strcmp(str,"eta") == 0 && eta) {
    for (int i = 1; i <= atom->ntypes; i++)
      if (map[i] >= 0) eta[i] = api->system->reax_param.sbp[map[i]].eta;
      else eta[i] = 0.0;
    return (void *) eta;
  }
  if (strcmp(str,"gamma") == 0 && gamma) {
    for (int i = 1; i <= atom->ntypes; i++)
      if (map[i] >= 0) gamma[i] = api->system->reax_param.sbp[map[i]].gamma;
      else gamma[i] = 0.0;
    return (void *) gamma;
  }
  return nullptr;
}

/* ---------------------------------------------------------------------- */

double PairReaxC::memory_usage()
{
  double bytes = 0.0;

  // From pair_reax_c
  bytes += (double)1.0 * api->system->N * sizeof(int);
  bytes += (double)1.0 * api->system->N * sizeof(double);

  // From reaxc_allocate: BO
  bytes += (double)1.0 * api->system->total_cap * sizeof(reax_atom);
  bytes += (double)19.0 * api->system->total_cap * sizeof(double);
  bytes += (double)3.0 * api->system->total_cap * sizeof(int);

  // From reaxc_lists
  bytes += (double)2.0 * api->lists->n * sizeof(int);
  bytes += (double)api->lists->num_intrs * sizeof(three_body_interaction_data);
  bytes += (double)api->lists->num_intrs * sizeof(bond_data);
  bytes += (double)api->lists->num_intrs * sizeof(dbond_data);
  bytes += (double)api->lists->num_intrs * sizeof(dDelta_data);
  bytes += (double)api->lists->num_intrs * sizeof(far_neighbor_data);
  bytes += (double)api->lists->num_intrs * sizeof(hbond_data);

  if (fixspecies_flag)
    bytes += (double)2 * nmax * MAXSPECBOND * sizeof(double);

  return bytes;
}

/* ---------------------------------------------------------------------- */

void PairReaxC::FindBond()
{
  int i, j, pj, nj;
  double bo_tmp, bo_cut;

  bond_data *bo_ij;
  bo_cut = 0.10;

  for (i = 0; i < api->system->n; i++) {
    nj = 0;
    for (pj = Start_Index(i, api->lists); pj < End_Index(i, api->lists); ++pj) {
      bo_ij = &(api->lists->select.bond_list[pj]);
      j = bo_ij->nbr;
      if (j < i) continue;

      bo_tmp = bo_ij->bo_data.BO;

      if (bo_tmp >= bo_cut) {
        tmpid[i][nj] = j;
        tmpbo[i][nj] = bo_tmp;
        nj ++;
        if (nj > MAXSPECBOND) error->all(FLERR,"Increase MAXSPECBOND in reaxc_defs.h");
      }
    }
  }
}
