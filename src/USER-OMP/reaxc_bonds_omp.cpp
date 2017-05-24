/*----------------------------------------------------------------------
  PuReMD - Purdue ReaxFF Molecular Dynamics Program

  Copyright (2010) Purdue University
  Hasan Metin Aktulga, hmaktulga@lbl.gov
  Joseph Fogarty, jcfogart@mail.usf.edu
  Sagar Pandit, pandit@usf.edu
  Ananth Y Grama, ayg@cs.purdue.edu

  Please cite the related publication:
  H. M. Aktulga, J. C. Fogarty, S. A. Pandit, A. Y. Grama,
  "Parallel Reactive Molecular Dynamics: Numerical Methods and
  Algorithmic Techniques", Parallel Computing, in press.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details:
  <http://www.gnu.org/licenses/>.
  ----------------------------------------------------------------------*/

#include "pair_reaxc_omp.h"
#include  <omp.h>

#include "reaxc_bonds_omp.h"
#include "reaxc_bond_orders_omp.h"
#include "reaxc_list.h"
#include "reaxc_tool_box.h"
#include "reaxc_vector.h"

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

void BondsOMP( reax_system *system, control_params *control,
            simulation_data *data, storage *workspace, reax_list **lists,
            output_controls *out_control )
{
#ifdef OMP_TIMING
  double endTimeBase, startTimeBase;
  startTimeBase = MPI_Wtime();
#endif
  
  int natoms = system->n;
  int nthreads = control->nthreads;
  reax_list *bonds = (*lists) + BONDS;
  double gp3 = system->reax_param.gp.l[3];
  double gp4 = system->reax_param.gp.l[4];
  double gp7 = system->reax_param.gp.l[7];
  double gp10 = system->reax_param.gp.l[10];
  double gp37 = (int) system->reax_param.gp.l[37];
  double total_Ebond = 0.0;

#pragma omp parallel default(shared) reduction(+: total_Ebond)
 { 
  int  i, j, pj;
  int start_i, end_i;
  int type_i, type_j;
  double ebond, ebond_thr=0.0, pow_BOs_be2, exp_be12, CEbo;
  double gp3, gp4, gp7, gp10, gp37;
  double exphu, exphua1, exphub1, exphuov, hulpov, estriph, estriph_thr=0.0;
  double decobdbo, decobdboua, decobdboub;
  single_body_parameters *sbp_i, *sbp_j;
  two_body_parameters *twbp;
  bond_order_data *bo_ij;
  int  tid = omp_get_thread_num();
  long reductionOffset = (system->N * tid);
  
  class PairReaxCOMP *pair_reax_ptr;
  pair_reax_ptr = static_cast<class PairReaxCOMP*>(system->pair_ptr);
  class ThrData *thr = pair_reax_ptr->getFixOMP()->get_thr(tid);
  
  pair_reax_ptr->ev_setup_thr_proxy(system->pair_ptr->eflag_either, 
				    system->pair_ptr->vflag_either, natoms, 
				    system->pair_ptr->eatom, system->pair_ptr->vatom, thr);
  
#pragma omp for schedule(guided)
  for (i = 0; i < natoms; ++i) {
    start_i = Start_Index(i, bonds);
    end_i = End_Index(i, bonds);
    
    for (pj = start_i; pj < end_i; ++pj) {
      j = bonds->select.bond_list[pj].nbr;
      
      if( system->my_atoms[i].orig_id > system->my_atoms[j].orig_id ) continue;

      if( system->my_atoms[i].orig_id == system->my_atoms[j].orig_id ) {
        if (system->my_atoms[j].x[2] <  system->my_atoms[i].x[2]) continue;
      	if (system->my_atoms[j].x[2] == system->my_atoms[i].x[2] && 
      	    system->my_atoms[j].x[1] <  system->my_atoms[i].x[1]) continue;
        if (system->my_atoms[j].x[2] == system->my_atoms[i].x[2] && 
      	    system->my_atoms[j].x[1] == system->my_atoms[i].x[1] && 
      	    system->my_atoms[j].x[0] <  system->my_atoms[i].x[0]) continue;
      }

      /* set the pointers */
      type_i = system->my_atoms[i].type;
      type_j = system->my_atoms[j].type;
      sbp_i = &( system->reax_param.sbp[type_i] );
      sbp_j = &( system->reax_param.sbp[type_j] );
      twbp = &( system->reax_param.tbp[type_i][type_j] );
      bo_ij = &( bonds->select.bond_list[pj].bo_data );
      
      /* calculate the constants */
      pow_BOs_be2 = pow( bo_ij->BO_s, twbp->p_be2 );
      exp_be12 = exp( twbp->p_be1 * ( 1.0 - pow_BOs_be2 ) );
      CEbo = -twbp->De_s * exp_be12 *
	( 1.0 - twbp->p_be1 * twbp->p_be2 * pow_BOs_be2 );
      
      /* calculate the Bond Energy */      
      total_Ebond += ebond =
	-twbp->De_s * bo_ij->BO_s * exp_be12
	-twbp->De_p * bo_ij->BO_pi
	-twbp->De_pp * bo_ij->BO_pi2;
      
      /* tally into per-atom energy */
      if (system->pair_ptr->evflag)
	pair_reax_ptr->ev_tally_thr_proxy(system->pair_ptr, i, j, natoms, 1, 
					  ebond, 0.0, 0.0, 0.0, 0.0, 0.0, thr);
      
      /* calculate derivatives of Bond Orders */
      bo_ij->Cdbo += CEbo;
      bo_ij->Cdbopi -= (CEbo + twbp->De_p);
      bo_ij->Cdbopi2 -= (CEbo + twbp->De_pp);
      
      /* Stabilisation terminal triple bond */
      if (bo_ij->BO >= 1.00) {
	if (gp37 == 2 ||
	    (sbp_i->mass == 12.0000 && sbp_j->mass == 15.9990) ||
	    (sbp_j->mass == 12.0000 && sbp_i->mass == 15.9990)) {
	  exphu = exp( -gp7 * SQR(bo_ij->BO - 2.50) );
	  exphua1 = exp(-gp3 * (workspace->total_bond_order[i]-bo_ij->BO));
	  exphub1 = exp(-gp3 * (workspace->total_bond_order[j]-bo_ij->BO));
	  exphuov = exp(gp4 * (workspace->Delta[i] + workspace->Delta[j]));
	  hulpov = 1.0 / (1.0 + 25.0 * exphuov);
	  
	  estriph = gp10 * exphu * hulpov * (exphua1 + exphub1);
	  total_Ebond += estriph;
	  
	  decobdbo = gp10 * exphu * hulpov * (exphua1 + exphub1) *
	    ( gp3 - 2.0 * gp7 * (bo_ij->BO-2.50) );
	  decobdboua = -gp10 * exphu * hulpov *
	    (gp3*exphua1 + 25.0*gp4*exphuov*hulpov*(exphua1+exphub1));
	  decobdboub = -gp10 * exphu * hulpov *
	    (gp3*exphub1 + 25.0*gp4*exphuov*hulpov*(exphua1+exphub1));
	  
	  /* tally into per-atom energy */
	  if (system->pair_ptr->evflag)
	    pair_reax_ptr->ev_tally_thr_proxy(system->pair_ptr, i, j, natoms, 1, 
					      estriph, 0.0, 0.0, 0.0, 0.0, 0.0, thr);
	  
	  bo_ij->Cdbo += decobdbo;
	  workspace->CdDelta[i] += decobdboua;
	  workspace->CdDeltaReduction[reductionOffset+j] += decobdboub;
        }
      }
    }
  } // for(i)

 } // omp
 
 data->my_en.e_bond += total_Ebond;
 
#ifdef OMP_TIMING
 endTimeBase = MPI_Wtime();
 ompTimingData[COMPUTEBONDSINDEX] += (endTimeBase-startTimeBase); 
#endif

}
