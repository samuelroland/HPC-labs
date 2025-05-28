#!/bin/bash
# Provided commands to setup FDs and run perf
# MUST BE RUN AS ROOT !!!
rm -f perf_fifo.ctl perf_fifo.ack
mkfifo perf_fifo.ctl
mkfifo perf_fifo.ack
exec {perf_ctl_fd}<>perf_fifo.ctl
exec {perf_ack_fd}<>perf_fifo.ack
export PERF_CTL_FD=${perf_ctl_fd}
export PERF_ACK_FD=${perf_ack_fd}

perf stat \
    --json \
    -e power/energy-pkg/ \
    --delay=-1 \
    --control fd:${perf_ctl_fd},${perf_ack_fd} \
    -- ./build/floatsparty_"$1"
