/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @precisions normal z -> s d c
*/
#include "magmasparse_internal.h"

#define COMPLEX

/* For hipSPARSE, they use a separate complex type than for hipBLAS */
#ifdef HAVE_HIP
  #define hipblasDoubleComplex hipDoubleComplex
#endif


// todo: make it spacific
#if CUDA_VERSION >= 11000  || defined(HAVE_HIP)
#define cusparseCreateSolveAnalysisInfo(info) {;}
#else
#define cusparseCreateSolveAnalysisInfo(info)                                                   \
    CHECK_CUSPARSE( cusparseCreateSolveAnalysisInfo( info ))
#endif

// todo: info is passed; buf has to be passed
#if CUDA_VERSION >= 11000 || defined(HAVE_HIP)
#define cusparseZcsrsv_analysis(handle, trans, m, nnz, descr, val, row, col, info)              \
    {                                                                                           \
        csrsv2Info_t linfo = 0;                                                                 \
        int bufsize;                                                                            \
        void *buf;                                                                              \
        cusparseCreateCsrsv2Info(&linfo);                                                       \
        cusparseZcsrsv2_bufferSize(handle, trans, m, nnz, descr, (cuDoubleComplex*)val, row, col, \
                                   linfo, &bufsize);                                            \
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseZcsrsv2_analysis(handle, trans, m, nnz, descr, val, row, col, linfo,            \
                                 CUSPARSE_SOLVE_POLICY_USE_LEVEL, buf);                         \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif

/**
    Purpose
    -------

    Reads in an Incomplete Cholesky preconditioner.

    Arguments
    ---------

    @param[in]
    A           magma_z_matrix
                input matrix A
                
    @param[in]
    b           magma_z_matrix
                input RHS b

    @param[in,out]
    precond     magma_z_preconditioner*
                preconditioner parameters
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_zgepr
    ********************************************************************/
extern "C"
magma_int_t
magma_zcustomicsetup(
    magma_z_matrix A,
    magma_z_matrix b,
    magma_z_preconditioner *precond,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    cusparseHandle_t cusparseHandle=NULL;
    cusparseMatDescr_t descrL=NULL;
    cusparseMatDescr_t descrU=NULL;
    
    magma_z_matrix hA={Magma_CSR};
    char preconditionermatrix[255];
    
    snprintf( preconditionermatrix, sizeof(preconditionermatrix),
                "/Users/hanzt0114cl306/work/matrices/matrices/ICT.mtx" );
    
    CHECK( magma_z_csr_mtx( &hA, preconditionermatrix , queue) );
    
    
    // for CUSPARSE
    CHECK( magma_zmtransfer( hA, &precond->M, Magma_CPU, Magma_DEV , queue ));

        // copy the matrix to precond->L and (transposed) to precond->U
    CHECK( magma_zmtransfer(precond->M, &(precond->L), Magma_DEV, Magma_DEV, queue ));
    CHECK( magma_zmtranspose( precond->L, &(precond->U), queue ));

    // extract the diagonal of L into precond->d
    CHECK( magma_zjacobisetup_diagscal( precond->L, &precond->d, queue ));
    CHECK( magma_zvinit( &precond->work1, Magma_DEV, hA.num_rows, 1, MAGMA_Z_ZERO, queue ));

    // extract the diagonal of U into precond->d2
    CHECK( magma_zjacobisetup_diagscal( precond->U, &precond->d2, queue ));
    CHECK( magma_zvinit( &precond->work2, Magma_DEV, hA.num_rows, 1, MAGMA_Z_ZERO, queue ));


    // CUSPARSE context //
    CHECK_CUSPARSE( cusparseCreate( &cusparseHandle ));
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrL ));
    CHECK_CUSPARSE( cusparseSetMatType( descrL, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrL, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrL, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrL, CUSPARSE_FILL_MODE_LOWER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoL );
    cusparseZcsrsv_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_NON_TRANSPOSE, precond->M.num_rows,
                             precond->M.nnz, descrL,
                             (cuDoubleComplex*)precond->M.val, precond->M.row, precond->M.col, 
                             precond->cuinfoL );
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrU ));
    CHECK_CUSPARSE( cusparseSetMatType( descrU, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrU, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrU, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrU, CUSPARSE_FILL_MODE_LOWER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoU );
    cusparseZcsrsv_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_TRANSPOSE, precond->M.num_rows,
                             precond->M.nnz, descrU,
                             (cuDoubleComplex*)precond->M.val, precond->M.row, precond->M.col, 
                             precond->cuinfoU );

    
    cleanup:
        
    cusparseDestroy( cusparseHandle );
    cusparseDestroyMatDescr( descrL );
    cusparseDestroyMatDescr( descrU );
    cusparseHandle=NULL;
    descrL=NULL;
    descrU=NULL;    
    magma_zmfree( &hA, queue );
    
    return info;
}
    
