#!/bin/bash
#SBATCH -A m2957
#SBATCH -C gpu
#SBATCH -q regular
#SBATCH -t 1:00:00
#SBATCH -N 64
#SBATCH --ntasks-per-node=4
#SBATCH --gpus 256
#SBATCH --mail-user=liuyangzhuan@lbl.gov
#SBATCH --mail-type=BEGIN
#SBATCH -e ./tmp.err
#
#modules:
module load PrgEnv-gnu
module load cmake
module load cudatoolkit
module load cray-libsci
module load python/3.11
ulimit -s unlimited
#MPI settings:
################################################# 
export MPICH_GPU_SUPPORT_ENABLED=1
export CRAY_ACCEL_TARGET=nvidia80
echo MPICH_GPU_SUPPORT_ENABLED=$MPICH_GPU_SUPPORT_ENABLED
export LD_LIBRARY_PATH=${CRAY_LD_LIBRARY_PATH}:$LD_LIBRARY_PATH
NROW=16   # number of MPI row processes 
NCOL=16   # number of MPI column processes 
NPZ=1    # number of 2D process grids  
NTH=16 # number of OMP threads
################################################# 


#SUPERLU settings:
################################################# 
export SUPERLU_PYTHON_LIB_PATH=../build/PYTHON/
export SUPERLU_LBS=GD  
export SUPERLU_ACC_OFFLOAD=1 # whether to do CPU or GPU numerical factorization
export GPU3DVERSION=0 # whether to do use the latest C++ numerical factorization 
export SUPERLU_ACC_SOLVE=0 # whether to do CPU or GPU triangular solve
export SUPERLU_BIND_MPI_GPU=1 # assign GPU based on the MPI rank, assuming one MPI per GPU
export SUPERLU_MAXSUP=256 # max supernode size
export SUPERLU_RELAX=64  # upper bound for relaxed supernode size
export SUPERLU_MAX_BUFFER_SIZE=100000000 ## 500000000 # buffer size in words on GPU
export SUPERLU_NUM_LOOKAHEADS=2   ##4, must be at least 2, see 'lookahead winSize'
export SUPERLU_NUM_GPU_STREAMS=1
export SUPERLU_N_GEMM=6000 # FLOPS threshold divide workload between CPU and GPU
nmpipergpu=1
export SUPERLU_MPI_PROCESS_PER_GPU=$nmpipergpu # nmpipergpu>1 can better saturate GPU for some smaller matrices
################################################# 



## The following is NVSHMEM settings for multi-GPU trisolve 
################################################# 
# module load nvshmem/2.11.0
NVSHMEM_HOME=/global/cfs/cdirs/m2957/lib/lib/PrgEnv-gnu/nvshmem_src_2.8.0-3/build/
export NVSHMEM_USE_GDRCOPY=1
export NVSHMEM_MPI_SUPPORT=1
export MPI_HOME=${MPICH_DIR}
export NVSHMEM_LIBFABRIC_SUPPORT=1
export LIBFABRIC_HOME=/opt/cray/libfabric/1.20.1
export LD_LIBRARY_PATH=$NVSHMEM_HOME/lib:$LD_LIBRARY_PATH
export NVSHMEM_DISABLE_CUDA_VMM=1
export FI_CXI_OPTIMIZED_MRS=false
export NVSHMEM_BOOTSTRAP_TWO_STAGE=1
export NVSHMEM_BOOTSTRAP=MPI
export NVSHMEM_REMOTE_TRANSPORT=libfabric
################################################# 



## The following computes the total core and node numbers, as well as set up OMP envs 
################################################# 
CORES_PER_NODE=64
THREADS_PER_NODE=128
GPUS_PER_NODE=4
CORE_VAL=`expr $NCOL \* $NROW \* $NPZ`
NODE_VAL=`expr $CORE_VAL / $GPUS_PER_NODE`
MOD_VAL=`expr $CORE_VAL % $GPUS_PER_NODE`
if [[ $MOD_VAL -ne 0 ]]
then
  NODE_VAL=`expr $NODE_VAL + 1`
fi
NCORE_VAL_TOT=`expr $NROW \* $NCOL \* $NPZ `


OMP_NUM_THREADS=$NTH
TH_PER_RANK=`expr $NTH \* 2`
export OMP_NUM_THREADS=$NTH
export OMP_PLACES=threads
export OMP_PROC_BIND=spread
export SLURM_CPU_BIND="cores"
export MPICH_MAX_THREAD_SAFETY=multiple
################################################# 


# ############### use mpirun to call the python driver 
# srun -N 4 -n $NCORE_VAL_TOT  -c $TH_PER_RANK --cpu_bind=cores python -u ../PYTHON/pddrive.py -c $NCOL -r $NROW -d $NPZ -s 1 -q 5 -m 1 -p 0 -i 0 -b 0 | tee a.out_singlelaunch 


############### sequentially call the python driver pddrive_master.py, but parallelly launching the workers pddrive_worker.py 
# srun -N 4 -n $NCORE_VAL_TOT  -c $TH_PER_RANK --cpu_bind=cores python -u ../PYTHON/pddrive_worker.py -c $NCOL -r $NROW -d $NPZ -s 1 -q 5 -m 1 -p 0 -i 0 -b 0 | tee a.out_seperatelaunch_worker  &
# python -u ../PYTHON/pddrive_master.py | tee a.out_seperatelaunch_master 


srun -N 64 -n $NCORE_VAL_TOT  -c $TH_PER_RANK --cpu_bind=cores python -u ../PYTHON/pddrive_worker.py -c $NCOL -r $NROW -d $NPZ -s 1 -q 5 -m 1 -p 0 -i 0 -b 0 | tee a.out_seperatelaunch_worker  






