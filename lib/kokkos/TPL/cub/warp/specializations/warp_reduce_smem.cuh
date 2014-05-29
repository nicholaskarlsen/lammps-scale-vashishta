/******************************************************************************
 * Copyright (c) 2011, Duane Merrill.  All rights reserved.
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the NVIDIA CORPORATION nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

/**
 * \file
 * cub::WarpReduceSmem provides smem-based variants of parallel reduction across CUDA warps.
 */

#pragma once

#include "../../thread/thread_operators.cuh"
#include "../../thread/thread_load.cuh"
#include "../../thread/thread_store.cuh"
#include "../../util_type.cuh"
#include "../../util_namespace.cuh"

/// Optional outer namespace(s)
CUB_NS_PREFIX

/// CUB namespace
namespace cub {

/**
 * \brief WarpReduceSmem provides smem-based variants of parallel reduction across CUDA warps.
 */
template <
    typename    T,                      ///< Data type being reduced
    int         LOGICAL_WARPS,          ///< Number of logical warps entrant
    int         LOGICAL_WARP_THREADS>   ///< Number of threads per logical warp
struct WarpReduceSmem
{
    /******************************************************************************
     * Constants and typedefs
     ******************************************************************************/

    enum
    {
        /// Whether the logical warp size is a power-of-two
        POW_OF_TWO = ((LOGICAL_WARP_THREADS & (LOGICAL_WARP_THREADS - 1)) == 0),

        /// The number of warp scan steps
        STEPS = Log2<LOGICAL_WARP_THREADS>::VALUE,

        /// The number of threads in half a warp
        HALF_WARP_THREADS = 1 << (STEPS - 1),

        /// The number of shared memory elements per warp
        WARP_SMEM_ELEMENTS =  LOGICAL_WARP_THREADS + HALF_WARP_THREADS,
    };

    /// Shared memory flag type
    typedef unsigned char SmemFlag;

    /// Shared memory storage layout type (1.5 warps-worth of elements for each warp)
    typedef T _TempStorage[LOGICAL_WARPS][WARP_SMEM_ELEMENTS];

    // Alias wrapper allowing storage to be unioned
    struct TempStorage : Uninitialized<_TempStorage> {};


    /******************************************************************************
     * Thread fields
     ******************************************************************************/

    _TempStorage     &temp_storage;
    int             warp_id;
    int             lane_id;


    /******************************************************************************
     * Construction
     ******************************************************************************/

    /// Constructor
    __device__ __forceinline__ WarpReduceSmem(
        TempStorage     &temp_storage,
        int             warp_id,
        int             lane_id)
    :
        temp_storage(temp_storage.Alias()),
        warp_id(warp_id),
        lane_id(lane_id)
    {}


    /******************************************************************************
     * Operation
     ******************************************************************************/

    /**
     * Reduction
     */
    template <
        bool                FULL_WARPS,             ///< Whether all lanes in each warp are contributing a valid fold of items
        int                 FOLDED_ITEMS_PER_LANE,  ///< Number of items folded into each lane
        typename            ReductionOp>
    __device__ __forceinline__ T Reduce(
        T                   input,                  ///< [in] Calling thread's input
        int                 folded_items_per_warp,  ///< [in] Total number of valid items folded into each logical warp
        ReductionOp         reduction_op)           ///< [in] Reduction operator
    {
        for (int STEP = 0; STEP < STEPS; STEP++)
        {
            const int OFFSET = 1 << STEP;

            // Share input through buffer
            ThreadStore<STORE_VOLATILE>(&temp_storage[warp_id][lane_id], input);

            // Update input if peer_addend is in range
            if ((FULL_WARPS && POW_OF_TWO) || ((lane_id + OFFSET) * FOLDED_ITEMS_PER_LANE < folded_items_per_warp))
            {
                T peer_addend = ThreadLoad<LOAD_VOLATILE>(&temp_storage[warp_id][lane_id + OFFSET]);
                input = reduction_op(input, peer_addend);
            }
        }

        return input;
    }


    /**
     * Segmented reduction
     */
    template <
        bool            HEAD_SEGMENTED,     ///< Whether flags indicate a segment-head or a segment-tail
        typename        Flag,
        typename        ReductionOp>
    __device__ __forceinline__ T SegmentedReduce(
        T               input,              ///< [in] Calling thread's input
        Flag            flag,               ///< [in] Whether or not the current lane is a segment head/tail
        ReductionOp     reduction_op)       ///< [in] Reduction operator
    {
    #if CUB_PTX_ARCH >= 200

        // Ballot-based segmented reduce

        // Get the start flags for each thread in the warp.
        int warp_flags = __ballot(flag);

        if (!HEAD_SEGMENTED)
            warp_flags <<= 1;

        // Keep bits above the current thread.
        warp_flags &= LaneMaskGt();

        // Accommodate packing of multiple logical warps in a single physical warp
        if ((LOGICAL_WARPS > 1) && (LOGICAL_WARP_THREADS < 32))
            warp_flags >>= (warp_id * LOGICAL_WARP_THREADS);

        // Find next flag
        int next_flag = __clz(__brev(warp_flags));

        // Clip the next segment at the warp boundary if necessary
        if (LOGICAL_WARP_THREADS != 32)
            next_flag = CUB_MIN(next_flag, LOGICAL_WARP_THREADS);

        for (int STEP = 0; STEP < STEPS; STEP++)
        {
            const int OFFSET = 1 << STEP;

            // Share input into buffer
            ThreadStore<STORE_VOLATILE>(&temp_storage[warp_id][lane_id], input);

            // Update input if peer_addend is in range
            if (OFFSET < next_flag - lane_id)
            {
                T peer_addend = ThreadLoad<LOAD_VOLATILE>(&temp_storage[warp_id][lane_id + OFFSET]);
                input = reduction_op(input, peer_addend);
            }
        }

        return input;

    #else

        // Smem-based segmented reduce

        enum
        {
            UNSET   = 0x0,  // Is initially unset
            SET     = 0x1,  // Is initially set
            SEEN    = 0x2,  // Has seen another head flag from a successor peer
        };

        // Alias flags onto shared data storage
        volatile SmemFlag *flag_storage = reinterpret_cast<SmemFlag*>(temp_storage[warp_id]);

        SmemFlag flag_status = (flag) ? SET : UNSET;

        for (int STEP = 0; STEP < STEPS; STEP++)
        {
            const int OFFSET = 1 << STEP;

            // Share input through buffer
            ThreadStore<STORE_VOLATILE>(&temp_storage[warp_id][lane_id], input);

            // Get peer from buffer
            T peer_addend = ThreadLoad<LOAD_VOLATILE>(&temp_storage[warp_id][lane_id + OFFSET]);

            // Share flag through buffer
            flag_storage[lane_id] = flag_status;

            // Get peer flag from buffer
            SmemFlag peer_flag_status = flag_storage[lane_id + OFFSET];

            // Update input if peer was in range
            if (lane_id < LOGICAL_WARP_THREADS - OFFSET)
            {
                if (HEAD_SEGMENTED)
                {
                    // Head-segmented
                    if ((flag_status & SEEN) == 0)
                    {
                        // Has not seen a more distant head flag
                        if (peer_flag_status & SET)
                        {
                            // Has now seen a head flag
                            flag_status |= SEEN;
                        }
                        else
                        {
                            // Peer is not a head flag: grab its count
                            input = reduction_op(input, peer_addend);
                        }

                        // Update seen status to include that of peer
                        flag_status |= (peer_flag_status & SEEN);
                    }
                }
                else
                {
                    // Tail-segmented.  Simply propagate flag status
                    if (!flag_status)
                    {
                        input = reduction_op(input, peer_addend);
                        flag_status |= peer_flag_status;
                    }

                }
            }
        }

        return input;

    #endif
    }


    /**
     * Summation
     */
    template <
        bool            FULL_WARPS,             ///< Whether all lanes in each warp are contributing a valid fold of items
        int             FOLDED_ITEMS_PER_LANE>  ///< Number of items folded into each lane
    __device__ __forceinline__ T Sum(
        T               input,                  ///< [in] Calling thread's input
        int             folded_items_per_warp)  ///< [in] Total number of valid items folded into each logical warp
    {
        return Reduce<FULL_WARPS, FOLDED_ITEMS_PER_LANE>(input, folded_items_per_warp, cub::Sum());
    }

};


}               // CUB namespace
CUB_NS_POSTFIX  // Optional outer namespace(s)
