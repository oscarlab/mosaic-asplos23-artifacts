# Copyright (c) 2007 The Hewlett-Packard Development Company
# All rights reserved.
#
# The license below extends only to copyright in the software and shall
# not be construed as granting a license to any other intellectual
# property including but not limited to intellectual property relating
# to a hardware implementation of the functionality of the software
# licensed hereunder.  You may use the software subject to the license
# terms below provided that you ensure that this notice is replicated
# unmodified and in its entirety in all distributions of the software,
# modified or unmodified, in source code or in binary form.
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
# Authors: Gabe Black

from m5.params import *
from m5.proxy import *

from BaseTLB import BaseTLB
from MemObject import MemObject

class X86PagetableWalker(MemObject):
    type = 'X86PagetableWalker'
    cxx_class = 'X86ISA::Walker'
    cxx_header = 'arch/x86/pagetable_walker.hh'
    port = MasterPort("Port for the hardware table walker")
    system = Param.System(Parent.any, "system object")

    #params for iceberg simulation
    toc_size = Param.Int("8","number of entries in TOC")

    #PTW cache settings
    usePTWC        = Param.Bool(False,"use PTW cache")

    l4_num_entries = Param.Int(128, "num entries in l4 ptw cache")
    l4_assoc       = Param.Int(1, "assoc in l4 ptw cache")

    l3_num_entries = Param.Int(128, "num entries in l3 ptw cache")
    l3_assoc       = Param.Int(1, "assoc in l3 ptw cache")

    l2_num_entries = Param.Int(128, "num entries in l2 ptw cache")
    l2_assoc       = Param.Int(1, "assoc in l2 ptw cache")

    num_squash_per_cycle = Param.Unsigned(4,
            "Number of outstanding walks that can be squashed per cycle")

class X86TLB(BaseTLB):
    type = 'X86TLB'
    cxx_class = 'X86ISA::TLB'
    cxx_header = 'arch/x86/tlb.hh'
    sizeL1 = Param.Unsigned(64, "TLB size L1")
    sizeL2 = Param.Unsigned(0, "TLB size L2")
    setAssocL1 = Param.Unsigned(0, "TLB set associativity L1")
    #iceberg TLB settings
    simulateIcebergTLB = Param.Bool(False, "simulate iceberg TLB")
    toc_size = Param.Int("8","number of entries in TOC")
    walker = Param.X86PagetableWalker(\
            X86PagetableWalker(), "page table walker")
