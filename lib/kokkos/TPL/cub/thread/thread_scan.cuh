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
 * Thread utilities for sequential prefix scan over statically-sized array types
 */

#pragma once

#include "../thread/thread_operators.cuh"
#include "../util_namespace.cuh"

/// Optional outer namespace(s)
CUB_NS_PREFIX

/// CUB namespace
namespace cub {

/**
 * \addtogroup ThreadModule
 * @{
 */

/**
 * \name Sequential prefix scan over statically-sized array types
 * @{
 */

/**
 * \brief Perform a sequential exclusive prefix scan over \p LENGTH elements of the \p input array, seeded with the specified \p prefix.  The aggregate is returned.
 *
 * \tparam LENGTH     Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanExclusive(
    T           *input,                 ///< [in] Input array
    T           *output,                ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op,                ///< [in] Binary scan operator
    T           prefix,                 ///< [in] Prefix to seed scan with
    bool        apply_prefix = true)    ///< [in] Whether or not the calling thread should apply its prefix.  If not, the first output element is undefined.  (Handy for preventing thread-0 from applying a prefix.)
{
    T inclusive = input[0];
    if (apply_prefix)
    {
        inclusive = scan_op(prefix, inclusive);
    }
    output[0] = prefix;
    T exclusive = inclusive;

    #pragma unroll
    for (int i = 1; i < LENGTH; ++i)
    {
        inclusive = scan_op(exclusive, input[i]);
        output[i] = exclusive;
        exclusive = inclusive;
    }

    return inclusive;
}


/**
 * \brief Perform a sequential exclusive prefix scan over the statically-sized \p input array, seeded with the specified \p prefix.  The aggregate is returned.
 *
 * \tparam LENGTH     <b>[inferred]</b> Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanExclusive(
    T           (&input)[LENGTH],       ///< [in] Input array
    T           (&output)[LENGTH],      ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op,                ///< [in] Binary scan operator
    T           prefix,                 ///< [in] Prefix to seed scan with
    bool        apply_prefix = true)    ///< [in] Whether or not the calling thread should apply its prefix.  (Handy for preventing thread-0 from applying a prefix.)
{
    return ThreadScanExclusive<LENGTH>((T*) input, (T*) output, scan_op, prefix);
}


/**
 * \brief Perform a sequential inclusive prefix scan over \p LENGTH elements of the \p input array.  The aggregate is returned.
 *
 * \tparam LENGTH     Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanInclusive(
    T           *input,                 ///< [in] Input array
    T           *output,                ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op)                ///< [in] Binary scan operator
{
    T inclusive = input[0];
    output[0] = inclusive;

    // Continue scan
    #pragma unroll
    for (int i = 0; i < LENGTH; ++i)
    {
        inclusive = scan_op(inclusive, input[i]);
        output[i] = inclusive;
    }

    return inclusive;
}


/**
 * \brief Perform a sequential inclusive prefix scan over the statically-sized \p input array.  The aggregate is returned.
 *
 * \tparam LENGTH     <b>[inferred]</b> Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanInclusive(
    T           (&input)[LENGTH],       ///< [in] Input array
    T           (&output)[LENGTH],      ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op)                ///< [in] Binary scan operator
{
    return ThreadScanInclusive<LENGTH>((T*) input, (T*) output, scan_op);
}


/**
 * \brief Perform a sequential inclusive prefix scan over \p LENGTH elements of the \p input array, seeded with the specified \p prefix.  The aggregate is returned.
 *
 * \tparam LENGTH     Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanInclusive(
    T           *input,                 ///< [in] Input array
    T           *output,                ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op,                ///< [in] Binary scan operator
    T           prefix,                 ///< [in] Prefix to seed scan with
    bool        apply_prefix = true)    ///< [in] Whether or not the calling thread should apply its prefix.  (Handy for preventing thread-0 from applying a prefix.)
{
    T inclusive = input[0];
    if (apply_prefix)
    {
        inclusive = scan_op(prefix, inclusive);
    }
    output[0] = inclusive;

    // Continue scan
    #pragma unroll
    for (int i = 1; i < LENGTH; ++i)
    {
        inclusive = scan_op(inclusive, input[i]);
        output[i] = inclusive;
    }

    return inclusive;
}


/**
 * \brief Perform a sequential inclusive prefix scan over the statically-sized \p input array, seeded with the specified \p prefix.  The aggregate is returned.
 *
 * \tparam LENGTH     <b>[inferred]</b> Length of \p input and \p output arrays
 * \tparam T          <b>[inferred]</b> The data type to be scanned.
 * \tparam ScanOp     <b>[inferred]</b> Binary scan operator type having member <tt>T operator()(const T &a, const T &b)</tt>
 */
template <
    int         LENGTH,
    typename    T,
    typename    ScanOp>
__device__ __forceinline__ T ThreadScanInclusive(
    T           (&input)[LENGTH],       ///< [in] Input array
    T           (&output)[LENGTH],      ///< [out] Output array (may be aliased to \p input)
    ScanOp      scan_op,                ///< [in] Binary scan operator
    T           prefix,                 ///< [in] Prefix to seed scan with
    bool        apply_prefix = true)    ///< [in] Whether or not the calling thread should apply its prefix.  (Handy for preventing thread-0 from applying a prefix.)
{
    return ThreadScanInclusive<LENGTH>((T*) input, (T*) output, scan_op, prefix, apply_prefix);
}


//@}  end member group

/** @} */       // end group ThreadModule


}               // CUB namespace
CUB_NS_POSTFIX  // Optional outer namespace(s)
