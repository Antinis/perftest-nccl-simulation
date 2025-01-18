#!/bin/bash

mpirun --mca btl_tcp_if_include eno1 \
-np 4 \
--host gpu15:2,gpu16:2 \
-x I_MPI_OFI_PROVIDER=tcp \
-x ROOT=/home/ubuntu/07.drivers-2.1/user/udrv \
-x BUILD_DIR=/home/ubuntu/07.drivers-2.1/user/udrv/build \
-x BIN=/home/ubuntu/07.drivers-2.1/user/udrv/build/bin \
-x LIB=/home/ubuntu/07.drivers-2.1/user/udrv/build/lib \
-x PYTHONPATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/python:/home/ubuntu/07.drivers-2.1/user/udrv/build/python/pyverbs \
-x LIBRARY_PATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/lib:$LIBRARY_PATH \
-x LD_LIBRARY_PATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/lib:/usr/local/openmpi/lib:/usr/local/cuda-12.6/lib64/:/home/ubuntu/nccl-2.19.3-1/build/lib:$LD_LIBRARY_PATH \
-x PATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/bin:$PATH \
-x C_INCLUDE_PATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/include:$C_INCLUDE_PATH \
-x CPLUS_INCLUDE_PATH=/home/ubuntu/07.drivers-2.1/user/udrv/build/include:$CPLUS_INCLUDE_PATH \
/home/ubuntu/perftest-4.1-0.2/ib_write_bw 65536
