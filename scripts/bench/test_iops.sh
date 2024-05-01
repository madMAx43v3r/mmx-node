#!/bin/bash

sudo fio --filename=$1 --direct=0 --rw=randread --bs=48 --ioengine=libaio --iodepth=1024 --runtime=30 --numjobs=16 --time_based --group_reporting --name=iops-test-job --eta-newline=1 --readonly
