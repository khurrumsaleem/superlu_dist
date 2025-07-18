#pragma once
#include <cstdio>
#include <cinttypes>
#include "superlu_ddefs.h"
// #include "lupanels.hpp"
#include "xlupanels.hpp"

#ifdef HAVE_CUDA
#include "lupanels_GPU.cuh"
#include "xlupanels_GPU.cuh"


int getBufferOffset(int k0, int k1, int winSize, int winParity, int halfWin)
{
    int offset = (k0 - k1) % winSize;
    if (winParity % 2)
         offset += halfWin;

    return offset;
}

template <typename Ftype>
int_t xLUstruct_t<Ftype>::dDFactPSolveGPU(int_t k, int_t offset, diagFactBufs_type<Ftype> **dFBufs)
{
    // this is new version with diagonal factor being performed on GPU
    // different from dDiagFactorPanelSolveGPU (it performs diag factor in CPU)

    /* Sherry: argument dFBufs[] is on CPU, not used in this routine */

    double t0 = SuperLU_timer_();
    int ksupc = SuperSize(k);
    cublasHandle_t cubHandle = A_gpu.cuHandles[offset];
    cusolverDnHandle_t cusolverH = A_gpu.cuSolveHandles[offset];
    cudaStream_t cuStream = A_gpu.cuStreams[offset];

    /*======= Diagonal Factorization ======*/
    if (iam == procIJ(k, k))
    {
        lPanelVec[g2lCol(k)].diagFactorCuSolver(k,
                                                cusolverH, cuStream,
                                                A_gpu.diagFactWork[offset], A_gpu.diagFactInfo[offset], // CPU pointers
                                                A_gpu.dFBufs[offset], ksupc, // CPU pointers
                                                thresh, xsup, options, stat, info);
	stat->ops[FACT] += 2.0/3.0 * ksupc * ksupc;
    }

    // CHECK_MALLOC(iam, "after diagFactorCuSolver()");

    // TODO: need to synchronize the cuda stream
    /*======= Diagonal Broadcast ======*/
    if (myrow == krow(k))
        MPI_Bcast((void *)A_gpu.dFBufs[offset], ksupc * ksupc,
                  get_mpi_type<Ftype>(), kcol(k), (grid->rscp).comm);

    // CHECK_MALLOC(iam, "after row Bcast");

    if (mycol == kcol(k))
        MPI_Bcast((void *)A_gpu.dFBufs[offset], ksupc * ksupc,
                  get_mpi_type<Ftype>(), krow(k), (grid->cscp).comm);

    // do the panels solver
    if (myrow == krow(k))
    {
        uPanelVec[g2lRow(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[offset], ksupc);
        cudaStreamSynchronize(cuStream); // synchronize befpre broadcast
    if(uPanelVec[g2lRow(k)].nzvalSize()>0){
	int ncols = uPanelVec[g2lRow(k)].nzcols();
	stat->ops[FACT] += ksupc*ksupc * ncols; // Sherry added 5/4/25
    }
    }

    if (mycol == kcol(k))
    {
        lPanelVec[g2lCol(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[offset], ksupc);
        cudaStreamSynchronize(cuStream);
	if(lPanelVec[g2lCol(k)].nzvalSize()>0){
	int nrows = lPanelVec[g2lCol(k)].nzrows();
	stat->ops[FACT] += ksupc*ksupc * nrows; // Sherry added 5/4/25
    }
    }
    SCT->tDiagFactorPanelSolve += (SuperLU_timer_() - t0);

    return 0;
} /* dDFactPSolveGPU */

template <typename Ftype>
int_t xLUstruct_t<Ftype>::dDFactPSolveGPU(int_t k, int_t handle_offset, int buffer_offset, diagFactBufs_type<Ftype> **dFBufs)
{
    // this is new version with diagonal factor being performed on GPU
    // different from dDiagFactorPanelSolveGPU (it performs diag factor in CPU)

    /* Sherry: argument dFBufs[] is on CPU, not used in this routine */

    double t0 = SuperLU_timer_();
    int ksupc = SuperSize(k);
    cublasHandle_t cubHandle = A_gpu.cuHandles[handle_offset];
    cusolverDnHandle_t cusolverH = A_gpu.cuSolveHandles[handle_offset];
    cudaStream_t cuStream = A_gpu.cuStreams[handle_offset];

    /*======= Diagonal Factorization ======*/
    if (iam == procIJ(k, k))
    {
        lPanelVec[g2lCol(k)].diagFactorCuSolver(k,
                                                cusolverH, cuStream,
                                                A_gpu.diagFactWork[handle_offset], A_gpu.diagFactInfo[handle_offset], // CPU pointers
                                                A_gpu.dFBufs[buffer_offset], ksupc,                                   // CPU pointers
                                                thresh, xsup, options, stat, info);
    }

    // CHECK_MALLOC(iam, "after diagFactorCuSolver()");

    // TODO: need to synchronize the cuda stream
    /*======= Diagonal Broadcast ======*/
    if (myrow == krow(k))
        MPI_Bcast((void *)A_gpu.dFBufs[buffer_offset], ksupc * ksupc,
                  get_mpi_type<Ftype>(), kcol(k), (grid->rscp).comm);

    // CHECK_MALLOC(iam, "after row Bcast");

    if (mycol == kcol(k))
        MPI_Bcast((void *)A_gpu.dFBufs[buffer_offset], ksupc * ksupc,
                  get_mpi_type<Ftype>(), krow(k), (grid->cscp).comm);

    // do the panels solver
    if (myrow == krow(k))
    {
        uPanelVec[g2lRow(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[buffer_offset], ksupc);
        cudaStreamSynchronize(cuStream); // synchronize befpre broadcast
    }

    if (mycol == kcol(k))
    {
        lPanelVec[g2lCol(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[buffer_offset], ksupc);
        cudaStreamSynchronize(cuStream);
    }
    SCT->tDiagFactorPanelSolve += (SuperLU_timer_() - t0);

    return 0;
} /* dDFactPSolveGPU */

/* This performs diag factor on CPU */
template <typename Ftype>
int_t xLUstruct_t<Ftype>::dDiagFactorPanelSolveGPU(int_t k, int_t offset, diagFactBufs_type<Ftype> **dFBufs)
{
    double t0 = SuperLU_timer_();
    int_t ksupc = SuperSize(k);
    cublasHandle_t cubHandle = A_gpu.cuHandles[offset];
    cudaStream_t cuStream = A_gpu.cuStreams[offset];
    if (iam == procIJ(k, k))
    {

        lPanelVec[g2lCol(k)].diagFactorPackDiagBlockGPU(k,
                                                        dFBufs[offset]->BlockUFactor, ksupc, // CPU pointers
                                                        dFBufs[offset]->BlockLFactor, ksupc, // CPU pointers
                                                        thresh, xsup, options, stat, info);
    }

    /*=======   Diagonal Broadcast          ======*/
    if (myrow == krow(k))
        MPI_Bcast((void *)dFBufs[offset]->BlockLFactor, ksupc * ksupc,
                  get_mpi_type<Ftype>(), kcol(k), (grid->rscp).comm);
    if (mycol == kcol(k))
        MPI_Bcast((void *)dFBufs[offset]->BlockUFactor, ksupc * ksupc,
                  get_mpi_type<Ftype>(), krow(k), (grid->cscp).comm);

    /*=======   Panel Update                ======*/
    if (myrow == krow(k))
    {

        cudaMemcpy(A_gpu.dFBufs[offset], dFBufs[offset]->BlockLFactor,
                   ksupc * ksupc * sizeof(Ftype), cudaMemcpyHostToDevice);
        uPanelVec[g2lRow(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[offset], ksupc);
        cudaStreamSynchronize(cuStream); // synchronize befpre broadcast
    }

    if (mycol == kcol(k))
    {
        cudaMemcpy(A_gpu.dFBufs[offset], dFBufs[offset]->BlockUFactor,
                   ksupc * ksupc * sizeof(Ftype), cudaMemcpyHostToDevice);
        lPanelVec[g2lCol(k)].panelSolveGPU(
            cubHandle, cuStream,
            ksupc, A_gpu.dFBufs[offset], ksupc);
        cudaStreamSynchronize(cuStream);
    }
    SCT->tDiagFactorPanelSolve += (SuperLU_timer_() - t0);

    return 0;
}

template <typename Ftype>
int_t xLUstruct_t<Ftype>::dPanelBcastGPU(int_t k, int_t offset)
{
    double t0 = SuperLU_timer_();
    /*=======   Panel Broadcast             ======*/
    // upanel_t k_upanel(UidxRecvBufs[offset], UvalRecvBufs[offset],
    //                   A_gpu.UidxRecvBufs[offset], A_gpu.UvalRecvBufs[offset]);
    // lpanel_t k_lpanel(LidxRecvBufs[offset], LvalRecvBufs[offset],
    //                   A_gpu.LidxRecvBufs[offset], A_gpu.LvalRecvBufs[offset]);
    // if (myrow == krow(k))
    // {
    //     k_upanel = uPanelVec[g2lRow(k)];
    // }
    // if (mycol == kcol(k))
    //     k_lpanel = lPanelVec[g2lCol(k)];
    xupanel_t<Ftype> k_upanel = getKUpanel(k, offset);
    xlpanel_t<Ftype> k_lpanel = getKLpanel(k, offset);

    if (UidxSendCounts[k] > 0)
    {
        // assuming GPU direct is available
        MPI_Bcast(k_upanel.gpuPanel.index, UidxSendCounts[k], mpi_int_t, krow(k), grid3d->cscp.comm);
        MPI_Bcast(k_upanel.gpuPanel.val, UvalSendCounts[k], get_mpi_type<Ftype>(), krow(k), grid3d->cscp.comm);
        // copy the index to cpu
        gpuErrchk(cudaMemcpy(k_upanel.index, k_upanel.gpuPanel.index,
                             sizeof(int_t) * UidxSendCounts[k], cudaMemcpyDeviceToHost));
    }

    if (LidxSendCounts[k] > 0)
    {
        MPI_Bcast(k_lpanel.gpuPanel.index, LidxSendCounts[k], mpi_int_t, kcol(k), grid3d->rscp.comm);
        MPI_Bcast(k_lpanel.gpuPanel.val, LvalSendCounts[k], get_mpi_type<Ftype>(), kcol(k), grid3d->rscp.comm);
        gpuErrchk(cudaMemcpy(k_lpanel.index, k_lpanel.gpuPanel.index,
                             sizeof(int_t) * LidxSendCounts[k], cudaMemcpyDeviceToHost));
    }
    SCT->tPanelBcast += (SuperLU_timer_() - t0);
    return 0;
}

template <typename Ftype>
int_t xLUstruct_t<Ftype>::dsparseTreeFactorGPU(
    sForest_t *sforest,
    diagFactBufs_type<Ftype> **dFBufs, // size maxEtree level
    gEtreeInfo_t *gEtreeInfo, // global etree info

    int tag_ub)
{
    int_t nnodes = sforest->nNodes; // number of nodes in the tree
    if (nnodes < 1)
    {
        return 1;
    }

    int_t *perm_c_supno = sforest->nodeList; // list of nodes in the order of factorization
    treeTopoInfo_t *treeTopoInfo = &sforest->topoInfo;
    int_t *myIperm = treeTopoInfo->myIperm;
    int_t maxTopoLevel = treeTopoInfo->numLvl;
    int_t *eTreeTopLims = treeTopoInfo->eTreeTopLims;

    /*main loop over all the levels*/
    int_t numLA = SUPERLU_MIN(A_gpu.numCudaStreams, getNumLookAhead(options));

#if (DEBUGlevel >= 1)
    CHECK_MALLOC(grid3d->iam, "Enter dsparseTreeFactorGPU()");

    printf("Using New code V100 with GPU acceleration\n");
    fflush(stdout);
    printf(". lookahead numLA %d\n", numLA);
    fflush(stdout);
#endif
    // start the pipeline.  Sherry: need to free these 3 arrays
    int *donePanelBcast = int32Malloc_dist(nnodes);
    int *donePanelSolve = int32Malloc_dist(nnodes);
    int *localNumChildrenLeft = int32Malloc_dist(nnodes);

    // TODO: not needed, remove after testing
    for (int_t i = 0; i < nnodes; i++)
    {
        donePanelBcast[i] = 0;
        donePanelSolve[i] = 0;
        localNumChildrenLeft[i] = 0;
    }

    for (int_t k0 = 0; k0 < nnodes; k0++)
    {
        int_t k = perm_c_supno[k0];
        int_t k_parent = gEtreeInfo->setree[k];
        int_t ik = myIperm[k_parent];
        if (ik > -1 && ik < nnodes)
            localNumChildrenLeft[ik]++;
    }

    // start the pipeline
    int_t topoLvl = 0;
    int_t k_st = eTreeTopLims[topoLvl];
    int_t k_end = eTreeTopLims[topoLvl + 1];

    // TODO: make this asynchronous
    for (int_t k0 = k_st; k0 < k_end; k0++)
    {
        int_t k = perm_c_supno[k0];
        int_t offset = 0;
        // dDiagFactorPanelSolveGPU(k, offset, dFBufs);
        dDFactPSolveGPU(k, offset, dFBufs);
        donePanelSolve[k0] = 1;
    }

    // TODO: its really the panels that needs to be doubled
    //  everything else can remain as it is
    int_t winSize = SUPERLU_MIN(numLA / 2, eTreeTopLims[1]);

    // printf(". lookahead winSize %d\n", winSize);
#if ( PRNTlevel >= 1 )    
    printf(". lookahead winSize %" PRIdMAX "\n", static_cast<intmax_t>(winSize));
    fflush(stdout);
#endif
    
    for (int k0 = k_st; k0 < winSize; ++k0)
    {
        int_t k = perm_c_supno[k0];
        int_t offset = k0 % numLA;
        if (!donePanelBcast[k0])
        {
            dPanelBcastGPU(k, offset);
            donePanelBcast[k0] = 1;
        }
    } /*for (int k0 = k_st; k0 < SUPERLU_MIN(k_end, k_st + numLA); ++k0)*/

    int_t k1 = 0;
    int_t winParity = 0;
    int_t halfWin = numLA / 2;
    while (k1 < nnodes)
    {
        for (int_t k0 = k1; k0 < SUPERLU_MIN(nnodes, k1 + winSize); ++k0)
        {
            int_t k = perm_c_supno[k0];
            int_t offset = getBufferOffset(k0, k1, winSize, winParity, halfWin);
            xupanel_t<Ftype> k_upanel = getKUpanel(k, offset);
            xlpanel_t<Ftype> k_lpanel = getKLpanel(k, offset);
            int_t k_parent = gEtreeInfo->setree[k];
            /* L o o k   A h e a d   P a n e l   U p d a t e */
            if (UidxSendCounts[k] > 0 && LidxSendCounts[k] > 0)
                lookAheadUpdateGPU(offset, k, k_parent, k_lpanel, k_upanel);
        }

        for (int_t k0 = k1; k0 < SUPERLU_MIN(nnodes, k1 + winSize); ++k0)
        {
            // int_t k = perm_c_supno[k0];
            int_t offset = getBufferOffset(k0, k1, winSize, winParity, halfWin);
            SyncLookAheadUpdate(offset);
        }

        for (int_t k0 = k1; k0 < SUPERLU_MIN(nnodes, k1 + winSize); ++k0)
        {
            int_t k = perm_c_supno[k0];
            int_t offset = getBufferOffset(k0, k1, winSize, winParity, halfWin);
            xupanel_t<Ftype> k_upanel = getKUpanel(k, offset);
            xlpanel_t<Ftype> k_lpanel = getKLpanel(k, offset);
            int_t k_parent = gEtreeInfo->setree[k];
            /* Look Ahead Panel Solve */
            if (k_parent < nsupers)
            {
                int_t k0_parent = myIperm[k_parent];
                if (k0_parent > 0 && k0_parent < nnodes)
                {
                    localNumChildrenLeft[k0_parent]--;
                    if (topoLvl < maxTopoLevel - 1 && !localNumChildrenLeft[k0_parent])
                    {
#if (PRNTlevel >= 2)
                        printf("parent %d of node %d during second phase\n", k0_parent, k0);
#endif
                        int_t dOffset = 0; // this is wrong
                        // dDiagFactorPanelSolveGPU(k_parent, dOffset,dFBufs);
                        dDFactPSolveGPU(k_parent, dOffset, dFBufs);
                        donePanelSolve[k0_parent] = 1;
                    }
                }
            }

            /*proceed with remaining SchurComplement update */
            if (UidxSendCounts[k] > 0 && LidxSendCounts[k] > 0)
                dSchurCompUpdateExcludeOneGPU(offset, k, k_parent, k_lpanel, k_upanel);
        }

        int_t k1_next = k1 + winSize;
        int_t oldWinSize = winSize;
        for (int_t k0_next = k1_next; k0_next < SUPERLU_MIN(nnodes, k1_next + winSize); ++k0_next)
        {
            int k_next = perm_c_supno[k0_next];
            if (!localNumChildrenLeft[k0_next])
            {
                // int offset_next = (k0_next-k1_next)%winSize;
                // if(!(winParity%2))
                //     offset_next += halfWin;

                int_t offset_next = getBufferOffset(k0_next, k1_next, winSize, winParity + 1, halfWin);
                dPanelBcastGPU(k_next, offset_next);
                donePanelBcast[k0_next] = 1;
                // printf("Trying  %d on offset %d\n", k0_next, offset_next);
            }
            else
            {
                winSize = k0_next - k1_next;
                break;
            }
        }

        for (int_t k0 = k1; k0 < SUPERLU_MIN(nnodes, k1 + oldWinSize); ++k0)
        {
            int_t k = perm_c_supno[k0];
            // int_t offset = (k0-k1)%oldWinSize;
            // if(winParity%2)
            //     offset+= halfWin;
            int_t offset = getBufferOffset(k0, k1, oldWinSize, winParity, halfWin);
            // printf("Syncing stream %d on offset %d\n", k0, offset);
            if (UidxSendCounts[k] > 0 && LidxSendCounts[k] > 0)
                gpuErrchk(cudaStreamSynchronize(A_gpu.cuStreams[offset]));
        }

        k1 = k1_next;
        winParity++;
    }

#if 0
    
    for (int_t topoLvl = 0; topoLvl < maxTopoLevel; ++topoLvl)
    {
        /* code */
        int_t k_st = eTreeTopLims[topoLvl];
        int_t k_end = eTreeTopLims[topoLvl + 1];
        for (int_t k0 = k_st; k0 < k_end; ++k0)
        {
            int_t k = perm_c_supno[k0];

            int_t ksupc = SuperSize(k);
            cublasHandle_t cubHandle = A_gpu.cuHandles[0];
            cudaStream_t cuStream = A_gpu.cuStreams[0];
            dDiagFactorPanelSolveGPU(k, 0, dFBufs);
            /*=======   Panel Broadcast             ======*/
            // panelBcastGPU(k, 0);
            int_t offset = k0%numLA;
            dPanelBcastGPU(k, offset);

            /*=======   Schurcomplement Update      ======*/
            upanel_t k_upanel(UidxRecvBufs[offset], UvalRecvBufs[offset],
                              A_gpu.UidxRecvBufs[offset], A_gpu.UvalRecvBufs[offset]);
            lpanel_t k_lpanel(LidxRecvBufs[offset], LvalRecvBufs[offset],
                              A_gpu.LidxRecvBufs[offset], A_gpu.LvalRecvBufs[offset]);
            if (myrow == krow(k))
            {
                k_upanel = uPanelVec[g2lRow(k)];
            }
            if (mycol == kcol(k))
                k_lpanel = lPanelVec[g2lCol(k)];

            if (UidxSendCounts[k] > 0 && LidxSendCounts[k] > 0)
            {
                int streamId = 0;

#if 0

                dSchurComplementUpdateGPU(
                    streamId,
                    k, k_lpanel, k_upanel);
#else
                int_t k_parent = gEtreeInfo->setree[k];
                lookAheadUpdateGPU(
                    streamId,
                    k, k_parent, k_lpanel, k_upanel);
                dSchurCompUpdateExcludeOneGPU(
                    streamId,
                    k, k_parent, k_lpanel, k_upanel);

#endif

            }
        } /*for k0= k_st:k_end */
    } /*for topoLvl = 0:maxTopoLevel*/

#endif /* match  #if 0 at line 562 before "for (int_t topoLvl = 0; topoLvl < maxTopoLevel; ++topoLvl)" */

    /* Sherry added 2/1/23 */
    SUPERLU_FREE(donePanelBcast);
    SUPERLU_FREE(donePanelSolve);
    SUPERLU_FREE(localNumChildrenLeft);

#if (DEBUGlevel >= 1)
    CHECK_MALLOC(grid3d->iam, "Exit dsparseTreeFactorGPU()");
#endif

    return 0;
} /* dsparseTreeFactorGPU */





//////////////////////////////////////////////////////////////////////////////////////////////////////////


// TODO: needs to be merged as a single factorization function
template <typename Ftype>
int_t xLUstruct_t<Ftype>::dsparseTreeFactorGPUBaseline(
    sForest_t *sforest,
    diagFactBufs_type<Ftype> **dFBufs, // size maxEtree level
    gEtreeInfo_t *gEtreeInfo, // global etree info

    int tag_ub)
{
    int_t nnodes = sforest->nNodes; // number of nodes in the tree
    if (nnodes < 1)
    {
        return 1;
    }

    printf("Using New code V100 with GPU acceleration\n");
#if (DEBUGlevel >= 1)
    CHECK_MALLOC(grid3d->iam, "Enter dsparseTreeFactorGPUBaseline()");
#endif

    int_t *perm_c_supno = sforest->nodeList; // list of nodes in the order of factorization
    treeTopoInfo_t *treeTopoInfo = &sforest->topoInfo;
    int_t *myIperm = treeTopoInfo->myIperm;
    int_t maxTopoLevel = treeTopoInfo->numLvl;
    int_t *eTreeTopLims = treeTopoInfo->eTreeTopLims;

    /*main loop over all the levels*/
    int_t numLA = getNumLookAhead(options);

    // start the pipeline

    for (int_t topoLvl = 0; topoLvl < maxTopoLevel; ++topoLvl)
    {
        /* code */
        int_t k_st = eTreeTopLims[topoLvl];
        int_t k_end = eTreeTopLims[topoLvl + 1];
        for (int_t k0 = k_st; k0 < k_end; ++k0)
        {
            int_t k = perm_c_supno[k0];
            int_t offset = k0 - k_st;
            int_t ksupc = SuperSize(k);
            cublasHandle_t cubHandle = A_gpu.cuHandles[0];
            cudaStream_t cuStream = A_gpu.cuStreams[0];
            /*=======   Diagonal Factorization      ======*/
            if (iam == procIJ(k, k))
            {
// #define NDEBUG
#ifndef NDEBUG
                lPanelVec[g2lCol(k)].checkGPU();
                lPanelVec[g2lCol(k)].diagFactor(k, dFBufs[offset]->BlockUFactor, ksupc,
                                                thresh, xsup, options, stat, info);
                lPanelVec[g2lCol(k)].packDiagBlock(dFBufs[offset]->BlockLFactor, ksupc);
#endif
                lPanelVec[g2lCol(k)].diagFactorPackDiagBlockGPU(k,
                                                                dFBufs[offset]->BlockUFactor, ksupc, // CPU pointers
                                                                dFBufs[offset]->BlockLFactor, ksupc, // CPU pointers
                                                                thresh, xsup, options, stat, info);
// cudaStreamSynchronize(cuStream);
#ifndef NDEBUG
                cudaStreamSynchronize(cuStream);
                lPanelVec[g2lCol(k)].checkGPU();
#endif
            }

            /*=======   Diagonal Broadcast          ======*/
            if (myrow == krow(k))
                MPI_Bcast((void *)dFBufs[offset]->BlockLFactor, ksupc * ksupc,
                          get_mpi_type<Ftype>(), kcol(k), (grid->rscp).comm);
            if (mycol == kcol(k))
                MPI_Bcast((void *)dFBufs[offset]->BlockUFactor, ksupc * ksupc,
                          get_mpi_type<Ftype>(), krow(k), (grid->cscp).comm);

            /*=======   Panel Update                ======*/
            if (myrow == krow(k))
            {
#ifndef NDEBUG
                uPanelVec[g2lRow(k)].checkGPU();
#endif
                cudaMemcpy(A_gpu.dFBufs[0], dFBufs[offset]->BlockLFactor,
                           ksupc * ksupc * sizeof(Ftype), cudaMemcpyHostToDevice);
                uPanelVec[g2lRow(k)].panelSolveGPU(
                    cubHandle, cuStream,
                    ksupc, A_gpu.dFBufs[0], ksupc);
                cudaStreamSynchronize(cuStream); // synchronize befpre broadcast
#ifndef NDEBUG
                uPanelVec[g2lRow(k)].panelSolve(ksupc, dFBufs[offset]->BlockLFactor, ksupc);
                cudaStreamSynchronize(cuStream);
                uPanelVec[g2lRow(k)].checkGPU();
#endif
            }

            if (mycol == kcol(k))
            {
                cudaMemcpy(A_gpu.dFBufs[0], dFBufs[offset]->BlockUFactor,
                           ksupc * ksupc * sizeof(Ftype), cudaMemcpyHostToDevice);
                lPanelVec[g2lCol(k)].panelSolveGPU(
                    cubHandle, cuStream,
                    ksupc, A_gpu.dFBufs[0], ksupc);
                cudaStreamSynchronize(cuStream);
#ifndef NDEBUG
                lPanelVec[g2lCol(k)].panelSolve(ksupc, dFBufs[offset]->BlockUFactor, ksupc);
                cudaStreamSynchronize(cuStream);
                lPanelVec[g2lCol(k)].checkGPU();
#endif
            }

            /*=======   Panel Broadcast             ======*/
            xupanel_t<Ftype> k_upanel(UidxRecvBufs[0], UvalRecvBufs[0],
                              A_gpu.UidxRecvBufs[0], A_gpu.UvalRecvBufs[0]);
            xlpanel_t<Ftype> k_lpanel(LidxRecvBufs[0], LvalRecvBufs[0],
                              A_gpu.LidxRecvBufs[0], A_gpu.LvalRecvBufs[0]);
            if (myrow == krow(k))
            {
                k_upanel = uPanelVec[g2lRow(k)];
            }
            if (mycol == kcol(k))
                k_lpanel = lPanelVec[g2lCol(k)];

            if (UidxSendCounts[k] > 0)
            {
                // assuming GPU direct is available
                MPI_Bcast(k_upanel.gpuPanel.index, UidxSendCounts[k], mpi_int_t, krow(k), grid3d->cscp.comm);
                MPI_Bcast(k_upanel.gpuPanel.val, UvalSendCounts[k], get_mpi_type<Ftype>(), krow(k), grid3d->cscp.comm);
                // copy the index to cpu
                cudaMemcpy(k_upanel.index, k_upanel.gpuPanel.index,
                           sizeof(int_t) * UidxSendCounts[k], cudaMemcpyDeviceToHost);

#ifndef NDEBUG
                MPI_Bcast(k_upanel.index, UidxSendCounts[k], mpi_int_t, krow(k), grid3d->cscp.comm);
                MPI_Bcast(k_upanel.val, UvalSendCounts[k], get_mpi_type<Ftype>(), krow(k), grid3d->cscp.comm);
#endif
            }

            if (LidxSendCounts[k] > 0)
            {
                MPI_Bcast(k_lpanel.gpuPanel.index, LidxSendCounts[k], mpi_int_t, kcol(k), grid3d->rscp.comm);
                MPI_Bcast(k_lpanel.gpuPanel.val, LvalSendCounts[k], get_mpi_type<Ftype>(), kcol(k), grid3d->rscp.comm);
                cudaMemcpy(k_lpanel.index, k_lpanel.gpuPanel.index,
                           sizeof(int_t) * LidxSendCounts[k], cudaMemcpyDeviceToHost);

#ifndef NDEBUG
                MPI_Bcast(k_lpanel.index, LidxSendCounts[k], mpi_int_t, kcol(k), grid3d->rscp.comm);
                MPI_Bcast(k_lpanel.val, LvalSendCounts[k], get_mpi_type<Ftype>(), kcol(k), grid3d->rscp.comm);
#endif
            }

            /*=======   Schurcomplement Update      ======*/

            if (UidxSendCounts[k] > 0 && LidxSendCounts[k] > 0)
            {
                // k_upanel.checkCorrectness();
                int streamId = 0;
#ifndef NDEBUG
                checkGPU();
#endif
                dSchurComplementUpdateGPU(
                    streamId,
                    k, k_lpanel, k_upanel);
// cudaStreamSynchronize(cuStream); // there is sync inside the kernel
#ifndef NDEBUG
                dSchurComplementUpdate(k, k_lpanel, k_upanel);
                cudaStreamSynchronize(cuStream);
                checkGPU();
#endif
            }
            // MPI_Barrier(grid3d->comm);

        } /*for k0= k_st:k_end */

    } /*for topoLvl = 0:maxTopoLevel*/

#if (DEBUGlevel >= 1)
    CHECK_MALLOC(grid3d->iam, "Exit dsparseTreeFactorGPUBaseline()");
#endif

    return 0;
} /* dsparseTreeFactorGPUBaseline */

#endif  /* match #if HAVE_CUDA  */

