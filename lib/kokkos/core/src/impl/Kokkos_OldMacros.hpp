/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Christian R. Trott (crtrott@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#ifndef KOKKOS_IMPL_OLD_MACROS_HPP
#define KOKKOS_IMPL_OLD_MACROS_HPP

#ifdef KOKKOS_ATOMICS_USE_CUDA
#ifndef KOKKOS_ENABLE_CUDA_ATOMICS
#define KOKKOS_ENABLE_CUDA_ATOMICS KOKKOS_ATOMICS_USE_CUDA
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_GCC
#ifndef KOKKOS_ENABLE_GNU_ATOMICS
#define KOKKOS_ENABLE_GNU_ATOMICS KOKKOS_ATOMICS_USE_GCC
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_GNU
#ifndef KOKKOS_ENABLE_GNU_ATOMICS
#define KOKKOS_ENABLE_GNU_ATOMICS KOKKOS_ATOMICS_USE_GNU
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_INTEL
#ifndef KOKKOS_ENABLE_INTEL_ATOMICS
#define KOKKOS_ENABLE_INTEL_ATOMICS KOKKOS_ATOMICS_USE_INTEL
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_OMP31
#ifndef KOKKOS_ENABLE_OPENMP_ATOMICS
#define KOKKOS_ENABLE_OPENMP_ATOMICS KOKKOS_ATOMICS_USE_OMP31
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_OPENMP31
#ifndef KOKKOS_ENABLE_OPENMP_ATOMICS
#define KOKKOS_ENABLE_OPENMP_ATOMICS KOKKOS_ATOMICS_USE_OPENMP31
#endif
#endif

#ifdef KOKKOS_ATOMICS_USE_WINDOWS
#ifndef KOKKOS_ENABLE_WINDOWS_ATOMICS
#define KOKKOS_ENABLE_WINDOWS_ATOMICS KOKKOS_ATOMICS_USE_WINDOWS
#endif
#endif

#ifdef KOKKOS_CUDA_CLANG_WORKAROUND
#ifndef KOKKOS_IMPL_CUDA_CLANG_WORKAROUND
#define KOKKOS_IMPL_CUDA_CLANG_WORKAROUND KOKKOS_CUDA_CLANG_WORKAROUND
#endif
#endif

#ifdef KOKKOS_CUDA_USE_LAMBDA
#ifndef KOKKOS_ENABLE_CUDA_LAMBDA
#define KOKKOS_ENABLE_CUDA_LAMBDA KOKKOS_CUDA_USE_LAMBDA
#endif
#endif

#ifdef KOKKOS_CUDA_USE_LDG_INTRINSIC
#ifndef KOKKOS_ENABLE_CUDA_LDG_INTRINSIC
#define KOKKOS_ENABLE_CUDA_LDG_INTRINSIC KOKKOS_CUDA_USE_LDG_INTRINSIC
#endif
#endif

#ifdef KOKKOS_CUDA_USE_RELOCATABLE_DEVICE_CODE
#ifndef KOKKOS_ENABLE_CUDA_RELOCATABLE_DEVICE_CODE
#define KOKKOS_ENABLE_CUDA_RELOCATABLE_DEVICE_CODE KOKKOS_CUDA_USE_RELOCATABLE_DEVICE_CODE
#endif
#endif

#ifdef KOKKOS_CUDA_USE_UVM
#ifndef KOKKOS_ENABLE_CUDA_UVM
#define KOKKOS_ENABLE_CUDA_UVM KOKKOS_CUDA_USE_UVM
#endif
#endif

#ifdef KOKKOS_HAVE_CUDA
#ifndef KOKKOS_ENABLE_CUDA
#define KOKKOS_ENABLE_CUDA KOKKOS_HAVE_CUDA
#endif
#endif

#ifdef KOKKOS_HAVE_CUDA_LAMBDA
#ifndef KOKKOS_ENABLE_CUDA_LAMBDA
#define KOKKOS_ENABLE_CUDA_LAMBDA KOKKOS_HAVE_CUDA_LAMBDA
#endif
#endif

#ifdef KOKKOS_HAVE_CUDA_RDC
#ifndef KOKKOS_ENABLE_CUDA_RELOCATABLE_DEVICE_CODE
#define KOKKOS_ENABLE_CUDA_RELOCATABLE_DEVICE_CODE KOKKOS_HAVE_CUDA_RDC
#endif
#endif

#ifdef KOKKOS_HAVE_CUSPARSE
#ifndef KOKKOS_ENABLE_CUSPARSE
#define KOKKOS_ENABLE_CUSPARSE KOKKOS_HAVE_CUSPARSE
#endif
#endif

#ifdef KOKKOS_HAVE_CXX1Z
#ifndef KOKKOS_ENABLE_CXX1Z
#define KOKKOS_ENABLE_CXX1Z KOKKOS_HAVE_CXX1Z
#endif
#endif

#ifdef KOKKOS_HAVE_DEBUG
#ifndef KOKKOS_DEBUG
#define KOKKOS_DEBUG KOKKOS_HAVE_DEBUG
#endif
#endif

#ifdef KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_CUDA
#ifndef KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_CUDA
#define KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_CUDA KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_CUDA
#endif
#endif

#ifdef KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_OPENMP
#ifndef KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_OPENMP
#define KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_OPENMP KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_OPENMP
#endif
#endif

#ifdef KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_SERIAL
#ifndef KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_SERIAL
#define KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_SERIAL KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_SERIAL
#endif
#endif

#ifdef KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_THREADS
#ifndef KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_THREADS
#define KOKKOS_ENABLE_DEFAULT_DEVICE_TYPE_THREADS KOKKOS_HAVE_DEFAULT_DEVICE_TYPE_THREADS
#endif
#endif

#ifdef KOKKOS_HAVE_HBWSPACE
#ifndef KOKKOS_ENABLE_HBWSPACE
#define KOKKOS_ENABLE_HBWSPACE KOKKOS_HAVE_HBWSPACE
#endif
#endif

#ifdef KOKKOS_HAVE_HWLOC
#ifndef KOKKOS_ENABLE_HWLOC
#define KOKKOS_ENABLE_HWLOC KOKKOS_HAVE_HWLOC
#endif
#endif

#ifdef KOKKOS_HAVE_MPI
#ifndef KOKKOS_ENABLE_MPI
#define KOKKOS_ENABLE_MPI KOKKOS_HAVE_MPI
#endif
#endif

#ifdef KOKKOS_HAVE_OPENMP
#ifndef KOKKOS_ENABLE_OPENMP
#define KOKKOS_ENABLE_OPENMP KOKKOS_HAVE_OPENMP
#endif
#endif

#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#ifndef KOKKOS_ENABLE_PRAGMA_IVDEP
#define KOKKOS_ENABLE_PRAGMA_IVDEP KOKKOS_HAVE_PRAGMA_IVDEP
#endif
#endif

#ifdef KOKKOS_OPT_RANGE_AGGRESSIVE_VECTORIZATION
#ifndef KOKKOS_ENABLE_AGGRESSIVE_VECTORIZATION
#define KOKKOS_ENABLE_AGGRESSIVE_VECTORIZATION KOKKOS_OPT_RANGE_AGGRESSIVE_VECTORIZATION
#endif
#endif

#ifdef KOKKOS_HAVE_PRAGMA_LOOPCOUNT
#ifndef KOKKOS_ENABLE_PRAGMA_LOOPCOUNT
#define KOKKOS_ENABLE_PRAGMA_LOOPCOUNT KOKKOS_HAVE_PRAGMA_LOOPCOUNT
#endif
#endif

#ifdef KOKKOS_HAVE_PRAGMA_SIMD
#ifndef KOKKOS_ENABLE_PRAGMA_SIMD
#define KOKKOS_ENABLE_PRAGMA_SIMD KOKKOS_HAVE_PRAGMA_SIMD
#endif
#endif

#ifdef KOKKOS_HAVE_PRAGMA_UNROLL
#ifndef KOKKOS_ENABLE_PRAGMA_UNROLL
#define KOKKOS_ENABLE_PRAGMA_UNROLL KOKKOS_HAVE_PRAGMA_UNROLL
#endif
#endif

#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#ifndef KOKKOS_ENABLE_PRAGMA_VECTOR
#define KOKKOS_ENABLE_PRAGMA_VECTOR KOKKOS_HAVE_PRAGMA_VECTOR
#endif
#endif

#ifdef KOKKOS_HAVE_PTHREAD
#ifndef KOKKOS_ENABLE_PTHREAD
#define KOKKOS_ENABLE_PTHREAD KOKKOS_HAVE_PTHREAD
#endif
#endif

#ifdef KOKKOS_HAVE_QTHREADS
#ifndef KOKKOS_ENABLE_QTHREADS
#define KOKKOS_ENABLE_QTHREADS KOKKOS_HAVE_QTHREADS
#endif
#endif

#ifdef KOKKOS_HAVE_SERIAL
#ifndef KOKKOS_ENABLE_SERIAL
#define KOKKOS_ENABLE_SERIAL KOKKOS_HAVE_SERIAL
#endif
#endif

#ifdef KOKKOS_HAVE_TYPE
#ifndef KOKKOS_IMPL_HAS_TYPE
#define KOKKOS_IMPL_HAS_TYPE KOKKOS_HAVE_TYPE
#endif
#endif

#ifdef KOKKOS_HAVE_WINTHREAD
#ifndef KOKKOS_ENABLE_WINTHREAD
#define KOKKOS_ENABLE_WINTHREAD KOKKOS_HAVE_WINTHREAD
#endif
#endif

#ifdef KOKKOS_HAVE_Winthread
#ifndef KOKKOS_ENABLE_WINTHREAD
#define KOKKOS_ENABLE_WINTHREAD KOKKOS_HAVE_Winthread
#endif
#endif

#ifdef KOKKOS_INTEL_MM_ALLOC_AVAILABLE
#ifndef KOKKOS_ENABLE_INTEL_MM_ALLOC
#define KOKKOS_ENABLE_INTEL_MM_ALLOC KOKKOS_INTEL_MM_ALLOC_AVAILABLE
#endif
#endif

#ifdef KOKKOS_MACRO_IMPL_TO_STRING
#ifndef KOKKOS_IMPL_MACRO_TO_STRING
#define KOKKOS_IMPL_MACRO_TO_STRING KOKKOS_MACRO_IMPL_TO_STRING
#endif
#endif

#ifdef KOKKOS_MACRO_TO_STRING
#ifndef KOKKOS_MACRO_TO_STRING
#define KOKKOS_MACRO_TO_STRING KOKKOS_MACRO_TO_STRING
#endif
#endif

#ifdef KOKKOS_MAY_ALIAS
#ifndef KOKKOS_IMPL_MAY_ALIAS
#define KOKKOS_IMPL_MAY_ALIAS KOKKOS_MAY_ALIAS
#endif
#endif

#ifdef KOKKOS_MDRANGE_IVDEP
#ifndef KOKKOS_IMPL_MDRANGE_IVDEP
#define KOKKOS_IMPL_MDRANGE_IVDEP KOKKOS_MDRANGE_IVDEP
#endif
#endif


#ifdef KOKKOS_MEMPOOL_PRINTERR
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINTERR
#define KOKKOS_ENABLE_MEMPOOL_PRINTERR KOKKOS_MEMPOOL_PRINTERR
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_ACTIVE_SUPERBLOCKS
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_ACTIVE_SUPERBLOCKS
#define KOKKOS_ENABLE_MEMPOOL_PRINT_ACTIVE_SUPERBLOCKS KOKKOS_MEMPOOL_PRINT_ACTIVE_SUPERBLOCKS
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_BLOCKSIZE_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_BLOCKSIZE_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_BLOCKSIZE_INFO KOKKOS_MEMPOOL_PRINT_BLOCKSIZE_INFO
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_CONSTRUCTOR_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_CONSTRUCTOR_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_CONSTRUCTOR_INFO KOKKOS_MEMPOOL_PRINT_CONSTRUCTOR_INFO
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_INDIVIDUAL_PAGE_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_INDIVIDUAL_PAGE_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_INDIVIDUAL_PAGE_INFO KOKKOS_MEMPOOL_PRINT_INDIVIDUAL_PAGE_INFO
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_INFO KOKKOS_MEMPOOL_PRINT_INFO
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_PAGE_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_PAGE_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_PAGE_INFO KOKKOS_MEMPOOL_PRINT_PAGE_INFO
#endif
#endif

#ifdef KOKKOS_MEMPOOL_PRINT_SUPERBLOCK_INFO
#ifndef KOKKOS_ENABLE_MEMPOOL_PRINT_SUPERBLOCK_INFO
#define KOKKOS_ENABLE_MEMPOOL_PRINT_SUPERBLOCK_INFO KOKKOS_MEMPOOL_PRINT_SUPERBLOCK_INFO
#endif
#endif

#ifdef KOKKOS_POSIX_MEMALIGN_AVAILABLE
#ifndef KOKKOS_ENABLE_POSIX_MEMALIGN
#define KOKKOS_ENABLE_POSIX_MEMALIGN KOKKOS_POSIX_MEMALIGN_AVAILABLE
#endif
#endif

#ifdef KOKKOS_POSIX_MMAP_FLAGS
#ifndef KOKKOS_IMPL_POSIX_MMAP_FLAGS
#define KOKKOS_IMPL_POSIX_MMAP_FLAGS KOKKOS_POSIX_MMAP_FLAGS
#endif
#endif

#ifdef KOKKOS_POSIX_MMAP_FLAGS_HUGE
#ifndef KOKKOS_IMPL_POSIX_MMAP_FLAGS_HUGE
#define KOKKOS_IMPL_POSIX_MMAP_FLAGS_HUGE KOKKOS_POSIX_MMAP_FLAGS_HUGE
#endif
#endif

#ifdef KOKKOS_SHARED_ALLOCATION_TRACKER_DECREMENT
#ifndef KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_DECREMENT
#define KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_DECREMENT KOKKOS_SHARED_ALLOCATION_TRACKER_DECREMENT
#endif
#endif

#ifdef KOKKOS_SHARED_ALLOCATION_TRACKER_ENABLED
#ifndef KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_ENABLED
#define KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_ENABLED KOKKOS_SHARED_ALLOCATION_TRACKER_ENABLED
#endif
#endif

#ifdef KOKKOS_SHARED_ALLOCATION_TRACKER_INCREMENT
#ifndef KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_INCREMENT
#define KOKKOS_IMPL_SHARED_ALLOCATION_TRACKER_INCREMENT KOKKOS_SHARED_ALLOCATION_TRACKER_INCREMENT
#endif
#endif

#ifdef KOKKOS_USE_CUDA_UVM
#ifndef KOKKOS_ENABLE_CUDA_UVM
#define KOKKOS_ENABLE_CUDA_UVM KOKKOS_USE_CUDA_UVM
#endif
#endif

#ifdef KOKKOS_USE_ISA_KNC
#ifndef KOKKOS_ENABLE_ISA_KNC
#define KOKKOS_ENABLE_ISA_KNC KOKKOS_USE_ISA_KNC
#endif
#endif

#ifdef KOKKOS_USE_ISA_POWERPCLE
#ifndef KOKKOS_ENABLE_ISA_POWERPCLE
#define KOKKOS_ENABLE_ISA_POWERPCLE KOKKOS_USE_ISA_POWERPCLE
#endif
#endif

#ifdef KOKKOS_USE_ISA_X86_64
#ifndef KOKKOS_ENABLE_ISA_X86_64
#define KOKKOS_ENABLE_ISA_X86_64 KOKKOS_USE_ISA_X86_64
#endif
#endif

#ifdef KOKKOS_USE_LIBRT
#ifndef KOKKOS_ENABLE_LIBRT
#define KOKKOS_ENABLE_LIBRT KOKKOS_USE_LIBRT
#endif
#endif

#ifdef KOKKOS_VIEW_OPERATOR_VERIFY
#ifndef KOKKOS_IMPL_VIEW_OPERATOR_VERIFY
#define KOKKOS_IMPL_VIEW_OPERATOR_VERIFY KOKKOS_VIEW_OPERATOR_VERIFY
#endif
#endif

#if defined( KOKKOS_ENABLE_PTHREAD ) || defined( KOKKOS_ENABLE_WINTHREAD )
#ifndef KOKKOS_ENABLE_THREADS
#define KOKKOS_ENABLE_THREADS
#endif
#endif

//------------------------------------------------------------------------------
// Deprecated macros
//------------------------------------------------------------------------------
#ifdef KOKKOS_HAVE_CXX11
#undef KOKKOS_HAVE_CXX11
#endif
#ifdef KOKKOS_ENABLE_CXX11
#undef KOKKOS_ENABLE_CXX11
#endif
#ifdef KOKKOS_USING_EXP_VIEW
#undef KOKKOS_USING_EXP_VIEW
#endif
#ifdef KOKKOS_USING_EXPERIMENTAL_VIEW
#undef KOKKOS_USING_EXPERIMENTAL_VIEW
#endif

#define KOKKOS_HAVE_CXX11 1
#define KOKKOS_ENABLE_CXX11 1
#define KOKKOS_USING_EXP_VIEW 1
#define KOKKOS_USING_EXPERIMENTAL_VIEW 1

#endif //KOKKOS_IMPL_OLD_MACROS_HPP
