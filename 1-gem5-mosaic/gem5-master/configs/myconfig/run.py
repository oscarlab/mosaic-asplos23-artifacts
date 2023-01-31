# -*- coding: utf-8 -*-
# Copyright (c) 2016 Jason Lowe-Power
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Authors: Jason Lowe-Power

import os
import sys
import time

import m5
from m5.objects import *

sys.path.append('configs/common/') # For the next line...

from system import MySystem

if __name__ == "__m5_main__":

    # create the system we are going to simulate
    system = MySystem(sys.argv[1],sys.argv[2],"atomic",1)

    system.work_begin_exit_count = 1
    system.work_end_exit_count = 1

    # set up the root SimObject and start the simulation
    root = Root(full_system = True, system = system)
    if system.getHostParallel():
         root.sim_quantum = int(1e6)

    # instantiate all of the objects we've created above
    m5.instantiate()

    globalStart = time.time()

    # Keep running until we are done.
    print("Running the simulation")
    exit_event = m5.simulate()
    print('Exiting @ tick %i because %s' % (m5.curTick(),exit_event.getCause()))

    # While there is still something to do in the guest keep executing.
    # This is needed since we exit for the ROI begin/end
    foundROI = False
    while exit_event.getCause() != "m5_exit instruction encountered":
        # If the user pressed ctrl-c on the host, then we really should exit
        if exit_event.getCause() == "user interrupt received":
            print("User interrupt. Exiting")
            break

        print("Exited because", exit_event.getCause())

        if exit_event.getCause() == "work started count reach":
            start_tick = m5.curTick()
            start_insts = system.totalInsts()
            foundROI = True
        elif exit_event.getCause() == "work items exit count reached":
            end_tick = m5.curTick()
            end_insts = system.totalInsts()

        print("Continuing")
        exit_event = m5.simulate()

    print("Performance statistics")

    print("Ran a total of", m5.curTick()/1e12, "simulated seconds")
    print("Total wallclock time: %.2fs, %.2f min" % \
                (time.time()-globalStart, (time.time()-globalStart)/60))

    if foundROI:
        print("Simulated time in ROI: %.2fs" % ((end_tick-start_tick)/1e12))
        print("Instructions executed in ROI: %d" % ((end_insts-start_insts)))
