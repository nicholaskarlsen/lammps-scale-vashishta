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
#include "lmptype.h"
#include "comm_tiled.h"
#include "comm_brick.h"
#include "atom.h"
#include "atom_vec.h"
#include "domain.h"
#include "force.h"
#include "pair.h"
#include "neighbor.h"
#include "modify.h"
#include "fix.h"
#include "compute.h"
#include "output.h"
#include "dump.h"
#include "memory.h"
#include "error.h"

using namespace LAMMPS_NS;

#define BUFFACTOR 1.5
#define BUFFACTOR 1.5
#define BUFMIN 1000
#define BUFEXTRA 1000
#define EPSILON 1.0e-6

// NOTE: change this to 16 after debugged

#define DELTA_PROCS 1

enum{SINGLE,MULTI};               // same as in Comm
enum{LAYOUT_UNIFORM,LAYOUT_NONUNIFORM,LAYOUT_TILED};    // several files

/* ---------------------------------------------------------------------- */

CommTiled::CommTiled(LAMMPS *lmp) : Comm(lmp)
{
  error->all(FLERR,"Comm_style tiled is not yet supported");

  style = 1;
  layout = LAYOUT_UNIFORM;
  init_buffers();
}

/* ---------------------------------------------------------------------- */

CommTiled::CommTiled(LAMMPS *lmp, Comm *oldcomm) : Comm(*oldcomm)
{
  error->all(FLERR,"Comm_style tiled is not yet supported");

  style = 1;
  layout = oldcomm->layout;
  copy_arrays(oldcomm);
  init_buffers();
}

/* ---------------------------------------------------------------------- */

CommTiled::~CommTiled()
{
  memory->destroy(buf_send);
  memory->destroy(buf_recv);
  memory->destroy(overlap);
  deallocate_swap(nswap);
  memory->sfree(rcbinfo);
}

/* ----------------------------------------------------------------------
   initialize comm buffers and other data structs local to CommTiled
   NOTE: if this is identical to CommBrick, put it into Comm ??
------------------------------------------------------------------------- */

void CommTiled::init_buffers()
{
  maxexchange = maxexchange_atom + maxexchange_fix;
  bufextra = maxexchange + BUFEXTRA;

  maxsend = BUFMIN;
  memory->create(buf_send,maxsend+bufextra,"comm:buf_send");
  maxrecv = BUFMIN;
  memory->create(buf_recv,maxrecv,"comm:buf_recv");

  maxoverlap = 0;
  overlap = NULL;

  nswap = 2 * domain->dimension;
  allocate_swap(nswap);

  rcbinfo = NULL;
}

/* ----------------------------------------------------------------------
   NOTE: if this is nearly identical to CommBrick, put it into Comm ??
------------------------------------------------------------------------- */

void CommTiled::init()
{
  triclinic = domain->triclinic;
  map_style = atom->map_style;

  // temporary restrictions

  if (triclinic) 
    error->all(FLERR,"Cannot yet use comm_style tiled with triclinic box");
  if (mode == MULTI)
    error->all(FLERR,"Cannot yet use comm_style tiled with multi-mode comm");

  // comm_only = 1 if only x,f are exchanged in forward/reverse comm
  // comm_x_only = 0 if ghost_velocity since velocities are added

  comm_x_only = atom->avec->comm_x_only;
  comm_f_only = atom->avec->comm_f_only;
  if (ghost_velocity) comm_x_only = 0;

  // set per-atom sizes for forward/reverse/border comm
  // augment by velocity and fix quantities if needed

  size_forward = atom->avec->size_forward;
  size_reverse = atom->avec->size_reverse;
  size_border = atom->avec->size_border;

  if (ghost_velocity) size_forward += atom->avec->size_velocity;
  if (ghost_velocity) size_border += atom->avec->size_velocity;

  for (int i = 0; i < modify->nfix; i++)
    size_border += modify->fix[i]->comm_border;

  // maxexchange = max # of datums/atom in exchange communication
  // maxforward = # of datums in largest forward communication
  // maxreverse = # of datums in largest reverse communication
  // query pair,fix,compute,dump for their requirements
  // pair style can force reverse comm even if newton off

  maxexchange = BUFMIN + maxexchange_fix;
  maxforward = MAX(size_forward,size_border);
  maxreverse = size_reverse;

  if (force->pair) maxforward = MAX(maxforward,force->pair->comm_forward);
  if (force->pair) maxreverse = MAX(maxreverse,force->pair->comm_reverse);

  for (int i = 0; i < modify->nfix; i++) {
    maxforward = MAX(maxforward,modify->fix[i]->comm_forward);
    maxreverse = MAX(maxreverse,modify->fix[i]->comm_reverse);
  }

  for (int i = 0; i < modify->ncompute; i++) {
    maxforward = MAX(maxforward,modify->compute[i]->comm_forward);
    maxreverse = MAX(maxreverse,modify->compute[i]->comm_reverse);
  }

  for (int i = 0; i < output->ndump; i++) {
    maxforward = MAX(maxforward,output->dump[i]->comm_forward);
    maxreverse = MAX(maxreverse,output->dump[i]->comm_reverse);
  }

  if (force->newton == 0) maxreverse = 0;
  if (force->pair) maxreverse = MAX(maxreverse,force->pair->comm_reverse_off);
}

/* ----------------------------------------------------------------------
   setup spatial-decomposition communication patterns
   function of neighbor cutoff(s) & cutghostuser & current box size and tiling
------------------------------------------------------------------------- */

void CommTiled::setup()
{
  int i,j,n;

  // domain properties used in setup method and methods it calls

  dimension = domain->dimension;
  prd = domain->prd;
  boxlo = domain->boxlo;
  boxhi = domain->boxhi;
  sublo = domain->sublo;
  subhi = domain->subhi;

  int *periodicity = domain->periodicity;

  // set function pointers

  if (layout != LAYOUT_TILED) {
    box_drop = &CommTiled::box_drop_brick;
    box_other = &CommTiled::box_other_brick;
    box_touch = &CommTiled::box_touch_brick;
    point_drop = &CommTiled::point_drop_brick;
  } else {
    box_drop = &CommTiled::box_drop_tiled;
    box_other = &CommTiled::box_other_tiled;
    box_touch = &CommTiled::box_touch_tiled;
    point_drop = &CommTiled::point_drop_tiled;
  }

  // if RCB decomp exists and just changed, gather needed global RCB info

  if (rcbnew) {
    if (!rcbinfo) 
      rcbinfo = (RCBinfo *) 
        memory->smalloc(nprocs*sizeof(RCBinfo),"comm:rcbinfo");
    rcbnew = 0;
    RCBinfo rcbone;
    memcpy(&rcbone.mysplit[0][0],&mysplit[0][0],6*sizeof(double));
    rcbone.cutfrac = rcbcutfrac;
    rcbone.dim = rcbcutdim;
    MPI_Allgather(&rcbone,sizeof(RCBinfo),MPI_CHAR,
                  rcbinfo,sizeof(RCBinfo),MPI_CHAR,world);
  }

  // check that cutoff < any periodic box length

  double cut = MAX(neighbor->cutneighmax,cutghostuser);
  cutghost[0] = cutghost[1] = cutghost[2] = cut;
  
  if ((periodicity[0] && cut > prd[0]) ||
      (periodicity[1] && cut > prd[1]) ||
      (dimension == 3 && periodicity[2] && cut > prd[2]))
    error->all(FLERR,"Communication cutoff for comm_style tiled "
               "cannot exceed periodic box length");

  // setup forward/reverse communication
  // loop over 6 swap directions
  // determine which procs I will send to and receive from in each swap
  // done by intersecting ghost box with all proc sub-boxes it overlaps
  // sets nsendproc, nrecvproc, sendproc, recvproc
  // sets sendother, recvother, sendself, pbc_flag, pbc, sendbox
  // resets nprocmax

  int noverlap1,indexme;
  double lo1[3],hi1[3],lo2[3],hi2[3];
  int one,two;

  int iswap = 0;
  for (int idim = 0; idim < dimension; idim++) {
    for (int idir = 0; idir < 2; idir++) {

      // one = first ghost box in same periodic image
      // two = second ghost box wrapped across periodic boundary
      // either may not exist

      one = 1;
      lo1[0] = sublo[0]; lo1[1] = sublo[1]; lo1[2] = sublo[2];
      hi1[0] = subhi[0]; hi1[1] = subhi[1]; hi1[2] = subhi[2];
      if (idir == 0) {
        lo1[idim] = sublo[idim] - cut;
        hi1[idim] = sublo[idim];
      } else {
        lo1[idim] = subhi[idim];
        hi1[idim] = subhi[idim] + cut;
      }
      
      two = 0;
      if (idir == 0 && periodicity[idim] && lo1[idim] < boxlo[idim]) two = 1;
      if (idir == 1 && periodicity[idim] && hi1[idim] > boxhi[idim]) two = 1;

      if (two) {
        lo2[0] = sublo[0]; lo2[1] = sublo[1]; lo2[2] = sublo[2];
        hi2[0] = subhi[0]; hi2[1] = subhi[1]; hi2[2] = subhi[2];
        if (idir == 0) {
          lo2[idim] = lo1[idim] + prd[idim];
          hi2[idim] = boxhi[idim];
          if (sublo[idim] == boxlo[idim]) one = 0;
        } else {
          lo2[idim] = boxlo[idim];
          hi2[idim] = hi1[idim] - prd[idim];
          if (subhi[idim] == boxhi[idim]) one = 0;
        }
      }

      if (one) {
        if (idir == 0) lo1[idim] = MAX(lo1[idim],boxlo[idim]);
        else hi1[idim] = MIN(hi1[idim],boxhi[idim]);
        if (lo1[idim] == hi1[idim]) one = 0;
      }

      // noverlap = # of overlaps of box1/2 with procs via box_drop()
      // overlap = list of overlapping procs
      // if overlap with self, indexme = index of me in list

      indexme = -1;
      noverlap = 0;
      if (one) (this->*box_drop)(idim,lo1,hi1,indexme);
      noverlap1 = noverlap;
      if (two) (this->*box_drop)(idim,lo2,hi2,indexme);

      // if self is in overlap list, move it to end of list

      if (indexme >= 0) {
        int tmp = overlap[noverlap-1];
        overlap[noverlap-1] = overlap[indexme];
        overlap[indexme] = tmp;
      }

      // reallocate 2nd dimensions of all send/recv arrays, based on noverlap
      // # of sends of this swap = # of recvs of iswap +/- 1

      if (noverlap > nprocmax[iswap]) {
        int oldmax = nprocmax[iswap];
        while (nprocmax[iswap] < noverlap) nprocmax[iswap] += DELTA_PROCS;
        grow_swap_send(iswap,nprocmax[iswap],oldmax);
        if (idir == 0) grow_swap_recv(iswap+1,nprocmax[iswap]);
        else grow_swap_recv(iswap-1,nprocmax[iswap]);
      }

      // overlap how has list of noverlap procs
      // includes PBC effects

      if (noverlap && overlap[noverlap-1] == me) sendself[iswap] = 1;
      else sendself[iswap] = 0;
      if (noverlap && noverlap-sendself[iswap]) sendother[iswap] = 1;
      else sendother[iswap] = 0;

      nsendproc[iswap] = noverlap;
      for (i = 0; i < noverlap; i++) sendproc[iswap][i] = overlap[i];

      if (idir == 0) {
        recvother[iswap+1] = sendother[iswap];
        nrecvproc[iswap+1] = noverlap;
        for (i = 0; i < noverlap; i++) recvproc[iswap+1][i] = overlap[i];
      } else {
        recvother[iswap-1] = sendother[iswap];
        nrecvproc[iswap-1] = noverlap;
        for (i = 0; i < noverlap; i++) recvproc[iswap-1][i] = overlap[i];
      }

      // compute sendbox for each of my sends
      // obox = intersection of ghostbox with other proc's sub-domain
      // sbox = what I need to send to other proc
      //      = sublo to MIN(sublo+cut,subhi) in idim, for idir = 0
      //      = MIN(subhi-cut,sublo) to subhi in idim, for idir = 1
      //      = obox in other 2 dims
      // if sbox touches other proc's sub-box boundaries in lower dims,
      //   extend sbox in those lower dims to include ghost atoms
      
      double oboxlo[3],oboxhi[3],sbox[6];

      for (i = 0; i < noverlap; i++) {
        pbc_flag[iswap][i] = 0;
        pbc[iswap][i][0] = pbc[iswap][i][1] = pbc[iswap][i][2] =
          pbc[iswap][i][3] = pbc[iswap][i][4] = pbc[iswap][i][5] = 0;
        
        (this->*box_other)(idim,idir,overlap[i],oboxlo,oboxhi);
        
        if (i < noverlap1) {
          sbox[0] = MAX(oboxlo[0],lo1[0]);
          sbox[1] = MAX(oboxlo[1],lo1[1]);
          sbox[2] = MAX(oboxlo[2],lo1[2]);
          sbox[3] = MIN(oboxhi[0],hi1[0]);
          sbox[4] = MIN(oboxhi[1],hi1[1]);
          sbox[5] = MIN(oboxhi[2],hi1[2]);
        } else {
          pbc_flag[iswap][i] = 1;
          if (idir == 0) pbc[iswap][i][idim] = 1;
          else pbc[iswap][i][idim] = -1;
          sbox[0] = MAX(oboxlo[0],lo2[0]);
          sbox[1] = MAX(oboxlo[1],lo2[1]);
          sbox[2] = MAX(oboxlo[2],lo2[2]);
          sbox[3] = MIN(oboxhi[0],hi2[0]);
          sbox[4] = MIN(oboxhi[1],hi2[1]);
          sbox[5] = MIN(oboxhi[2],hi2[2]);
        }

        if (idir == 0) {
          sbox[idim] = sublo[idim];
          if (i < noverlap1) sbox[3+idim] = MIN(sbox[3+idim]+cut,subhi[idim]);
          else sbox[3+idim] = MIN(sbox[3+idim]-prd[idim]+cut,subhi[idim]);
        } else {
          if (i < noverlap1) sbox[idim] = MAX(sbox[idim]-cut,sublo[idim]);
          else sbox[idim] = MAX(sbox[idim]+prd[idim]-cut,sublo[idim]);
          sbox[3+idim] = subhi[idim];
        }

        if (idim >= 1) {
          if (sbox[0] == oboxlo[0]) sbox[0] -= cut;
          if (sbox[3] == oboxhi[0]) sbox[3] += cut;
        }
        if (idim == 2) {
          if (sbox[1] == oboxlo[1]) sbox[1] -= cut;
          if (sbox[4] == oboxhi[1]) sbox[4] += cut;
        }
        
        memcpy(sendbox[iswap][i],sbox,6*sizeof(double));
      }

      iswap++;
    }
  }

  // setup exchange communication = subset of forward/reverse comm procs
  // loop over dimensions
  // determine which procs I will exchange with in each dimension
  // subset of procs that touch my proc in forward/reverse comm
  // sets nexchproc & exchproc, resets nexchprocmax

  int proc;

  for (int idim = 0; idim < dimension; idim++) {

    // overlap = list of procs that touch my sub-box in idim
    // proc can appear twice in list if touches in both directions
    // 2nd add-to-list checks to insure each proc appears exactly once

    noverlap = 0;
    iswap = 2*idim;
    n = nsendproc[iswap];
    for (i = 0; i < n; i++) {
      proc = sendproc[iswap][i];
      if (proc == me) continue;
      if ((this->*box_touch)(proc,idim,0)) {
        if (noverlap == maxoverlap) {
          maxoverlap += DELTA_PROCS;
          memory->grow(overlap,maxoverlap,"comm:overlap");
        }
        overlap[noverlap++] = proc;
      }
    }
    noverlap1 = noverlap;
    iswap = 2*idim+1;
    n = nsendproc[iswap];

    MPI_Barrier(world);

    for (i = 0; i < n; i++) {
      proc = sendproc[iswap][i];
      if (proc == me) continue;
      if ((this->*box_touch)(proc,idim,1)) {
        for (j = 0; j < noverlap1; j++)
          if (overlap[j] == proc) break;
        if (j < noverlap1) continue;
        if (noverlap == maxoverlap) {
          maxoverlap += DELTA_PROCS;
          memory->grow(overlap,maxoverlap,"comm:overlap");
        }
        overlap[noverlap++] = proc;
      }
    }

    MPI_Barrier(world);

    // reallocate exchproc and exchnum if needed based on noverlap
    
    if (noverlap > nexchprocmax[idim]) {
      while (nexchprocmax[idim] < noverlap) nexchprocmax[idim] += DELTA_PROCS;
      delete [] exchproc[idim];
      exchproc[idim] = new int[nexchprocmax[idim]];
      delete [] exchnum[idim];
      exchnum[idim] = new int[nexchprocmax[idim]];
    }

    nexchproc[idim] = noverlap;
    for (i = 0; i < noverlap; i++) exchproc[idim][i] = overlap[i];
  }

  // reallocate MPI Requests and Statuses as needed

  int nmax = 0;
  for (i = 0; i < nswap; i++) nmax = MAX(nmax,nprocmax[i]);
  for (i = 0; i < dimension; i++) nmax = MAX(nmax,nexchprocmax[i]);
  if (nmax > maxreqstat) {
    maxreqstat = nmax;
    delete [] requests;
    delete [] statuses;
    requests = new MPI_Request[maxreqstat];
    statuses = new MPI_Status[maxreqstat];
  }

  // DEBUG

  /*
  MPI_Barrier(world);

  printf("SUBBOX %d: %g %g %g: %g %g %g\n",
         me,sublo[0],sublo[1],sublo[2],subhi[0],subhi[1],subhi[2]);
  MPI_Barrier(world);

  for (i = 0; i < nswap; i++) {
    if (nsendproc[i] == 1)
      printf("SETUP SEND %d %d: nsend %d self %d sproc0 %d: "
             "%g %g %g: %g %g %g\n",
             i,me,nsendproc[i],sendself[i],sendproc[i][0],
             sendbox[i][0][0],
             sendbox[i][0][1],
             sendbox[i][0][2],
             sendbox[i][0][3],
             sendbox[i][0][4],
             sendbox[i][0][5]);
    else if (nsendproc[i] == 2) 
      printf("SETUP SEND %d %d: nsend %d self %d sprocs %d %d: "
             "%g %g %g: %g %g %g: %g %g %g: %g %g %g\n",
             i,me,nsendproc[i],sendself[i],sendproc[i][0],sendproc[i][1],
             sendbox[i][0][0],
             sendbox[i][0][1],
             sendbox[i][0][2],
             sendbox[i][0][3],
             sendbox[i][0][4],
             sendbox[i][0][5],
             sendbox[i][1][0],
             sendbox[i][1][1],
             sendbox[i][1][2],
             sendbox[i][1][3],
             sendbox[i][1][4],
             sendbox[i][1][5]);
    else if (nsendproc[i] == 3) 
      printf("SETUP SEND %d %d: nsend %d self %d sprocs %d %d %d: "
             "%g %g %g: %g %g %g: %g %g %g: %g %g %g: %g %g %g: %g %g %g\n",
             i,me,nsendproc[i],sendself[i],sendproc[i][0],sendproc[i][1],
             sendproc[i][2],
             sendbox[i][0][0],
             sendbox[i][0][1],
             sendbox[i][0][2],
             sendbox[i][0][3],
             sendbox[i][0][4],
             sendbox[i][0][5],
             sendbox[i][1][0],
             sendbox[i][1][1],
             sendbox[i][1][2],
             sendbox[i][1][3],
             sendbox[i][1][4],
             sendbox[i][1][5],
             sendbox[i][2][0],
             sendbox[i][2][1],
             sendbox[i][2][2],
             sendbox[i][2][3],
             sendbox[i][2][4],
             sendbox[i][2][5]);
    else if (nsendproc[i] >= 4)
      printf("SETUP SEND %d %d: nsend %d self %d sprocs %d %d %d %d: "
             "%g %g %g: %g %g %g: %g %g %g: %g %g %g: %g %g %g: %g %g %g"
             " %g %g %g: %g %g %g\n",
             i,me,nsendproc[i],sendself[i],sendproc[i][0],sendproc[i][1],
             sendproc[i][2],sendproc[i][3],
             sendbox[i][0][0],
             sendbox[i][0][1],
             sendbox[i][0][2],
             sendbox[i][0][3],
             sendbox[i][0][4],
             sendbox[i][0][5],
             sendbox[i][1][0],
             sendbox[i][1][1],
             sendbox[i][1][2],
             sendbox[i][1][3],
             sendbox[i][1][4],
             sendbox[i][1][5],
             sendbox[i][2][0],
             sendbox[i][2][1],
             sendbox[i][2][2],
             sendbox[i][2][3],
             sendbox[i][2][4],
             sendbox[i][2][5],
             sendbox[i][3][0],
             sendbox[i][3][1],
             sendbox[i][3][2],
             sendbox[i][3][3],
             sendbox[i][3][4],
             sendbox[i][3][5]);
    if (nrecvproc[i] == 1)
      printf("SETUP RECV %d %d: nrecv %d other %d rproc0 %d\n",
             i,me,nrecvproc[i],sendother[i],recvproc[i][0]);
    else if (nrecvproc[i] >= 2)
      printf("SETUP RECV %d %d: nrecv %d other %d rprocs %d %d\n",
             i,me,nrecvproc[i],sendother[i],recvproc[i][0],recvproc[i][1]);
  }

  for (i = 0; i < dimension; i++) {
    if (nexchproc[i] == 1)
      printf("SETUP EXCH %d %d: nexch %d exch %d\n",
             i,me,nexchproc[i],exchproc[i][0]);
    else if (nexchproc[i] == 2)
      printf("SETUP EXCH %d %d: nexch %d exch %d %d\n",
             i,me,nexchproc[i],exchproc[i][0],exchproc[i][1]);
    else if (nexchproc[i] == 3)
      printf("SETUP EXCH %d %d: nexch %d exch %d %d %d\n",
             i,me,nexchproc[i],exchproc[i][0],exchproc[i][1],exchproc[i][2]);
    else if (nexchproc[i] >= 4)
      printf("SETUP EXCH %d %d: nexch %d exch %d %d %d %d\n",
             i,me,nexchproc[i],exchproc[i][0],exchproc[i][1],
             exchproc[i][2],exchproc[i][3]);
  }

  MPI_Barrier(world);
  */
}

/* ----------------------------------------------------------------------
   forward communication of atom coords every timestep
   other per-atom attributes may also be sent via pack/unpack routines
------------------------------------------------------------------------- */

void CommTiled::forward_comm(int dummy)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;
  AtomVec *avec = atom->avec;
  double **x = atom->x;

  // exchange data with another set of procs in each swap
  // post recvs from all procs except self
  // send data to all procs except self
  // copy data to self if sendself is set
  // wait on all procs except self and unpack received data
  // if comm_x_only set, exchange or copy directly to x, don't unpack

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (comm_x_only) {
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++)
          MPI_Irecv(x[firstrecv[iswap][i]],size_forward_recv[iswap][i],
                    MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
      }
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++) {
          n = avec->pack_comm(sendnum[iswap][i],sendlist[iswap][i],
                              buf_send,pbc_flag[iswap][i],pbc[iswap][i]);
          MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
        }
      }
      if (sendself[iswap]) {
        avec->pack_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                        x[firstrecv[iswap][nrecv]],pbc_flag[iswap][nsend],
                        pbc[iswap][nsend]);
      }
      if (recvother[iswap]) MPI_Waitall(nrecv,requests,statuses);

    } else if (ghost_velocity) {
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++)
          MPI_Irecv(&buf_recv[size_forward*forward_recv_offset[iswap][i]],
                    size_forward_recv[iswap][i],
                    MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
      }
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++) {
          n = avec->pack_comm_vel(sendnum[iswap][i],sendlist[iswap][i],
                                  buf_send,pbc_flag[iswap][i],pbc[iswap][i]);
          MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
        }
      }
      if (sendself[iswap]) {
        avec->pack_comm_vel(sendnum[iswap][nsend],sendlist[iswap][nsend],
                            buf_send,pbc_flag[iswap][nsend],pbc[iswap][nsend]);
        avec->unpack_comm_vel(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                              buf_send);
      }
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++) {
          MPI_Waitany(nrecv,requests,&irecv,&status);
          avec->unpack_comm_vel(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                                &buf_recv[size_forward*
                                          forward_recv_offset[iswap][irecv]]);
        }
      }

    } else {
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++)
          MPI_Irecv(&buf_recv[size_forward*forward_recv_offset[iswap][i]],
                    size_forward_recv[iswap][i],
                    MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
      }
      if (sendother[iswap]) {
        for (i = 0; i < nsendproc[iswap]; i++) {
          n = avec->pack_comm(sendnum[iswap][i],sendlist[iswap][i],
                              buf_send,pbc_flag[iswap][i],pbc[iswap][i]);
          MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
        }
      }
      if (sendself[iswap]) {
        avec->pack_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                        buf_send,pbc_flag[iswap][nsend],pbc[iswap][nsend]);
        avec->unpack_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                          buf_send);
      }
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++) {
          MPI_Waitany(nrecv,requests,&irecv,&status);
          avec->unpack_comm(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                            &buf_recv[size_forward*
                                      forward_recv_offset[iswap][irecv]]);
        }
      }
    }
  }
}

/* ----------------------------------------------------------------------
   reverse communication of forces on atoms every timestep
   other per-atom attributes may also be sent via pack/unpack routines
------------------------------------------------------------------------- */

void CommTiled::reverse_comm()
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;
  AtomVec *avec = atom->avec;
  double **f = atom->f;

  // exchange data with another set of procs in each swap
  // post recvs from all procs except self
  // send data to all procs except self
  // copy data to self if sendself is set
  // wait on all procs except self and unpack received data
  // if comm_f_only set, exchange or copy directly from f, don't pack

  for (int iswap = nswap-1; iswap >= 0; iswap--) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (comm_f_only) {
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++) {
          MPI_Irecv(&buf_recv[size_reverse*reverse_recv_offset[iswap][i]],
                    size_reverse_recv[iswap][i],MPI_DOUBLE,
                    sendproc[iswap][i],0,world,&requests[i]);
        }
      }
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++)
          MPI_Send(f[firstrecv[iswap][i]],size_reverse_send[iswap][i],
                   MPI_DOUBLE,recvproc[iswap][i],0,world);
      }
      if (sendself[iswap]) {
        avec->unpack_reverse(sendnum[iswap][nsend],sendlist[iswap][nsend],
                             f[firstrecv[iswap][nrecv]]);
      }
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++) {
          MPI_Waitany(nsend,requests,&irecv,&status);
          avec->unpack_reverse(sendnum[iswap][irecv],sendlist[iswap][irecv],
                               &buf_recv[size_reverse*
                                         reverse_recv_offset[iswap][irecv]]);
        }
      }
      
    } else {
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++)
          MPI_Irecv(&buf_recv[size_reverse*reverse_recv_offset[iswap][i]],
                    size_reverse_recv[iswap][i],MPI_DOUBLE,
                    sendproc[iswap][i],0,world,&requests[i]);
      }
      if (recvother[iswap]) {
        for (i = 0; i < nrecv; i++) {
          n = avec->pack_reverse(recvnum[iswap][i],firstrecv[iswap][i],
                                 buf_send);
          MPI_Send(buf_send,n,MPI_DOUBLE,recvproc[iswap][i],0,world);
        }
      }
      if (sendself[iswap]) {
        avec->pack_reverse(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                           buf_send);
        avec->unpack_reverse(sendnum[iswap][nsend],sendlist[iswap][nsend],
                             buf_send);
      }
      if (sendother[iswap]) {
        for (i = 0; i < nsend; i++) {
          MPI_Waitany(nsend,requests,&irecv,&status);
          avec->unpack_reverse(sendnum[iswap][irecv],sendlist[iswap][irecv],
                               &buf_recv[size_reverse*
                                         reverse_recv_offset[iswap][irecv]]);
        }
      }
    }
  }
}

/* ----------------------------------------------------------------------
   exchange: move atoms to correct processors
   NOTE: need to re-doc this
   atoms exchanged with all 6 stencil neighbors
   send out atoms that have left my box, receive ones entering my box
   atoms will be lost if not inside some proc's box
     can happen if atom moves outside of non-periodic bounary
     or if atom moves more than one proc away
   this routine called before every reneighboring
   for triclinic, atoms must be in lamda coords (0-1) before exchange is called
------------------------------------------------------------------------- */

void CommTiled::exchange()
{
  int i,m,nexch,nsend,nrecv,nlocal,proc,offset;
  double lo,hi,value;
  double **x;
  AtomVec *avec = atom->avec;

  // clear global->local map for owned and ghost atoms
  // b/c atoms migrate to new procs in exchange() and
  //   new ghosts are created in borders()
  // map_set() is done at end of borders()
  // clear ghost count and any ghost bonus data internal to AtomVec

  if (map_style) atom->map_clear();
  atom->nghost = 0;
  atom->avec->clear_bonus();

  // insure send buf is large enough for single atom
  // fixes can change per-atom size requirement on-the-fly

  int bufextra_old = bufextra;
  maxexchange = maxexchange_atom + maxexchange_fix;
  bufextra = maxexchange + BUFEXTRA;
  if (bufextra > bufextra_old)
    memory->grow(buf_send,maxsend+bufextra,"comm:buf_send");

  // domain properties used in exchange method and methods it calls
  // subbox bounds for orthogonal or triclinic

  prd = domain->prd;
  boxlo = domain->boxlo;
  boxhi = domain->boxhi;

  if (triclinic == 0) {
    sublo = domain->sublo;
    subhi = domain->subhi;
  } else {
    sublo = domain->sublo_lamda;
    subhi = domain->subhi_lamda;
  }

  // loop over dimensions

  dimension = domain->dimension;

  for (int dim = 0; dim < dimension; dim++) {

    // fill buffer with atoms leaving my box, using < and >=
    // when atom is deleted, fill it in with last atom

    x = atom->x;
    lo = sublo[dim];
    hi = subhi[dim];
    nlocal = atom->nlocal;
    i = nsend = 0;

    while (i < nlocal) {
      if (x[i][dim] < lo || x[i][dim] >= hi) {
        if (nsend > maxsend) grow_send(nsend,1);
        proc = (this->*point_drop)(dim,x[i]);

        /*
        // DEBUG:
        // test if proc is not in exch list, means will lose atom
        // could be that should lose atom
        int flag = 0;
        for (int k = 0; k < nexchproc[dim]; k++)
          if (proc == exchproc[k]) flag = 1;
        if (!flag) 
          printf("Losing exchange atom: dim %d me %d %proc %d: %g %g %g\n",
                 dim,me,proc,x[i][0],x[i][1],x[i][2]);
        */

        if (proc != me) {
          buf_send[nsend++] = proc;
          nsend += avec->pack_exchange(i,&buf_send[nsend]);
          avec->copy(nlocal-1,i,1);
          nlocal--;
        } else i++;
      } else i++;
    }

    atom->nlocal = nlocal;

    // send and recv atoms from neighbor procs that touch my sub-box in dim
    // no send/recv with self
    // send size of message first
    // receiver may receive multiple messages, realloc buf_recv if needed

    nexch = nexchproc[dim];
    if (!nexch) continue;

    for (m = 0; m < nexch; m++)
      MPI_Irecv(&exchnum[dim][m],1,MPI_INT,
                exchproc[dim][m],0,world,&requests[m]);
    for (m = 0; m < nexch; m++)
      MPI_Send(&nsend,1,MPI_INT,exchproc[dim][m],0,world);
    MPI_Waitall(nexch,requests,statuses);

    nrecv = 0;
    for (m = 0; m < nexch; m++) nrecv += exchnum[dim][m];
    if (nrecv > maxrecv) grow_recv(nrecv);

    offset = 0;
    for (m = 0; m < nexch; m++) {
      MPI_Irecv(&buf_recv[offset],exchnum[dim][m],
                MPI_DOUBLE,exchproc[dim][m],0,world,&requests[m]);
      offset += exchnum[dim][m];
    }
    for (m = 0; m < nexch; m++)
      MPI_Send(buf_send,nsend,MPI_DOUBLE,exchproc[dim][m],0,world);
    MPI_Waitall(nexch,requests,statuses);

    // check incoming atoms to see if I own it and they are in my box
    // if so, add to my list
    // box check is only for this dimension,
    //   atom may be passed to another proc in later dims

    m = 0;
    while (m < nrecv) {
      proc = static_cast<int> (buf_recv[m++]);
      if (proc == me) {
        value = buf_recv[m+dim+1];
        if (value >= lo && value < hi) {
          m += avec->unpack_exchange(&buf_recv[m]);
          continue;
        }
      }
      m += static_cast<int> (buf_recv[m]);
    }
  }

  if (atom->firstgroupname) atom->first_reorder();

  // DEBUG

  // check that total count of atoms is correct

  /*
  nlocal = atom->nlocal;
  int ntotal;
  MPI_Allreduce(&nlocal,&ntotal,1,MPI_INT,MPI_SUM,world);
  if (ntotal != atom->natoms) {
    if (me == 0) printf("Atom counts: old %d new %d\n",atom->natoms,ntotal);
    //error->all(FLERR,"Post-exchange atom count wrong");
  }

  // check that each proc owns exact atoms it should in its sub-box
  // lo/hi = first/last owned atom in lattice

  int ilo,jlo,klo,ihi,jhi,khi;
  bounds(sublo[0],sublo[1],sublo[2],subhi[0],subhi[1],subhi[2],
         ilo,jlo,klo,ihi,jhi,khi);
  int ncount = (ihi-ilo+1) * (jhi-jlo+1) * (khi-klo+1);

  if (ncount != nlocal) 
    printf("EXCH proc %d: correct %d actual %d\n",me,ncount,nlocal);
  int flag = 0;
  if (ncount != nlocal) {
    flag = 1;
    //for (int i = 0; i < nlocal; i++)
    //  printf("MISSING %d %d: %g %g %g\n",i,atom->tag[i],
    //         atom->x[i][0],atom->x[i][1],atom->x[i][2]);
  }
  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  if (flagall) error->all(FLERR,"Bad exchange results");
  else if (me == 0) printf("EXCH is correct\n");
  */
}

/* ----------------------------------------------------------------------
   convert box bounds into lattice indices
   assume static_int can be below/above neg/pos floating point value
   integer lo = integer >= floating point value
   integer hi = integer < floating point value
------------------------------------------------------------------------- */

void CommTiled::bounds(double xlo, double ylo, double zlo, 
                       double xhi, double yhi, double zhi,
                       int &ilo, int &jlo, int &klo, 
                       int &ihi, int &jhi, int &khi)
{
  ilo = static_cast<int> (xlo);
  if (ilo < xlo) ilo++;

  jlo = static_cast<int> (ylo);
  if (jlo < ylo) jlo++;

  klo = static_cast<int> (zlo);
  if (klo < zlo) klo++;

  ihi = static_cast<int> (xhi);
  if (ihi >= xhi) ihi--;

  jhi = static_cast<int> (yhi);
  if (jhi >= yhi) jhi--;

  khi = static_cast<int> (zhi);
  if (khi >= zhi) khi--;
}

/* ----------------------------------------------------------------------
   borders: list nearby atoms to send to neighboring procs at every timestep
   one list is created per swap/proc that will be made
   as list is made, actually do communication
   this does equivalent of a forward_comm(), so don't need to explicitly
     call forward_comm() on reneighboring timestep
   this routine is called before every reneighboring
   for triclinic, atoms must be in lamda coords (0-1) before borders is called
------------------------------------------------------------------------- */

void CommTiled::borders()
{
  int i,m,n,irecv,nlast,nsend,nrecv,ncount,ncountall;
  double xlo,xhi,ylo,yhi,zlo,zhi;
  double *bbox;
  double **x;
  MPI_Status status;
  AtomVec *avec = atom->avec;

  // send/recv max one = max # of atoms in single send/recv for any swap
  // send/recv max all = max # of atoms in all sends/recvs within any swap

  int smaxone = 0;
  int smaxall = 0;
  int rmaxone = 0;
  int rmaxall = 0;

  // loop over swaps in all dimensions

  for (int iswap = 0; iswap < nswap; iswap++) {

    // find atoms within rectangles using >= and <
    // hi test with ">" is important b/c don't want to send an atom
    //   in lower dim (on boundary) that a proc will recv again in higher dim
    // for x-dim swaps, check owned atoms
    // for yz-dim swaps, check owned and ghost atoms
    // store sent atom indices in list for use in future timesteps
    // NOTE: assume SINGLE mode, add back in logic for MULTI mode later
    //       and for ngroup when bordergroup is set

    x = atom->x;
    if (iswap % 2 == 0) nlast = atom->nlocal + atom->nghost;
      
    ncountall = 0;
    for (m = 0; m < nsendproc[iswap]; m++) {
      bbox = sendbox[iswap][m];
      xlo = bbox[0]; ylo = bbox[1]; zlo = bbox[2];
      xhi = bbox[3]; yhi = bbox[4]; zhi = bbox[5];

      ncount = 0;

      for (i = 0; i < nlast; i++) {
        if (x[i][0] >= xlo && x[i][0] < xhi &&
            x[i][1] >= ylo && x[i][1] < yhi &&
            x[i][2] >= zlo && x[i][2] < zhi) {
          if (ncount == maxsendlist[iswap][m]) grow_list(iswap,m,ncount);
          sendlist[iswap][m][ncount++] = i;
        }
      }

      sendnum[iswap][m] = ncount;
      smaxone = MAX(smaxone,ncount);
      ncountall += ncount;
    }
    smaxall = MAX(smaxall,ncountall);

    // send sendnum counts to procs who recv from me except self
    // copy data to self if sendself is set

    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (recvother[iswap])
      for (m = 0; m < nrecv; m++)
        MPI_Irecv(&recvnum[iswap][m],1,MPI_INT,
                  recvproc[iswap][m],0,world,&requests[m]);
    if (sendother[iswap])
      for (m = 0; m < nsend; m++)
        MPI_Send(&sendnum[iswap][m],1,MPI_INT,sendproc[iswap][m],0,world);
    if (sendself[iswap]) recvnum[iswap][nrecv] = sendnum[iswap][nsend];
    if (recvother[iswap]) MPI_Waitall(nrecv,requests,statuses);

    // setup other per swap/proc values from sendnum and recvnum

    for (m = 0; m < nsendproc[iswap]; m++) {
      size_reverse_recv[iswap][m] = sendnum[iswap][m]*size_reverse;
      if (m == 0) reverse_recv_offset[iswap][0] = 0;
      else reverse_recv_offset[iswap][m] = 
             reverse_recv_offset[iswap][m-1] + sendnum[iswap][m-1];
    }

    ncountall = 0;
    for (m = 0; m < nrecvproc[iswap]; m++) {
      ncount = recvnum[iswap][m];
      rmaxone = MAX(rmaxone,ncount);
      ncountall += ncount;

      size_forward_recv[iswap][m] = ncount*size_forward;
      size_reverse_send[iswap][m] = ncount*size_reverse;
      if (m == 0) {
        firstrecv[iswap][0] = atom->nlocal + atom->nghost;
        forward_recv_offset[iswap][0] = 0;
      } else {
        firstrecv[iswap][m] = firstrecv[iswap][m-1] + recvnum[iswap][m-1];
        forward_recv_offset[iswap][m] = 
          forward_recv_offset[iswap][m-1] + recvnum[iswap][m-1];
      }
    }
    rmaxall = MAX(rmaxall,ncountall);

    // insure send/recv buffers are large enough for this border comm swap

    if (smaxone*size_border > maxsend) grow_send(smaxone*size_border,0);
    if (rmaxall*size_border > maxrecv) grow_recv(rmaxall*size_border);

    // swap atoms with other procs using pack_border(), unpack_border()
    // use Waitall() instead of Waitany() because calls to unpack_border()
    //   must increment per-atom arrays in ascending order

    if (ghost_velocity) {
      if (recvother[iswap]) {
        for (m = 0; m < nrecv; m++)
          MPI_Irecv(&buf_recv[size_border*forward_recv_offset[iswap][m]],
                    recvnum[iswap][m]*size_border,
                    MPI_DOUBLE,recvproc[iswap][m],0,world,&requests[m]);
      }
      if (sendother[iswap]) {
        for (m = 0; m < nsend; m++) {
          n = avec->pack_border_vel(sendnum[iswap][m],sendlist[iswap][m],
                                    buf_send,pbc_flag[iswap][m],pbc[iswap][m]);
          MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][m],0,world);
        }
      }
      if (sendself[iswap]) {
        n = avec->pack_border_vel(sendnum[iswap][nsend],sendlist[iswap][nsend],
                                  buf_send,pbc_flag[iswap][nsend],
                                  pbc[iswap][nsend]);
        avec->unpack_border_vel(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                                buf_send);
      }
      if (recvother[iswap]) {
        MPI_Waitall(nrecv,requests,statuses);
        for (m = 0; m < nrecv; m++)
          avec->unpack_border_vel(recvnum[iswap][m],firstrecv[iswap][m],
                                  &buf_recv[size_border*
                                            forward_recv_offset[iswap][m]]);
      }

    } else {
      if (recvother[iswap]) {
        for (m = 0; m < nrecv; m++)
          MPI_Irecv(&buf_recv[size_border*forward_recv_offset[iswap][m]],
                    recvnum[iswap][m]*size_border,
                    MPI_DOUBLE,recvproc[iswap][m],0,world,&requests[m]);
      }
      if (sendother[iswap]) {
        for (m = 0; m < nsend; m++) {
          n = avec->pack_border(sendnum[iswap][m],sendlist[iswap][m],
                                buf_send,pbc_flag[iswap][m],pbc[iswap][m]);
          MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][m],0,world);
        }
      }
      if (sendself[iswap]) {
        n = avec->pack_border(sendnum[iswap][nsend],sendlist[iswap][nsend],
                              buf_send,pbc_flag[iswap][nsend],
                              pbc[iswap][nsend]);
        avec->unpack_border(recvnum[iswap][nsend],firstrecv[iswap][nsend],
                            buf_send);
      }
      if (recvother[iswap]) {
        MPI_Waitall(nrecv,requests,statuses);
        for (m = 0; m < nrecv; m++)
          avec->unpack_border(recvnum[iswap][m],firstrecv[iswap][m],
                              &buf_recv[size_border*
                                        forward_recv_offset[iswap][m]]);
      }
    }

    // increment ghost atoms

    n = nrecvproc[iswap];
    if (n) 
      atom->nghost += forward_recv_offset[iswap][n-1] + recvnum[iswap][n-1];
  }

  // insure send/recv buffers are long enough for all forward & reverse comm
  // send buf is for one forward or reverse sends to one proc
  // recv buf is for all forward or reverse recvs in one swap

  int max = MAX(maxforward*smaxone,maxreverse*rmaxone);
  if (max > maxsend) grow_send(max,0);
  max = MAX(maxforward*rmaxall,maxreverse*smaxall);
  if (max > maxrecv) grow_recv(max);

  // reset global->local map

  if (map_style) atom->map_set();

  // DEBUG

  /*
  MPI_Barrier(world);

  for (i = 0; i < nswap; i++) {
    if (nsendproc[i] == 1)
      printf("BORDERS SEND %d %d: proc %d snum0 %d\n",
             i,me,sendproc[i][0],sendnum[i][0]);
    else if (nsendproc[i] == 2)
      printf("BORDERS SEND %d %d: proc %d %d snums %d %d\n",
             i,me,sendproc[i][0],sendproc[i][1],sendnum[i][0],sendnum[i][1]);
    else if (nsendproc[i] == 3)
      printf("BORDERS SEND %d %d: proc %d %d %d snums %d %d %d\n",
             i,me,sendproc[i][0],sendproc[i][1],sendproc[i][2],
             sendnum[i][0],sendnum[i][1],sendnum[i][2]);
    else if (nsendproc[i] == 8)
      printf("BORDERS SEND %d %d: proc %d %d %d %d: %d %d %d %d "
             "snums %d %d %d %d: %d %d %d %d\n",
             i,me,
             sendproc[i][0],sendproc[i][1],sendproc[i][2],sendproc[i][3],
             sendproc[i][4],sendproc[i][5],sendproc[i][6],sendproc[i][7],
             sendnum[i][0],sendnum[i][1],sendnum[i][2],sendnum[i][3],
             sendnum[i][4],sendnum[i][5],sendnum[i][6],sendnum[i][7]);
    else if (nsendproc[i] >= 4)
      printf("BORDERS SEND %d %d: proc %d %d %d %d snums %d %d %d %d\n",
             i,me,sendproc[i][0],sendproc[i][1],sendproc[i][2],
             sendproc[i][3],
             sendnum[i][0],sendnum[i][1],sendnum[i][2],sendnum[i][3]);
    if (nrecvproc[i] == 1)
      printf("BORDERS RECV %d %d: proc %d rnum0 %d\n",
             i,me,recvproc[i][0],recvnum[i][0]);
    else if (nrecvproc[i] == 2)
      printf("BORDERS RECV %d %d: proc %d %d rnums %d %d\n",
             i,me,recvproc[i][0],recvproc[i][1],recvnum[i][0],recvnum[i][1]);
    else if (nrecvproc[i] == 3)
      printf("BORDERS RECV %d %d: proc %d %d %d rnums %d %d %d\n",
             i,me,recvproc[i][0],recvproc[i][1],recvproc[i][2],
             recvnum[i][0],recvnum[i][1],recvnum[i][2]);
    else if (nrecvproc[i] == 8)
      printf("BORDERS RECV %d %d: proc %d %d %d %d: %d %d %d %d "
             "rnums %d %d %d %d: %d %d %d %d\n",
             i,me,
             recvproc[i][0],recvproc[i][1],recvproc[i][2],recvproc[i][3],
             recvproc[i][4],recvproc[i][5],recvproc[i][6],recvproc[i][7],
             recvnum[i][0],recvnum[i][1],recvnum[i][2],recvnum[i][3],
             recvnum[i][4],recvnum[i][5],recvnum[i][6],recvnum[i][7]);
    else if (nrecvproc[i] >= 4)
      printf("BORDERS RECV %d %d: proc %d %d %d %d rnums %d %d %d %d\n",
             i,me,recvproc[i][0],recvproc[i][1],recvproc[i][2],recvproc[i][3],
             recvnum[i][0],recvnum[i][1],recvnum[i][2],recvnum[i][3]);
  }

  MPI_Barrier(world);
  /*

  // DEBUG

  // check that each proc owns exact atoms it should in its sub-box + ghosts
  // lo/hi = first/last owned atom in lattice

  /*
  int ilo,jlo,klo,ihi,jhi,khi;
  double ghostlo[3],ghosthi[3];

  ghostlo[0] = sublo[0] - cutghost[0];
  ghostlo[1] = sublo[1] - cutghost[1];
  ghosthi[0] = subhi[0] + cutghost[0];
  ghosthi[1] = subhi[1] + cutghost[1];
  if (domain->dimension == 3) {
    ghostlo[2] = sublo[2] - cutghost[2];
    ghosthi[2] = subhi[2] + cutghost[2];
  } else {
    ghostlo[2] = sublo[2];
    ghosthi[2] = subhi[2];
  }

  bounds(ghostlo[0],ghostlo[1],ghostlo[2],ghosthi[0],ghosthi[1],ghosthi[2],
         ilo,jlo,klo,ihi,jhi,khi);
  ncount = (ihi-ilo+1) * (jhi-jlo+1) * (khi-klo+1);
  ncount -= atom->nlocal;

  int nghost = atom->nghost;
  if (ncount != nghost) 
    printf("BORDERS proc %d: correct %d actual %d\n",me,ncount,nghost);
  int flag = 0;
  if (ncount != nghost) flag = 1;
  int flagall;
  MPI_Allreduce(&flag,&flagall,1,MPI_INT,MPI_SUM,world);
  if (flagall) error->all(FLERR,"Bad borders results");
  else if (me == 0) printf("BORDERS is correct\n");
  */
}

/* ----------------------------------------------------------------------
   forward communication invoked by a Pair
------------------------------------------------------------------------- */

void CommTiled::forward_comm_pair(Pair *pair)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = pair->comm_forward;

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++)
        MPI_Irecv(&buf_recv[nsize*forward_recv_offset[iswap][i]],
                  nsize*recvnum[iswap][i],
                  MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsendproc[iswap]; i++) {
        n = pair->pack_forward_comm(sendnum[iswap][i],sendlist[iswap][i],
                                    buf_send,pbc_flag[iswap][i],pbc[iswap][i]);
        MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      pair->pack_forward_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                              buf_send,pbc_flag[iswap][nsend],
                              pbc[iswap][nsend]);
      pair->unpack_forward_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                                buf_send);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        MPI_Waitany(nrecv,requests,&irecv,&status);
        pair->unpack_forward_comm(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                                  &buf_recv[nsize*
                                            forward_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   reverse communication invoked by a Pair
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::reverse_comm_pair(Pair *pair)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = MAX(pair->comm_reverse,pair->comm_reverse_off);

  for (int iswap = nswap-1; iswap >= 0; iswap--) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++)
        MPI_Irecv(&buf_recv[nsize*reverse_recv_offset[iswap][i]],
                  nsize*sendnum[iswap][i],MPI_DOUBLE,
                  sendproc[iswap][i],0,world,&requests[i]);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        n = pair->pack_reverse_comm(recvnum[iswap][i],firstrecv[iswap][i],
                                    buf_send);
        MPI_Send(buf_send,n,MPI_DOUBLE,recvproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      pair->pack_reverse_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                              buf_send);
      pair->unpack_reverse_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                                buf_send);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++) {
        MPI_Waitany(nsend,requests,&irecv,&status);
        pair->unpack_reverse_comm(sendnum[iswap][irecv],sendlist[iswap][irecv],
                                  &buf_recv[nsize*
                                            reverse_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   forward communication invoked by a Fix
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::forward_comm_fix(Fix *fix)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = fix->comm_forward;

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++)
        MPI_Irecv(&buf_recv[nsize*forward_recv_offset[iswap][i]],
                  nsize*recvnum[iswap][i],
                  MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsendproc[iswap]; i++) {
        n = fix->pack_forward_comm(sendnum[iswap][i],sendlist[iswap][i],
                                   buf_send,pbc_flag[iswap][i],pbc[iswap][i]);
        MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      fix->pack_forward_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                             buf_send,pbc_flag[iswap][nsend],
                             pbc[iswap][nsend]);
      fix->unpack_forward_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                               buf_send);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        MPI_Waitany(nrecv,requests,&irecv,&status);
        fix->unpack_forward_comm(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                                 &buf_recv[nsize*
                                           forward_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   reverse communication invoked by a Fix
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::reverse_comm_fix(Fix *fix)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = fix->comm_reverse;

  for (int iswap = nswap-1; iswap >= 0; iswap--) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++)
        MPI_Irecv(&buf_recv[nsize*reverse_recv_offset[iswap][i]],
                  nsize*sendnum[iswap][i],MPI_DOUBLE,
                  sendproc[iswap][i],0,world,&requests[i]);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        n = fix->pack_reverse_comm(recvnum[iswap][i],firstrecv[iswap][i],
                                   buf_send);
        MPI_Send(buf_send,n,MPI_DOUBLE,recvproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      fix->pack_reverse_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                             buf_send);
      fix->unpack_reverse_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                               buf_send);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++) {
        MPI_Waitany(nsend,requests,&irecv,&status);
        fix->unpack_reverse_comm(sendnum[iswap][irecv],sendlist[iswap][irecv],
                                 &buf_recv[nsize*
                                           reverse_recv_offset[iswap][irecv]]);
      }
    }
  }
}

// NOTE: these two forward/reverse methods still need to be updated

/* ----------------------------------------------------------------------
   forward communication invoked by a Fix
   n = total datums for all atoms, allows for variable number/atom
   NOTE: complicated b/c don't know # to recv a priori
------------------------------------------------------------------------- */

void CommTiled::forward_comm_variable_fix(Fix *fix)
{
}

/* ----------------------------------------------------------------------
   reverse communication invoked by a Fix
   n = total datums for all atoms, allows for variable number/atom
------------------------------------------------------------------------- */

void CommTiled::reverse_comm_variable_fix(Fix *fix)
{
}

/* ----------------------------------------------------------------------
   forward communication invoked by a Compute
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::forward_comm_compute(Compute *compute)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = compute->comm_forward;

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++)
        MPI_Irecv(&buf_recv[nsize*forward_recv_offset[iswap][i]],
                  nsize*recvnum[iswap][i],
                  MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsendproc[iswap]; i++) {
        n = compute->pack_forward_comm(sendnum[iswap][i],sendlist[iswap][i],
                                       buf_send,pbc_flag[iswap][i],
                                       pbc[iswap][i]);
        MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      compute->pack_forward_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                                 buf_send,pbc_flag[iswap][nsend],
                                 pbc[iswap][nsend]);
      compute->unpack_forward_comm(recvnum[iswap][nrecv],
                                   firstrecv[iswap][nrecv],buf_send);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        MPI_Waitany(nrecv,requests,&irecv,&status);
        compute->
          unpack_forward_comm(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                              &buf_recv[nsize*
                                        forward_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   reverse communication invoked by a Compute
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::reverse_comm_compute(Compute *compute)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = compute->comm_reverse;

  for (int iswap = nswap-1; iswap >= 0; iswap--) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++)
        MPI_Irecv(&buf_recv[nsize*reverse_recv_offset[iswap][i]],
                  nsize*sendnum[iswap][i],MPI_DOUBLE,
                  sendproc[iswap][i],0,world,&requests[i]);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        n = compute->pack_reverse_comm(recvnum[iswap][i],firstrecv[iswap][i],
                                       buf_send);
        MPI_Send(buf_send,n,MPI_DOUBLE,recvproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      compute->pack_reverse_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                                 buf_send);
      compute->unpack_reverse_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                                   buf_send);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++) {
        MPI_Waitany(nsend,requests,&irecv,&status);
        compute->
          unpack_reverse_comm(sendnum[iswap][irecv],sendlist[iswap][irecv],
                              &buf_recv[nsize*
                                        reverse_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   forward communication invoked by a Dump
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::forward_comm_dump(Dump *dump)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = dump->comm_forward;

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++)
        MPI_Irecv(&buf_recv[nsize*forward_recv_offset[iswap][i]],
                  nsize*recvnum[iswap][i],
                  MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsendproc[iswap]; i++) {
        n = dump->pack_forward_comm(sendnum[iswap][i],sendlist[iswap][i],
                                    buf_send,pbc_flag[iswap][i],
                                    pbc[iswap][i]);
        MPI_Send(buf_send,n,MPI_DOUBLE,sendproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      dump->pack_forward_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                              buf_send,pbc_flag[iswap][nsend],
                              pbc[iswap][nsend]);
      dump->unpack_forward_comm(recvnum[iswap][nrecv],
                                firstrecv[iswap][nrecv],buf_send);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        MPI_Waitany(nrecv,requests,&irecv,&status);
        dump->unpack_forward_comm(recvnum[iswap][irecv],firstrecv[iswap][irecv],
                                  &buf_recv[nsize*
                                            forward_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   reverse communication invoked by a Dump
   n = constant number of datums per atom
------------------------------------------------------------------------- */

void CommTiled::reverse_comm_dump(Dump *dump)
{
  int i,irecv,n,nsend,nrecv;
  MPI_Status status;

  int nsize = dump->comm_reverse;

  for (int iswap = nswap-1; iswap >= 0; iswap--) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++)
        MPI_Irecv(&buf_recv[nsize*reverse_recv_offset[iswap][i]],
                  nsize*sendnum[iswap][i],MPI_DOUBLE,
                  sendproc[iswap][i],0,world,&requests[i]);
    }
    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        n = dump->pack_reverse_comm(recvnum[iswap][i],firstrecv[iswap][i],
                                    buf_send);
        MPI_Send(buf_send,n,MPI_DOUBLE,recvproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      dump->pack_reverse_comm(recvnum[iswap][nrecv],firstrecv[iswap][nrecv],
                              buf_send);
      dump->unpack_reverse_comm(sendnum[iswap][nsend],sendlist[iswap][nsend],
                                buf_send);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsend; i++) {
        MPI_Waitany(nsend,requests,&irecv,&status);
        dump->unpack_reverse_comm(sendnum[iswap][irecv],sendlist[iswap][irecv],
                                  &buf_recv[nsize*
                                            reverse_recv_offset[iswap][irecv]]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   forward communication of Nsize values in per-atom array
------------------------------------------------------------------------- */

void CommTiled::forward_comm_array(int nsize, double **array)
{ 
  int i,j,k,m,n,iatom,last,irecv,nsend,nrecv;
  MPI_Status status;

  // NOTE: should check that buf_send and buf_recv are big enough

  for (int iswap = 0; iswap < nswap; iswap++) {
    nsend = nsendproc[iswap] - sendself[iswap];
    nrecv = nrecvproc[iswap] - sendself[iswap];

    MPI_Barrier(world);

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++)
        MPI_Irecv(&buf_recv[nsize*forward_recv_offset[iswap][i]],
                  nsize*recvnum[iswap][i],
                  MPI_DOUBLE,recvproc[iswap][i],0,world,&requests[i]);
    }
    if (sendother[iswap]) {
      for (i = 0; i < nsendproc[iswap]; i++) {
        m = 0;
        for (iatom = 0; iatom < sendnum[iswap][i]; iatom++) {
          j = sendlist[iswap][i][iatom];
          for (k = 0; k < nsize; k++)
            buf_send[m++] = array[j][k];
        }
        MPI_Send(buf_send,nsize*sendnum[iswap][i],
                 MPI_DOUBLE,sendproc[iswap][i],0,world);
      }
    }
    if (sendself[iswap]) {
      m = 0;
      for (iatom = 0; iatom < sendnum[iswap][nsend]; iatom++) {
        j = sendlist[iswap][nsend][iatom];
        for (k = 0; k < nsize; k++)
          buf_send[m++] = array[j][k];
      }
      m = 0;
      last = firstrecv[iswap][nrecv] + recvnum[iswap][nrecv];
      for (iatom = firstrecv[iswap][nrecv]; iatom < last; iatom++)
        for (k = 0; k < nsize; k++)
          array[iatom][k] = buf_send[m++];
    }

    if (recvother[iswap]) {
      for (i = 0; i < nrecv; i++) {
        MPI_Waitany(nrecv,requests,&irecv,&status);
        m = nsize*forward_recv_offset[iswap][irecv];
        last = firstrecv[iswap][irecv] + recvnum[iswap][irecv];
        for (iatom = firstrecv[iswap][irecv]; iatom < last; iatom++)
          for (k = 0; k < nsize; k++)
            array[iatom][k] = buf_recv[m++];
      }
    }
  }
}

// NOTE: this one is not used ???

/* ----------------------------------------------------------------------
   exchange info provided with all 6 stencil neighbors
------------------------------------------------------------------------- */

int CommTiled::exchange_variable(int n, double *inbuf, double *&outbuf)
{
  int nrecv = n;
  return nrecv;
}

/* ----------------------------------------------------------------------
   determine overlap list of Noverlap procs the lo/hi box overlaps
   overlap = non-zero area in common between box and proc sub-domain
   box is owned by me and extends in dim
------------------------------------------------------------------------- */

void CommTiled::box_drop_brick(int idim, double *lo, double *hi, int &indexme)
{
  // NOTE: this is not triclinic compatible
  // NOTE: there error messages are internal - should not occur

  int index,dir;
  if (hi[idim] == sublo[idim]) {
    index = myloc[idim] - 1;
    dir = -1;
  } else if (lo[idim] == subhi[idim]) {
    index = myloc[idim] + 1;
    dir = 1;
  } else if (hi[idim] == boxhi[idim]) {
    index = procgrid[idim] - 1;
    dir = -1;
  } else if (lo[idim] == boxlo[idim]) {
    index = 0;
    dir = 1;
  } else error->one(FLERR,"Comm tiled: no match in box drop brick");

  int other1,other2,proc;
  double lower,upper;
  double *split;

  if (idim == 0) {
    other1 = myloc[1]; other2 = myloc[2];
    split = xsplit;
  } else if (idim == 1) {
    other1 = myloc[0]; other2 = myloc[2];
    split = ysplit;
  } else {
    other1 = myloc[0]; other2 = myloc[1];
    split = zsplit;
  }

  if (index < 0 || index > procgrid[idim]) 
    error->one(FLERR,"Comm tiled: index error in box drop brick");

  while (1) {
    lower = boxlo[idim] + prd[idim]*split[index];
    if (index < procgrid[idim]-1) 
      upper = boxlo[idim] + prd[idim]*split[index+1];
    else upper = boxhi[idim];
    if (lower >= hi[idim] || upper <= lo[idim]) break;

    if (idim == 0) proc = grid2proc[index][other1][other2];
    else if (idim == 1) proc = grid2proc[other1][index][other2];
    else proc = grid2proc[other1][other2][index];

    if (noverlap == maxoverlap) {
      maxoverlap += DELTA_PROCS;
      memory->grow(overlap,maxoverlap,"comm:overlap");
    }

    if (proc == me) indexme = noverlap;
    overlap[noverlap++] = proc;
    index += dir;
    if (index < 0 || index >= procgrid[idim]) break;
  }
}

/* ----------------------------------------------------------------------
   determine overlap list of Noverlap procs the lo/hi box overlaps
   overlap = non-zero area in common between box and proc sub-domain
   recursive method for traversing an RCB tree of cuts
   no need to split lo/hi box as recurse b/c OK if box extends outside RCB box
------------------------------------------------------------------------- */

void CommTiled::box_drop_tiled(int idim, double *lo, double *hi, int &indexme)
{
  box_drop_tiled_recurse(lo,hi,0,nprocs-1,indexme);
}

void CommTiled::box_drop_tiled_recurse(double *lo, double *hi, 
                                       int proclower, int procupper,
                                       int &indexme)
{
  // end recursion when partition is a single proc
  // add proc to overlap list

  if (proclower == procupper) {
    if (noverlap == maxoverlap) {
      maxoverlap += DELTA_PROCS;
      memory->grow(overlap,maxoverlap,"comm:overlap");
    }

    if (proclower == me) indexme = noverlap;
    overlap[noverlap++] = proclower;
    return;
  }

  // drop box on each side of cut it extends beyond
  // use > and < criteria so does not include a box it only touches
  // procmid = 1st processor in upper half of partition
  //         = location in tree that stores this cut
  // dim = 0,1,2 dimension of cut
  // cut = position of cut

  int procmid = proclower + (procupper - proclower) / 2 + 1;
  int idim = rcbinfo[procmid].dim;
  double cut = boxlo[idim] + prd[idim]*rcbinfo[procmid].cutfrac;
  
  if (lo[idim] < cut) 
    box_drop_tiled_recurse(lo,hi,proclower,procmid-1,indexme);
  if (hi[idim] > cut)
    box_drop_tiled_recurse(lo,hi,procmid,procupper,indexme);
}

/* ----------------------------------------------------------------------
   return other box owned by proc as lo/hi corner pts
------------------------------------------------------------------------- */

void CommTiled::box_other_brick(int idim, int idir,
                                int proc, double *lo, double *hi)
{
  lo[0] = sublo[0]; lo[1] = sublo[1]; lo[2] = sublo[2]; 
  hi[0] = subhi[0]; hi[1] = subhi[1]; hi[2] = subhi[2]; 

  int other1,other2,oproc;
  double *split;

  if (idim == 0) {
    other1 = myloc[1]; other2 = myloc[2];
    split = xsplit;
  } else if (idim == 1) {
    other1 = myloc[0]; other2 = myloc[2];
    split = ysplit;
  } else {
    other1 = myloc[0]; other2 = myloc[1];
    split = zsplit;
  }

  int dir = -1;
  if (idir) dir = 1;
  int index = myloc[idim];
  int n = procgrid[idim];

  for (int i = 0; i < n; i++) {
    index += dir;
    if (index < 0) index = n-1;
    else if (index >= n) index = 0;

    if (idim == 0) oproc = grid2proc[index][other1][other2];
    else if (idim == 1) oproc = grid2proc[other1][index][other2];
    else oproc = grid2proc[other1][other2][index];

    if (proc == oproc) {
      lo[idim] = boxlo[idim] + prd[idim]*split[index];
      if (split[index+1] < 1.0) 
        hi[idim] = boxlo[idim] + prd[idim]*split[index+1];
      else hi[idim] = boxhi[idim];
      return;
    }
  }
}

/* ----------------------------------------------------------------------
   return other box owned by proc as lo/hi corner pts
------------------------------------------------------------------------- */

void CommTiled::box_other_tiled(int idim, int idir,
                                int proc, double *lo, double *hi)
{
  double (*split)[2] = rcbinfo[proc].mysplit;

  lo[0] = boxlo[0] + prd[0]*split[0][0];
  if (split[0][1] < 1.0) hi[0] = boxlo[0] + prd[0]*split[0][1];
  else hi[0] = boxhi[0];

  lo[1] = boxlo[1] + prd[1]*split[1][0];
  if (split[1][1] < 1.0) hi[1] = boxlo[1] + prd[1]*split[1][1];
  else hi[1] = boxhi[1];

  lo[2] = boxlo[2] + prd[2]*split[2][0];
  if (split[2][1] < 1.0) hi[2] = boxlo[2] + prd[2]*split[2][1];
  else hi[2] = boxhi[2];
}

/* ----------------------------------------------------------------------
   return 1 if proc's box touches me, else 0
   procneigh stores 6 procs that touch me
------------------------------------------------------------------------- */

int CommTiled::box_touch_brick(int proc, int idim, int idir)
{
  if (procneigh[idim][idir] == proc) return 1;
  return 0;
}

/* ----------------------------------------------------------------------
   return 1 if proc's box touches me, else 0
------------------------------------------------------------------------- */

int CommTiled::box_touch_tiled(int proc, int idim, int idir)
{
  // sending to left
  // only touches if proc hi = my lo, or if proc hi = boxhi and my lo = boxlo

  if (idir == 0) {
    if (rcbinfo[proc].mysplit[idim][1] == rcbinfo[me].mysplit[idim][0])
      return 1;
    else if (rcbinfo[proc].mysplit[idim][1] == 1.0 && 
             rcbinfo[me].mysplit[idim][0] == 0.0)
      return 1;

  // sending to right
  // only touches if proc lo = my hi, or if proc lo = boxlo and my hi = boxhi

  } else {
    if (rcbinfo[proc].mysplit[idim][0] == rcbinfo[me].mysplit[idim][1])
      return 1;
    else if (rcbinfo[proc].mysplit[idim][0] == 0.0 && 
             rcbinfo[me].mysplit[idim][1] == 1.0)
      return 1;
  }

  return 0;
}

/* ----------------------------------------------------------------------
------------------------------------------------------------------------- */

int CommTiled::point_drop_brick(int idim, double *x)
{
  if (closer_subbox_edge(idim,x)) return procneigh[idim][1];
  return procneigh[idim][0];
}

/* ----------------------------------------------------------------------
   determine overlap list of Noverlap procs the lo/hi box overlaps
   overlap = non-zero area in common between box and proc sub-domain
   recursive method for traversing an RCB tree of cuts
   no need to split lo/hi box as recurse b/c OK if box extends outside RCB box
------------------------------------------------------------------------- */

int CommTiled::point_drop_tiled(int idim, double *x)
{
  double xnew[3];
  xnew[0] = x[0]; xnew[1] = x[1]; xnew[2] = x[2];

  if (idim == 0) {
    if (xnew[1] < sublo[1] || xnew[1] > subhi[1]) {
      if (closer_subbox_edge(1,x)) xnew[1] = subhi[1];
      else xnew[1] = sublo[1];
    }
  }
  if (idim <= 1) {
    if (xnew[2] < sublo[2] || xnew[2] > subhi[2]) {
      if (closer_subbox_edge(2,x)) xnew[2] = subhi[2];
      else xnew[2] = sublo[2];
    }
  }

  int proc = point_drop_tiled_recurse(xnew,0,nprocs-1);
  if (proc == me) return me;

  if (idim == 0) {
    int done = 1;
    if (rcbinfo[proc].mysplit[1][0] == rcbinfo[me].mysplit[1][1]) {
      xnew[1] -= EPSILON * (subhi[1]-sublo[1]);
      done = 0;
    }
    if (rcbinfo[proc].mysplit[2][0] == rcbinfo[me].mysplit[2][1]) {
      xnew[2] -= EPSILON * (subhi[2]-sublo[2]);
      done = 0;
    }
    if (!done) {
      proc = point_drop_tiled_recurse(xnew,0,nprocs-1);
      done = 1;
      if (rcbinfo[proc].mysplit[1][0] == rcbinfo[me].mysplit[1][1]) {
        xnew[1] -= EPSILON * (subhi[1]-sublo[1]);
        done = 0;
      }
      if (rcbinfo[proc].mysplit[2][0] == rcbinfo[me].mysplit[2][1]) {
        xnew[2] -= EPSILON * (subhi[2]-sublo[2]);
        done = 0;
      }
      if (!done) proc = point_drop_tiled_recurse(xnew,0,nprocs-1);
    }
  } else if (idim == 1) {
    int done = 1;
    if (rcbinfo[proc].mysplit[2][0] == rcbinfo[me].mysplit[2][1]) {
      xnew[2] -= EPSILON * (subhi[2]-sublo[2]);
      proc = point_drop_tiled_recurse(xnew,0,nprocs-1);
    }
  }

  return proc;
}

/* ----------------------------------------------------------------------
   recursive form
------------------------------------------------------------------------- */

int CommTiled::point_drop_tiled_recurse(double *x, 
                                        int proclower, int procupper)
{
  // end recursion when partition is a single proc
  // return proc

  if (proclower == procupper) return proclower;
  
  // drop point on side of cut it is on
  // use < criterion so point is not on high edge of proc sub-domain
  // procmid = 1st processor in upper half of partition
  //         = location in tree that stores this cut
  // dim = 0,1,2 dimension of cut
  // cut = position of cut

  int procmid = proclower + (procupper - proclower) / 2 + 1;
  int idim = rcbinfo[procmid].dim;
  double cut = boxlo[idim] + prd[idim]*rcbinfo[procmid].cutfrac;

  if (x[idim] < cut) return point_drop_tiled_recurse(x,proclower,procmid-1);
  else return point_drop_tiled_recurse(x,procmid,procupper);
}

/* ----------------------------------------------------------------------
   assume x[idim] is outside subbox bounds in same dim
------------------------------------------------------------------------- */

int CommTiled::closer_subbox_edge(int idim, double *x)
{
  double deltalo,deltahi;

  if (sublo[idim] == boxlo[idim])
    deltalo = fabs(x[idim]-prd[idim] - sublo[idim]);
  else deltalo = fabs(x[idim] - sublo[idim]);

  if (subhi[idim] == boxhi[idim])
    deltahi = fabs(x[idim]+prd[idim] - subhi[idim]);
  else deltahi = fabs(x[idim] - subhi[idim]);

  if (deltalo < deltahi) return 0;
  return 1;
}

/* ----------------------------------------------------------------------
   realloc the size of the send buffer as needed with BUFFACTOR and bufextra
   if flag = 1, realloc
   if flag = 0, don't need to realloc with copy, just free/malloc
------------------------------------------------------------------------- */

void CommTiled::grow_send(int n, int flag)
{
  maxsend = static_cast<int> (BUFFACTOR * n);
  if (flag)
    memory->grow(buf_send,maxsend+bufextra,"comm:buf_send");
  else {
    memory->destroy(buf_send);
    memory->create(buf_send,maxsend+bufextra,"comm:buf_send");
  }
}

/* ----------------------------------------------------------------------
   free/malloc the size of the recv buffer as needed with BUFFACTOR
------------------------------------------------------------------------- */

void CommTiled::grow_recv(int n)
{
  maxrecv = static_cast<int> (BUFFACTOR * n);
  memory->destroy(buf_recv);
  memory->create(buf_recv,maxrecv,"comm:buf_recv");
}

/* ----------------------------------------------------------------------
   realloc the size of the iswap sendlist as needed with BUFFACTOR
------------------------------------------------------------------------- */

void CommTiled::grow_list(int iswap, int iwhich, int n)
{
  maxsendlist[iswap][iwhich] = static_cast<int> (BUFFACTOR * n);
  memory->grow(sendlist[iswap][iwhich],maxsendlist[iswap][iwhich],
               "comm:sendlist[i][j]");
}

/* ----------------------------------------------------------------------
   allocation of swap info
------------------------------------------------------------------------- */

void CommTiled::allocate_swap(int n)
{
  nsendproc = new int[n];
  nrecvproc = new int[n];
  sendother = new int[n];
  recvother = new int[n];
  sendself = new int[n];
  nprocmax = new int[n];

  sendproc = new int*[n];
  recvproc = new int*[n];
  sendnum = new int*[n];
  recvnum = new int*[n];
  size_forward_recv = new int*[n];
  firstrecv = new int*[n];
  size_reverse_send = new int*[n];
  size_reverse_recv = new int*[n];
  forward_recv_offset = new int*[n];
  reverse_recv_offset = new int*[n];

  pbc_flag = new int*[n];
  pbc = new int**[n];
  sendbox = new double**[n];
  maxsendlist = new int*[n];
  sendlist = new int**[n];

  for (int i = 0; i < n; i++) {
    sendproc[i] = recvproc[i] = NULL;
    sendnum[i] = recvnum[i] = NULL;
    size_forward_recv[i] = firstrecv[i] = NULL;
    size_reverse_send[i] = size_reverse_recv[i] = NULL;
    forward_recv_offset[i] = reverse_recv_offset[i] = NULL;

    pbc_flag[i] = NULL;
    pbc[i] = NULL;
    sendbox[i] = NULL;
    maxsendlist[i] = NULL;
    sendlist[i] = NULL;
  }

  maxreqstat = 0;
  requests = NULL;
  statuses = NULL;

  for (int i = 0; i < n; i++) {
    nprocmax[i] = DELTA_PROCS;
    grow_swap_send(i,DELTA_PROCS,0);
    grow_swap_recv(i,DELTA_PROCS);
  }

  nexchproc = new int[n/2];
  nexchprocmax = new int[n/2];
  exchproc = new int*[n/2];
  exchnum = new int*[n/2];

  for (int i = 0; i < n/2; i++) {
    nexchprocmax[i] = DELTA_PROCS;
    exchproc[i] = new int[DELTA_PROCS];
    exchnum[i] = new int[DELTA_PROCS];
  }
}

/* ----------------------------------------------------------------------
   grow info for swap I, to allow for N procs to communicate with
   ditto for complementary recv for swap I+1 or I-1, as invoked by caller
------------------------------------------------------------------------- */

void CommTiled::grow_swap_send(int i, int n, int nold)
{
  delete [] sendproc[i];
  sendproc[i] = new int[n];
  delete [] sendnum[i];
  sendnum[i] = new int[n];

  delete [] size_reverse_recv[i];
  size_reverse_recv[i] = new int[n];
  delete [] reverse_recv_offset[i];
  reverse_recv_offset[i] = new int[n];

  delete [] pbc_flag[i];
  pbc_flag[i] = new int[n];
  memory->destroy(pbc[i]);
  memory->create(pbc[i],n,6,"comm:pbc_flag");
  memory->destroy(sendbox[i]);
  memory->create(sendbox[i],n,6,"comm:sendbox");

  delete [] maxsendlist[i];
  maxsendlist[i] = new int[n];

  for (int j = 0; j < nold; j++) memory->destroy(sendlist[i][j]);
  delete [] sendlist[i];
  sendlist[i] = new int*[n];
  for (int j = 0; j < n; j++) {
    maxsendlist[i][j] = BUFMIN;
    memory->create(sendlist[i][j],BUFMIN,"comm:sendlist[i][j]");
  }
}

void CommTiled::grow_swap_recv(int i, int n)
{
  delete [] recvproc[i];
  recvproc[i] = new int[n];
  delete [] recvnum[i];
  recvnum[i] = new int[n];

  delete [] size_forward_recv[i];
  size_forward_recv[i] = new int[n];
  delete [] firstrecv[i];
  firstrecv[i] = new int[n];
  delete [] forward_recv_offset[i];
  forward_recv_offset[i] = new int[n];

  delete [] size_reverse_send[i];
  size_reverse_send[i] = new int[n];
}

/* ----------------------------------------------------------------------
   deallocate swap info
------------------------------------------------------------------------- */

void CommTiled::deallocate_swap(int n)
{
  delete [] nsendproc;
  delete [] nrecvproc;
  delete [] sendother;
  delete [] recvother;
  delete [] sendself;

  for (int i = 0; i < n; i++) {
    delete [] sendproc[i];
    delete [] recvproc[i];
    delete [] sendnum[i];
    delete [] recvnum[i];
    delete [] size_forward_recv[i];
    delete [] firstrecv[i];
    delete [] size_reverse_send[i];
    delete [] size_reverse_recv[i];
    delete [] forward_recv_offset[i];
    delete [] reverse_recv_offset[i];

    delete [] pbc_flag[i];
    memory->destroy(pbc[i]);
    memory->destroy(sendbox[i]);
    delete [] maxsendlist[i];

    for (int j = 0; j < nprocmax[i]; j++) memory->destroy(sendlist[i][j]);
    delete [] sendlist[i];
  }

  delete [] sendproc;
  delete [] recvproc;
  delete [] sendnum;
  delete [] recvnum;
  delete [] size_forward_recv;
  delete [] firstrecv;
  delete [] size_reverse_send;
  delete [] size_reverse_recv;
  delete [] forward_recv_offset;
  delete [] reverse_recv_offset;

  delete [] pbc_flag;
  delete [] pbc;
  delete [] sendbox;
  delete [] maxsendlist;
  delete [] sendlist;

  delete [] requests;
  delete [] statuses;

  delete [] nprocmax;

  delete [] nexchproc;
  delete [] nexchprocmax;

  for (int i = 0; i < n/2; i++) {
    delete [] exchproc[i];
    delete [] exchnum[i];
  }

  delete [] exchproc;
  delete [] exchnum;
}

/* ----------------------------------------------------------------------
   return # of bytes of allocated memory
------------------------------------------------------------------------- */

bigint CommTiled::memory_usage()
{
  bigint bytes = 0;
  return bytes;
}
