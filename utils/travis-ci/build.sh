#!/bin/bash

BASE=`pwd`

# build name resolver
cd $BASE/name-resolver
cmake .
make -j

# build orchestrator
cd $BASE/orchestrator
cmake -DENABLE_KVM=$KVM -DENABLE_DOCKER=$DOCKER -DENABLE_NATIVE=$NATIVE -DENABLE_DPDK_PROCESSES=$DPDK -DVSWITCH_IMPLEMENTATION=$VSWITCH -DENABLE_DOUBLE_DECKER_CONNECTION=$DD -DENABLE_RESOURCE_MANAGER=$DD .
make -j
