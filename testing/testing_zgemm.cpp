/*
 *  -- MAGMA (version 1.0) --
 *     Univ. of Tennessee, Knoxville
 *     Univ. of California, Berkeley
 *     Univ. of Colorado, Denver
 *     November 2010
 *
 * @precisions normal z -> c d s
 *
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cublas.h>
#include "magma.h"
#include "magmablas.h"

#define PRECISION_z
magma_int_t M , N , K , LDA, LDB , LDC ;

double verifyResult(const cuDoubleComplex *mat, const cuDoubleComplex *mat_ref) {
    double norm = 0.0;
    for (magma_int_t i = 0; i < M; i++) {
        for (magma_int_t j = 0; j < N; j++) {
            cuDoubleComplex tmp;
            MAGMA_Z_OP_NEG(tmp, mat[i+j * M ], mat_ref[i+j * M ]);
            if (cuCabs( tmp ) > norm){
                norm = cuCabs( tmp );
            }
        }
    }
    return norm;
}

// define pfactor for number of flops in complex
#define PRECISION_z
#if (defined(PRECISION_s) || defined(PRECISION_d))
   #define pfactor 1.
#else
   #define pfactor 4.
#endif

int main( int argc, char** argv)
{
    magma_int_t oneTime =1024;
    magma_int_t step   = 1024;
    magma_int_t count  = 10;
    magma_int_t flag   = 0 ;

    char TRANSA = 'N';
    char TRANSB = 'N';
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    if (argc != 1){
        for(magma_int_t i = 1; i<argc; i++){
            if (strcmp("-N", argv[i])==0){
                oneTime = atoi(argv[++i]);
                step =  1000  ;
                count = 1;
                flag = 0 ;
            }
            else if (strcmp("-NN", argv[i])==0){
                TRANSA = TRANSB = MagmaNoTrans;
            }
            else if (strcmp("-TT", argv[i])==0){
                TRANSA = TRANSB = MagmaTrans;
            }
            else if (strcmp("-NT", argv[i])==0){
                TRANSA = MagmaNoTrans;
                TRANSB = MagmaTrans;
            }
            else if (strcmp("-TN", argv[i])==0){
                TRANSA = MagmaTrans;
                TRANSB = MagmaNoTrans;
            }
#if defined(PRECISION_z) || defined(PRECISION_c)
            else if (strcmp("-NC", argv[i])==0){
                TRANSA = MagmaNoTrans;
                TRANSB = MagmaConjTrans;
            }
            else if (strcmp("-TC", argv[i])==0){
                TRANSA = MagmaTrans;
                TRANSB = MagmaConjTrans;
            }
            else if (strcmp("-CN", argv[i])==0){
                TRANSA = MagmaConjTrans;
                TRANSB = MagmaNoTrans;
            }
            else if (strcmp("-CT", argv[i])==0){
                TRANSA = MagmaConjTrans;
                TRANSB = MagmaTrans;
            }
            else if (strcmp("-CC", argv[i])==0){
                TRANSA = TRANSB = MagmaConjTrans;
            }
#endif
        }
    }

    printf("\nUsage: \n");
    printf("  testing_zgemm [-NN|NT|TN|TT] [-N %d] \n\n", 1024);

    TimeStruct start, end;
    double cuda_perf , magma_perf ;

    cuInit( 0 );
    cublasInit( );
    printout_devices( );
    printf("\n");

    printf("\n");
    printf("Testing TRANSA = %c  TRANSB = %c\n", TRANSA, TRANSB);
    printf("    N     MAGMA GFLop/s    CUBLAS GFlop/s       error\n");
    printf("========================================================\n");
    for(magma_int_t i=oneTime;i<=(oneTime+(count-1)*step);i+=step){
        for( magma_int_t ops = 0 ; ops <1 + flag ; ops ++){

            cuDoubleComplex ALPHA = MAGMA_Z_ONE;
            cuDoubleComplex BETA  = MAGMA_Z_ONE;

            M = N = K = LDA = LDB = LDC =i+ops;
            magma_int_t size_A1 , size_B1, size_C1 ;

            if( TRANSA == 'N')
                size_A1 = LDA * K ;
            else
                size_A1 = LDA * M ;
            if( TRANSB == 'N')
                size_B1 = LDB * N ;
            else
                size_B1 = LDB * K ;

            size_C1 = LDC * N ;
            cuDoubleComplex *h_A = (cuDoubleComplex* ) malloc(sizeof(cuDoubleComplex) * size_A1);
            cuDoubleComplex *h_B = (cuDoubleComplex* ) malloc(sizeof(cuDoubleComplex) * size_B1);
            cuDoubleComplex *h_C_m = (cuDoubleComplex* ) malloc(sizeof(cuDoubleComplex) * size_C1);
            cuDoubleComplex *h_C_c = (cuDoubleComplex* ) malloc(sizeof(cuDoubleComplex) * size_C1);
            if( h_A == NULL ||  h_B == NULL ||  h_C_m == NULL ||  h_C_c == NULL ) {
                fprintf (stderr, "!!!! host memory allocation error\n");
                exit(1);
            }

            /* Initialize the matrices */
            lapackf77_zlarnv( &ione, ISEED, &size_A1, h_A );
            lapackf77_zlarnv( &ione, ISEED, &size_B1, h_B );
            lapackf77_zlarnv( &ione, ISEED, &size_C1, h_C_m );
            memcpy(h_C_c, h_C_m, sizeof(cuDoubleComplex) * size_C1);

            /* =====================================================================
               Performs operation using MAGMA-BLAS
               =================================================================== */
            cuDoubleComplex *d_A_m , *d_B_m , *d_C_m;
            cublasAlloc( size_A1, sizeof(cuDoubleComplex), (void**)&d_A_m );
            cublasAlloc( size_B1, sizeof(cuDoubleComplex), (void**)&d_B_m ) ;
            cublasAlloc( size_C1, sizeof(cuDoubleComplex), (void**)&d_C_m ) ;
            if(TRANSA=='N')
                cublasSetMatrix( M, K, sizeof( cuDoubleComplex ), h_A, LDA, d_A_m, LDA ) ;
            else
                cublasSetMatrix( K, M, sizeof( cuDoubleComplex ), h_A, LDA, d_A_m, LDA ) ;
            if(TRANSB=='N')
                cublasSetMatrix( K, N, sizeof( cuDoubleComplex ), h_B, LDB, d_B_m, LDB ) ;
            else
                cublasSetMatrix( N, K, sizeof( cuDoubleComplex ), h_B, LDB, d_B_m, LDB ) ;
            cublasSetMatrix( M, N, sizeof( cuDoubleComplex ), h_C_m, LDC, d_C_m, LDC ) ;


            start = get_current_time();
            magmablas_zgemm( TRANSA, TRANSB, M, N, K, ALPHA, d_A_m, LDA,
                             d_B_m, LDB, BETA, d_C_m, LDC );
            end = get_current_time();

            cublasGetMatrix( M, N, sizeof( cuDoubleComplex ), d_C_m, LDC, h_C_m, LDC ) ;
            magma_perf = pfactor*2.*M*N*K/(GetTimerValue(start,end))/1e6 ;
            cublasFree(d_A_m);
            cublasFree(d_B_m);
            cublasFree(d_C_m);
            /* =====================================================================
               Performs operation using CUDA-BLAS
               =================================================================== */
            cuDoubleComplex *d_A_c , *d_B_c , *d_C_c;
            cublasAlloc( size_A1, sizeof(cuDoubleComplex), (void**)&d_A_c );
            cublasAlloc( size_B1, sizeof(cuDoubleComplex), (void**)&d_B_c ) ;
            cublasAlloc( size_C1, sizeof(cuDoubleComplex), (void**)&d_C_c ) ;
            if(TRANSA=='N')
                cublasSetMatrix( M, K, sizeof( cuDoubleComplex ), h_A, LDA, d_A_c, LDA ) ;
            else
                cublasSetMatrix( K, M, sizeof( cuDoubleComplex ), h_A, LDA, d_A_c, LDA ) ;
            if(TRANSB=='N')
                cublasSetMatrix( K, N, sizeof( cuDoubleComplex ), h_B, LDB, d_B_c, LDB ) ;
            else
                cublasSetMatrix( N, K, sizeof( cuDoubleComplex ), h_B, LDB, d_B_c, LDB ) ;

            cublasSetMatrix( M, N, sizeof( cuDoubleComplex ), h_C_c, LDC, d_C_c, LDC ) ;
            start = get_current_time();
            cublasZgemm( TRANSA, TRANSB, M, N, K, ALPHA, d_A_c, LDA,
                         d_B_c, LDB, BETA, d_C_c, LDC );
            end = get_current_time();
            cublasGetMatrix( M, N, sizeof( cuDoubleComplex ), d_C_c, LDC, h_C_c, LDC ) ;
            cuda_perf = pfactor*2.*M*N*K/(GetTimerValue(start,end))/1e6 ;

            // * Memory clean up * /
            cublasFree(d_A_c);
            cublasFree(d_B_c);
            cublasFree(d_C_c);

            /* =====================================================================
               Error Computation and Performance Compariosn
               =================================================================== */
            double error = verifyResult(h_C_m, h_C_c);

            printf("%5d       %6.2f           %6.2f         %e\n",
                   M,magma_perf, cuda_perf, error);

            free(h_A);
            free(h_B);
            free(h_C_m);
            free(h_C_c);
        }
    }
    cublasShutdown();
}
