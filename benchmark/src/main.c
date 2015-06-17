//-----------------------------------------------------------------------------
//! Copyright (c) 2014-2015, Benjamin Worpitz
//! All rights reserved.
//!
//! Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met :
//! * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
//! * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
//! * Neither the name of the TU Dresden nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
//!
//! THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
//! IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//! HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#include <matmul/matmul.h>

#include <stdio.h>                  // printf
#include <assert.h>                 // assert
#include <stdlib.h>                 // malloc, free
#include <stdbool.h>                // bool, true, false
#include <time.h>                   // time()
#include <float.h>                  // DBL_MAX
#include <math.h>                   // pow

#ifdef _MSC_VER
    #include <Windows.h>
    //-----------------------------------------------------------------------------
    //! \return A monotonically increasing time value in seconds.
    //-----------------------------------------------------------------------------
    double getTimeSec()
    {
        LARGE_INTEGER li, frequency;
        BOOL bSucQueryPerformanceCounter = QueryPerformanceCounter(&li);
        if(bSucQueryPerformanceCounter)
        {
            BOOL bSucQueryPerformanceFrequency = QueryPerformanceFrequency(&frequency);
            if(bSucQueryPerformanceFrequency)
            {
                return ((double)li.QuadPart)/((double)frequency.QuadPart);
            }
            else
            {
                // Throw assertion in debug mode, else return 0 time.
                assert(bSucQueryPerformanceFrequency);
                return 0.0;
            }
        }
        else
        {
            // Throw assertion in debug mode, else return 0 time.
            assert(bSucQueryPerformanceCounter);
            return 0.0;
        }
    }
#else
    #include <sys/time.h>
    //-----------------------------------------------------------------------------
    //! \return A monotonically increasing time value in seconds.
    //-----------------------------------------------------------------------------
    double getTimeSec()
    {
        struct timeval act_time;
        gettimeofday(&act_time, NULL);
        return (double)act_time.tv_sec + (double)act_time.tv_usec / 1000000.0;
    }
#endif

//-----------------------------------------------------------------------------
//! A struct holding the algorithms to benchmark.
//-----------------------------------------------------------------------------
typedef struct SMatMulAlgo
{
    void(*pMatMul)(size_t const, size_t const, size_t const, TElem const, TElem const * const, size_t const, TElem const * const, size_t const, TElem const, TElem * const, size_t const);
    char const * pszName;
    double const fExponentOmega;
} SMatMulAlgo;

//-----------------------------------------------------------------------------
//! \return The time in milliseconds required to multiply 2 random matrices of the given type and size
//! \param n The matrix dimension.
//-----------------------------------------------------------------------------
double measureRandomMatMul(
    SMatMulAlgo const * const algo,
    size_t const m, size_t const n, size_t const k,
    size_t const uiRepeatCount
 #ifdef BENCHMARK_VERIFY_RESULT
    ,bool * pbResultsCorrect
#endif
    )
{
    // Generate random alpha and beta.
    TElem const alpha = matmul_gen_rand_val(MATMUL_EPSILON, (TElem)1);
    TElem const beta = matmul_gen_rand_val(MATMUL_EPSILON, (TElem)1);

    // Allocate and initialize the matrices of the given size.
#ifdef MATMUL_MPI
    size_t const uiNumElements = n * n;
    TElem const * /*const*/ A = 0;
    TElem const * /*const*/ B = 0;
    TElem * /*const*/ C = 0;
    #ifdef BENCHMARK_VERIFY_RESULT
        TElem * /*const*/ D = 0;
    #endif

    int iRank1D;
    MPI_Comm_rank(MATMUL_MPI_COMM, &iRank1D);
    if(iRank1D==MATMUL_MPI_ROOT)
    {
        A = matmul_arr_alloc_rand_fill(uiNumElements);
        B = matmul_arr_alloc_rand_fill(uiNumElements);
        C = matmul_arr_alloc(uiNumElements);
    #ifdef BENCHMARK_VERIFY_RESULT
        D = matmul_arr_alloc(uiNumElements);
    #endif
    }
#else
    size_t const uiNumElements = n * n;
    TElem const * const A = matmul_arr_alloc_rand_fill(uiNumElements);
    TElem const * const B = matmul_arr_alloc_rand_fill(uiNumElements);
    TElem * const C = matmul_arr_alloc(uiNumElements);
    #ifdef BENCHMARK_VERIFY_RESULT
        TElem * const D = matmul_arr_alloc(uiNumElements);
    #endif
#endif

    // Initialize the measurement result.
#ifdef BENCHMARK_REPEAT_TAKE_MINIMUM
    double fTimeMeasuredSec = DBL_MAX;
#else
    double fTimeMeasuredSec = 0.0;
#endif

    // Iterate.
    for(size_t i = 0; i < uiRepeatCount; ++i)
    {
#ifdef MATMUL_MPI
        if(iRank1D==MATMUL_MPI_ROOT)
        {
#endif
            // Because we calculate C += A*B we need to initialize C.
            // Even if we would not need this, we would have to initialize the C array with data before using it because else we would measure page table time on first write.
            // We have to fill C with new data in subsequent iterations because else the values in C would get bigger and bigger in each iteration.
            matmul_arr_rand_fill(C, uiNumElements);
#ifdef BENCHMARK_VERIFY_RESULT
            matmul_mat_copy(n, n, C, n, D, n);
#endif

#ifdef MATMUL_MPI
        }
#endif

#ifdef MATMUL_MPI
        double fTimeStart = 0;
        // Only the root process does the printing.
        if(iRank1D==MATMUL_MPI_ROOT)
        {
#endif

#ifdef BENCHMARK_PRINT_ITERATIONS
            // If there are multiple repetitions, print the iteration we are at now.
            if(uiRepeatCount!=1)
            {
                if(i>0)
                {
                    printf("; ");
                }
                printf("\ti=%"MATMUL_PRINTF_SIZE_T, i);
            }
#endif

#ifdef MATMUL_MPI
            fTimeStart = getTimeSec();
        }
#else
        double const fTimeStart = getTimeSec();
#endif
        // Matrix multiplication.
        (algo->pMatMul)(n, n, n, alpha, A, n, B, n, beta, C, n);

#ifdef MATMUL_MPI
        // Only the root process does the printing.
        if(iRank1D==MATMUL_MPI_ROOT)
        {
#endif
            double const fTimeEnd = getTimeSec();
            double const fTimeElapsed = fTimeEnd - fTimeStart;

#ifdef BENCHMARK_PRINT_MATRICES
            matmul_mat_print(n, n, A, n);
            printf("*\n");
            matmul_mat_print(n, n, B, n);
            printf("=\n");
            matmul_mat_print(n, n, C, n);
            printf("\n");
#endif

#ifdef BENCHMARK_VERIFY_RESULT
            matmul_gemm_seq_basic(n, n, n, alpha, A, n, B, n, beta, D, n);

            // The threshold difference from where the value is considered to be a real error.
            TElem const fErrorThreshold = (TElem)(MATMUL_EPSILON * ((TElem)m) * ((TElem)n) * ((TElem)k));
            *pbResultsCorrect = matmul_mat_cmp(n, n, C, n, D, n, fErrorThreshold);
#endif

#ifdef BENCHMARK_REPEAT_TAKE_MINIMUM
            fTimeMeasuredSec = (fTimeElapsed<fTimeMeasuredSec) ? fTimeElapsed : fTimeMeasuredSec;
#else
            fTimeMeasuredSec += fTimeElapsed * (1.0/double(BENCHMARK_REPEAT_COUNT));
#endif

#ifdef MATMUL_MPI
        }
#endif
    }

#ifdef MATMUL_MPI
    // Only the root process does the printing.
    if(iRank1D==MATMUL_MPI_ROOT)
    {
#endif

#ifndef BENCHMARK_PRINT_GFLOPS
        // Print the time needed for the calculation.
        printf("\t%12.8lf", fTimeMeasuredSec);
#else
        // Print the GFLOPS.
        double const fOperations = 2.0*pow((double)n, (algo.pMatMul)->fExponentOmega);
        double const fFLOPS = (fTimeMeasuredSec!=0) ? (fOperations/fTimeMeasuredSec) : 0.0;
        printf("\t%12.8lf", fFLOPS*1.0e-9);
#endif

        matmul_arr_free((TElem * const)A);
        matmul_arr_free((TElem * const)B);
        matmul_arr_free(C);
#ifdef BENCHMARK_VERIFY_RESULT
        matmul_arr_free(D);
#endif

#ifdef MATMUL_MPI
    }
#endif

    return fTimeMeasuredSec;
}

//-----------------------------------------------------------------------------
//! A struct containing an array of all matrix sizes to test.
//-----------------------------------------------------------------------------
typedef struct SMatMulSizes
{
    size_t uiNumSizes;
    size_t * puiSizes;
} SMatMulSizes;

//-----------------------------------------------------------------------------
//! Fills the matrix sizes struct.
//! \param uiNMin The start matrix dimension.
//! \param uiStepWidth The step width for each iteration. If set to 0 the size is doubled on each iteration.
//! \param uiNMax The macimum matrix dimension.
//-----------------------------------------------------------------------------
SMatMulSizes buildSizes(
    size_t const uiNMin,
    size_t const uiStepWidth,
    size_t const uiNMax)
{
    SMatMulSizes sizes;
    sizes.uiNumSizes = 0;
    sizes.puiSizes = 0;

    size_t uiN;
    for(uiN = uiNMin; uiN <= uiNMax; uiN += (uiStepWidth == 0) ? uiN : uiStepWidth)
    {
        ++sizes.uiNumSizes;
    }

    sizes.puiSizes = (size_t *)malloc(sizes.uiNumSizes * sizeof(size_t));

    size_t uiIdx = 0;
    for(uiN = uiNMin; uiN <= uiNMax; uiN += (uiStepWidth == 0) ? uiN : uiStepWidth)
    {
        sizes.puiSizes[uiIdx] = uiN;
        ++uiIdx;
    }

    return sizes;
}

//-----------------------------------------------------------------------------
//! Class template with static member templates because function templates do not allow partial specialization.
//! \param uiNMin The start matrix dimension.
//! \param uiStepWidth The step width for each iteration. If set to 0 the size is doubled on each iteration.
//! \param uiNMax The maximum matrix dimension.
//! \return True, if all results are correct.
//-----------------------------------------------------------------------------
#ifdef BENCHMARK_VERIFY_RESULT
    bool
#else
    void
#endif
measureRandomMatMuls(
    SMatMulAlgo const * const pMatMulAlgos,
    size_t const uiNumAlgos,
    SMatMulSizes const * const pSizes,
    size_t const uiRepeatCount)
{
#ifdef MATMUL_MPI
    int iRank1D;
    MPI_Comm_rank(MATMUL_MPI_COMM, &iRank1D);
    if(iRank1D==MATMUL_MPI_ROOT)
    {
#endif
#ifndef BENCHMARK_PRINT_GFLOPS
        printf("\n#time in s");
#else
        printf("\n#GFLOPS");
#endif
        printf("\nm=n=k");
        // Table heading
        for(size_t uiAlgoIdx = 0; uiAlgoIdx < uiNumAlgos; ++uiAlgoIdx)
        {
                printf("\t%s", pMatMulAlgos[uiAlgoIdx].pszName);
        }
#ifdef MATMUL_MPI
    }
#endif

#ifdef BENCHMARK_VERIFY_RESULT
    bool bAllResultsCorrect = true;
#endif
    if(pSizes)
    {
        for(size_t uiSizeIdx = 0; uiSizeIdx < pSizes->uiNumSizes; ++uiSizeIdx)
        {
            size_t const n = pSizes->puiSizes[uiSizeIdx];
            // Print the operation
            printf("\n%"MATMUL_PRINTF_SIZE_T, n);

            for(size_t uiAlgoIdx = 0; uiAlgoIdx < uiNumAlgos; ++uiAlgoIdx)
            {
#ifdef BENCHMARK_VERIFY_RESULT
                bool bResultsCorrectAlgo = true;
#endif
                // Execute the operation and measure the time taken.
                measureRandomMatMul(
                    &pMatMulAlgos[uiAlgoIdx],
                    n,
                    n,
                    n,
                    uiRepeatCount
#ifdef BENCHMARK_VERIFY_RESULT
                    , &bResultsCorrectAlgo
#endif
                    );

#ifdef BENCHMARK_VERIFY_RESULT
                    bAllResultsCorrect &= bResultsCorrectAlgo;
#endif
            }
        }
    }
    else
    {
        printf("Pointer to structure of test sizes 'pSizes' is not allowed to be nullptr!\n");
    }

#ifdef BENCHMARK_VERIFY_RESULT
    return bAllResultsCorrect;
#endif
}

//-----------------------------------------------------------------------------
//! Prints some startup informations.
//-----------------------------------------------------------------------------
void main_print_startup()
{
    printf("# matmul benchmark copyright (c) 2014-2015, Benjamin Worpitz:");
#ifdef NDEBUG
    printf(" Release");
#else
    printf(" Debug");
#endif
    printf("; BENCHMARK_MIN_N=%"MATMUL_PRINTF_SIZE_T, (size_t)BENCHMARK_MIN_N);
    printf("; BENCHMARK_STEP_N=%"MATMUL_PRINTF_SIZE_T, (size_t)BENCHMARK_STEP_N);
    printf("; BENCHMARK_MAX_N=%"MATMUL_PRINTF_SIZE_T, (size_t)BENCHMARK_MAX_N);
    printf("; BENCHMARK_REPEAT_COUNT=%"MATMUL_PRINTF_SIZE_T, (size_t)BENCHMARK_REPEAT_COUNT);
#ifdef BENCHMARK_PRINT_GFLOPS
    printf("; BENCHMARK_PRINT_GFLOPS=ON");
#else
    printf("; BENCHMARK_PRINT_GFLOPS=OFF");
#endif
#ifdef BENCHMARK_REPEAT_TAKE_MINIMUM
    printf("; BENCHMARK_REPEAT_TAKE_MINIMUM=ON");
#else
    printf("; BENCHMARK_REPEAT_TAKE_MINIMUM=OFF");
#endif
#ifdef BENCHMARK_VERIFY_RESULT
    printf("; BENCHMARK_VERIFY_RESULT");
#endif
#ifdef BENCHMARK_PRINT_MATRICES
    printf("; BENCHMARK_PRINT_MATRICES");
#endif
#ifdef BENCHMARK_PRINT_ITERATIONS
    printf("; BENCHMARK_PRINT_ITERATIONS");
#endif
#ifdef MATMUL_MPI
    printf("; MATMUL_MPI");
#endif
}

//-----------------------------------------------------------------------------
//! Main method initiating the measurements of all algorithms selected in config.h.
//-----------------------------------------------------------------------------
int main(
#ifdef MATMUL_MPI
    int argc,
    char ** argv
#endif
    )
{
    // Disable buffering of fprintf. Always print it immediately.
    setvbuf(stdout, 0, _IONBF, 0);

#ifdef MATMUL_MPI
    // Initialize MPI before calling any mpi methods.
    int mpiStatus = MPI_Init(&argc, &argv);
    int iRank1D;
    if(mpiStatus != MPI_SUCCESS)
    {
        printf("\nUnable to initialize MPI. MPI_Init failed.");
    }
    else
    {
        MPI_Comm_rank(MATMUL_MPI_COMM, &iRank1D);

        if(iRank1D==MATMUL_MPI_ROOT)
        {
            main_print_startup();
        }
    }
#else
    main_print_startup();
#endif

    double const fTimeStart = getTimeSec();

    srand((unsigned int)time(0));

    static SMatMulAlgo const algos[] = {
#ifndef MATMUL_MPI
    #ifdef BENCHMARK_SEQ_BASIC
        {matmul_gemm_seq_basic, "gemm_seq_basic", 3.0},
    #endif
    #ifdef BENCHMARK_SEQ_SINGLE_OPTS
        {matmul_gemm_seq_index_pointer, "gemm_seq_index_pointer", 3.0},
        {matmul_gemm_seq_restrict, "gemm_seq_restrict", 3.0},
        {matmul_gemm_seq_loop_reorder, "gemm_seq_loop_reorder", 3.0},
        {matmul_gemm_seq_index_precalculate, "gemm_seq_index_precalculate", 3.0},
        {matmul_gemm_seq_loop_unroll_4, "gemm_seq_loop_unroll_4", 3.0},
        {matmul_gemm_seq_loop_unroll_8, "gemm_seq_loop_unroll_8", 3.0},
        {matmul_gemm_seq_loop_unroll_16, "gemm_seq_loop_unroll_16", 3.0},
        {matmul_gemm_seq_block, "gemm_seq_block", 3.0},
    #endif
    #ifdef BENCHMARK_SEQ_MULTIPLE_OPTS
        {matmul_gemm_seq_multiple_opts_block, "gemm_seq_complete_opt_block", 3.0},
        {matmul_gemm_seq_multiple_opts_no_block, "gemm_seq_complete_opt_no_block", 3.0},
        {matmul_gemm_seq_multiple_opts, "gemm_seq_complete_opt", 3.0},
    #endif
    #ifdef BENCHMARK_SEQ_STRASSEN
        {matmul_gemm_seq_strassen, "gemm_seq_strassen", 2.80735},   // 2.80735 = log(7.0) / log(2.0)
    #endif
    #ifdef BENCHMARK_PAR_OMP2
        #if _OPENMP >= 200203   // OpenMP 2.0
        {matmul_gemm_par_omp2_guided_schedule, "gemm_par_omp2_guided_schedule", 3.0},
        {matmul_gemm_par_omp2_static_schedule, "gemm_par_omp2_static_schedule", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_OMP3
        #if _OPENMP >= 200805   // OpenMP 3.0 (3.1=201107)
        {matmul_gemm_par_omp3_static_schedule_collapse, "gemm_par_omp3_static_schedule_collapse", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_OMP4
        #if _OPENMP >= 201307   // OpenMP 4.0
        {matmul_gemm_par_omp4, "gemm_par_omp4", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_PHI_OFF_OMP2
        #if _OPENMP >= 200203   // OpenMP 2.0
        {matmul_gemm_par_phi_off_omp2_guided_schedule, "gemm_par_phi_off_omp2_guided_schedule", 3.0},
        {matmul_gemm_par_phi_off_omp2_static_schedule, "gemm_par_phi_off_omp2_static_schedule", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_PHI_OFF_OMP3
        #if _OPENMP >= 200805   // OpenMP 3.0
        {matmul_gemm_par_phi_off_omp3_static_schedule_collapse, "gemm_par_phi_off_omp3_static_schedule_collapse", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_PHI_OFF_OMP4
        #if _OPENMP >= 201307   // OpenMP 4.0
        {matmul_gemm_par_phi_off_omp4, "gemm_par_phi_off_omp4", 3.0},
        #endif
    #endif
    #ifdef BENCHMARK_PAR_STRASSEN_OMP2
        #if _OPENMP >= 200203   // OpenMP 2.0
        {matmul_gemm_par_strassen_omp2, "gemm_par_strassen_omp", 2.80735},   // 2.80735 = log(7.0) / log(2.0)
        #endif
    #endif
    #ifdef BENCHMARK_PAR_OPENACC
        {matmul_gemm_par_openacc_kernels, "gemm_par_openacc_kernels", 3.0},
        {matmul_gemm_par_openacc_parallel, "gemm_par_openacc_parallel", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_CUDA
        {matmul_gemm_par_cuda, "gemm_par_cuda", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_B_SEQ_T_SEQ
        {matmul_gemm_par_alpaka_cpu_b_seq_t_seq, "gemm_par_alpaka_cpu_b_seq_t_seq", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_B_OMP2_T_SEQ
        {matmul_gemm_par_alpaka_cpu_b_omp2_t_seq, "gemm_par_alpaka_cpu_b_omp2_t_seq", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_B_SEQ_T_OMP2
        {matmul_gemm_par_alpaka_cpu_b_seq_t_omp2, "gemm_par_alpaka_cpu_b_seq_t_omp2", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_BT_OMP4
        {matmul_gemm_par_alpaka_cpu_bt_omp4, "gemm_par_alpaka_cpu_bt_omp4", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_B_SEQ_T_THREADS
        {matmul_gemm_par_alpaka_cpu_b_seq_t_threads, "gemm_par_alpaka_cpu_b_seq_t_threads", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_CPU_B_SEQ_T_FIBERS
        {matmul_gemm_par_alpaka_cpu_b_seq_t_fibers, "gemm_par_alpaka_cpu_b_seq_t_fibers", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_ALPAKA_ACC_GPU_CUDA
        {matmul_gemm_par_alpaka_gpu_cuda, "gemm_par_alpaka_gpu_cuda", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_BLAS_MKL
        {matmul_gemm_par_blas_mkl, "gemm_par_blas_mkl", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_PHI_OFF_BLAS_MKL
        {matmul_gemm_par_phi_off_blas_mkl, "gemm_par_phi_off_blas_mkl", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_BLAS_CUBLAS
        {matmul_gemm_par_blas_cublas2, "gemm_par_blas_cublas2", 3.0},
    #endif
#else
    #ifdef BENCHMARK_PAR_MPI_CANNON_STD
        {matmul_gemm_par_mpi_cannon_block, "gemm_par_mpi_cannon_block", 3.0},
        {matmul_gemm_par_mpi_cannon_nonblock, "gemm_par_mpi_cannon_nonblock", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_MPI_CANNON_MKL
        {matmul_gemm_par_mpi_cannon_nonblock_blas_mkl, "gemm_par_mpi_cannon_nonblock_blas_mkl", 3.0},
    #endif
    #ifdef BENCHMARK_BUILD_PAR_MPI_CANNON_CUBLAS
        {matmul_gemm_par_mpi_cannon_nonblock_blas_cublas, "gemm_par_mpi_cannon_nonblock_blas_cublas", 3.0},
    #endif
    #ifdef BENCHMARK_PAR_MPI_DNS
        {matmul_gemm_par_mpi_dns, "gemm_par_mpi_dns", 3.0},
    #endif
#endif
    };

    SMatMulSizes const sizes = buildSizes(
        (size_t)BENCHMARK_MIN_N,
        (size_t)BENCHMARK_STEP_N,
        (size_t)BENCHMARK_MAX_N);

#ifdef BENCHMARK_VERIFY_RESULT
    bool const bAllResultsCorrect =
#endif
    measureRandomMatMuls(
        algos,
        sizeof(algos)/sizeof(algos[0]),
        &sizes,
        BENCHMARK_REPEAT_COUNT);

    free(sizes.puiSizes);

#ifdef MATMUL_MPI
    if(mpiStatus == MPI_SUCCESS)
    {
        if(iRank1D==MATMUL_MPI_ROOT)
        {
            double const fTimeEnd = getTimeSec();
            double const fTimeElapsed = fTimeEnd - fTimeStart;
            printf("\nTotal runtime: %12.6lf s ", fTimeElapsed);
        }
        MPI_Finalize();
    }
#else
    double const fTimeEnd = getTimeSec();
    double const fTimeElapsed = fTimeEnd - fTimeStart;
    printf("\nTotal runtime: %12.6lf s ", fTimeElapsed);
#endif

#ifdef BENCHMARK_VERIFY_RESULT
    return !bAllResultsCorrect;
#else
    return EXIT_SUCCESS;
#endif
}