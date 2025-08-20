#!/bin/bash

# cmake build for SuperLU_dist on NERSC Perlmutter
#
# Last update: July 21, 2022
# Perlmutter is not in production and the software environment changes rapidly.
# Expect this file to be frequently updated
#
# This build script targets Perlmutter Slingshot 11 GPU compute nodes
#
# When requesting GPU compute nodes using salloc, will have to add the _ss11 suffix to the QOS
# For example, if previously requested 1 node using salloc as follows
# salloc -C gpu -N 1 -G 4 -t 30 -A m3894 -q regular
# will now need:
# salloc -C gpu -N 1 -G 4 -t 30 -A m3894 -q regular_ss11
# will also need to issue the 5 module load commands below and the
# two "export LD_LIBRARY_PATH" commands below in the shell after
# receiving node allocation or in scripts that will run on the nodes
#
# For sbatch scripts, if previously had
# #SBATCH -q regular
# will now need
# #SBATCH -q regular_ss11
# will also need to include the 5 module load commands below and the
# two "export LD_LIBRARY_PATH" commands below in the batch script
#
# Note: you may have to specify your own parmetis/metis libraries

# module load cpe/23.03
module load PrgEnv-intel
# module load gcc/11.2.0
module load cmake
# module load cudatoolkit
# avoid bug in cray-libsci/21.08.1.2
# module load cray-libsci/22.11.1.2
module load cray-libsci
# module use /global/common/software/nersc/pe/modulefiles/latest
# module load nvshmem/2.11.0
# export MAGMA_ROOT=/global/cfs/cdirs/m2957/lib/magma_master
# avoid bug in cudatoolkit
# export LD_LIBRARY_PATH=${LD_LIBRARY_PATH//\/usr\/local\/cuda-12.4\/compat:/}
# export LD_LIBRARY_PATH=${LD_LIBRARY_PATH//\/usr\/local\/cuda-11.7\/compat:/}

# NVSHMEM_HOME=/global/cfs/cdirs/m2957/lib/lib/PrgEnv-gnu/nvshmem_src_2.8.0-3/build/
#NVSHMEM_HOME=${CRAY_NVIDIA_PREFIX}/comm_libs/nvshmem/
cmake .. \
  -DCMAKE_C_FLAGS="-O2 -std=c11 -DPRNTlevel=0 -DPROFlevel=0 -DDEBUGlevel=0 -DAdd_" \
  -DCMAKE_CXX_FLAGS="-O2 -std=c++11" \
  -DCMAKE_Fortran_FLAGS="-O2" \
  -DCMAKE_CXX_COMPILER=CC \
  -DCMAKE_C_COMPILER=cc \
  -DCMAKE_Fortran_COMPILER=ftn \
  -DXSDK_ENABLE_Fortran=ON \
  -DTPL_ENABLE_INTERNAL_BLASLIB=OFF \
  -DTPL_ENABLE_LAPACKLIB=ON \
  -DBUILD_SHARED_LIBS=ON \
  -DTPL_ENABLE_CUDALIB=OFF \
  -DCMAKE_CUDA_FLAGS="-ccbin=CC" \
  -DCMAKE_CUDA_ARCHITECTURES=80 \
  -DCMAKE_INSTALL_PREFIX=. \
  -DCMAKE_INSTALL_LIBDIR=./lib \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTPL_BLAS_LIBRARIES=$CRAY_LIBSCI_PREFIX/lib/libsci_intel_mp.so \
  -DTPL_LAPACK_LIBRARIES=$CRAY_LIBSCI_PREFIX/lib/libsci_intel_mp.so \
  -DTPL_PARMETIS_INCLUDE_DIRS="/global/cfs/cdirs/m2957/lib/lib/PrgEnv-intel/parmetis-4.0.3/include;/global/cfs/cdirs/m2957/lib/lib/PrgEnv-intel/parmetis-4.0.3/metis/include" \
  -DTPL_PARMETIS_LIBRARIES="/global/cfs/cdirs/m2957/lib/lib/PrgEnv-intel/parmetis-4.0.3/build/Linux-x86_64/libparmetis/libparmetis.so;/global/cfs/cdirs/m2957/lib/lib/PrgEnv-intel/parmetis-4.0.3/build/Linux-x86_64/libmetis/libmetis.so" \
  -DTPL_ENABLE_COMBBLASLIB=OFF \
  -DCMAKE_VERBOSE_MAKEFILE:BOOL=ON \
  -DMPIEXEC_NUMPROC_FLAG=-n \
  -DMPIEXEC_EXECUTABLE=/usr/bin/srun \
  -DMPIEXEC_MAX_NUMPROCS=16

make pddrive -j16
make pddrive3d -j16
make pzdrive3d -j16
make f_pddrive
make pzdrive3d_qcd 

## -DTPL_BLAS_LIBRARIES=/global/cfs/cdirs/m3894/ptlin/tpl/amd_blis/install/amd_blis-20211021-n9-gcc9.3.0/lib/libblis.a \


