/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2007 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Gabe Black
 */

#include "arch/x86/pagetable_walker.hh"

#include <memory>
#include <string.h>

#include "arch/x86/pagetable.hh"
#include "arch/x86/tlb.hh"
#include "arch/x86/vtophys.hh"
#include "base/bitfield.hh"
#include "base/trie.hh"
#include "cpu/base.hh"
#include "cpu/thread_context.hh"
#include "debug/PageTableWalker.hh"
#include "mem/packet_access.hh"
#include "mem/request.hh"

namespace X86ISA {

Fault
Walker::start(ThreadContext * _tc, BaseTLB::Translation *_translation,
              RequestPtr _req, BaseTLB::Mode _mode, bool isWalkForDeleteinPTWC, bool isWalkForBuildTOC)
{
    // TODO: in timing mode, instead of blocking when there are other
    // outstanding requests, see if this request can be coalesced with
    // another one (i.e. either coalesce or start walk)
    WalkerState * newState = new WalkerState(this, _translation, _req);
    newState->initState(_tc, _mode, sys->isTimingMode());

    //handling for special cases
    newState->isWalkForDeleteinPTWC = isWalkForDeleteinPTWC;
    newState->isWalkForBuildTOC = isWalkForBuildTOC;

    if (!isWalkForDeleteinPTWC && !isWalkForBuildTOC) {
    	newState->walkStartTick = curTick();
    	totalPageWalks++;
    }

    if (currStates.size()) {
        assert(newState->isTiming());
        /*if (isWalkForBuildTOC)
            printf("walk in progress, so pushing to queue\n");*/
        DPRINTF(PageTableWalker, "Walks in progress: %d\n", currStates.size());
        currStates.push_back(newState);
        return NoFault;
    } else {
        /*if (isWalkForBuildTOC)
            printf("no walk in progress, so walking directly\n");*/
        currStates.push_back(newState);
        Fault fault = newState->startWalk();
        if (!newState->isTiming()) {
            currStates.pop_front();
            delete newState;
        }
        return fault;
    }
}


//function to build TOC
void
Walker::build_toc_bitmap(ThreadContext * _tc, 
               BaseTLB::Translation *translation,
               RequestPtr req, BaseTLB::Mode mode)
{

}

Fault
Walker::startFunctional(ThreadContext * _tc, Addr &addr, unsigned &logBytes,
              BaseTLB::Mode _mode)
{
    funcState.initState(_tc, _mode);
    return funcState.startFunctional(addr, logBytes);
}

bool
Walker::WalkerPort::recvTimingResp(PacketPtr pkt)
{
    return walker->recvTimingResp(pkt);
}

bool
Walker::recvTimingResp(PacketPtr pkt)
{
    WalkerSenderState * senderState =
        dynamic_cast<WalkerSenderState *>(pkt->popSenderState());
    WalkerState * senderWalk = senderState->senderWalk;
    bool walkComplete = senderWalk->recvPacket(pkt);
    delete senderState;
    if (walkComplete) {
        std::list<WalkerState *>::iterator iter;
        //walk to delete duplicate entries
        if (senderWalk->isWalkForBuildTOC) {
            Addr toc_target_address = senderWalk->req->getVaddr() & (~mask(12+toc_bits));
            for (iter = currStates.begin(); iter != currStates.end(); iter++) {
                WalkerState * walkerState = *(iter);
                Addr toc_current_address = walkerState->req->getVaddr() & (~mask(12+toc_bits));
                if (walkerState->isWalkForBuildTOC &&
                     walkerState != senderWalk &&
                     toc_current_address == toc_target_address) {
                    bool delayedResponse;
                    Fault fault = tlb->translate(walkerState->req, walkerState->tc, NULL, walkerState->mode,
                                                    delayedResponse, true,
                                                    true /*do iceberg tlb lookup as this is for toc building*/);
                    assert(!delayedResponse);
                	// Let the CPU continue.
                    walkerState->translation->finish(fault, walkerState->req, walkerState->tc, walkerState->mode);
                    iter = currStates.erase(iter);
                }
            }
        }
        for (iter = currStates.begin(); iter != currStates.end(); iter++) {
            WalkerState * walkerState = *(iter);
            if (walkerState == senderWalk) {
                iter = currStates.erase(iter);
                break;
            }
        }
        delete senderWalk;
        // Since we block requests when another is outstanding, we
        // need to check if there is a waiting request to be serviced
        if (currStates.size() && !startWalkWrapperEvent.scheduled())
        {
            // delay sending any new requests until we are finished
            // with the responses
            schedule(startWalkWrapperEvent, clockEdge());
        }
    }
    return true;
}

void
Walker::callSchedWalk(WalkerState *current)
{
	std::list<WalkerState *>::iterator iter;
    for (iter = currStates.begin(); iter != currStates.end(); iter++) {
		WalkerState * walkerState = *(iter);
		if (walkerState == current) {
			iter = currStates.erase(iter);
			break;
		}
	}
	if (currStates.size() && !startWalkWrapperEvent.scheduled())
	{
	    //printf("calling startWalkWrapperEvent from callschedwalk\n");
		// delay sending any new requests until we are finished
		// with the responses
		schedule(startWalkWrapperEvent, clockEdge());
	}
}

void
Walker::WalkerPort::recvReqRetry()
{
    walker->recvReqRetry();
}

void
Walker::recvReqRetry()
{
    std::list<WalkerState *>::iterator iter;
    for (iter = currStates.begin(); iter != currStates.end(); iter++) {
        WalkerState * walkerState = *(iter);
        if (walkerState->isRetrying()) {
            walkerState->retry();
        }
    }
}

bool Walker::sendTiming(WalkerState* sendingState, PacketPtr pkt)
{
    WalkerSenderState* walker_state = new WalkerSenderState(sendingState);
    pkt->pushSenderState(walker_state);
    if (port.sendTimingReq(pkt)) {
        return true;
    } else {
        // undo the adding of the sender state and delete it, as we
        // will do it again the next time we attempt to send it
        pkt->popSenderState();
        delete walker_state;
        return false;
    }

}

BaseMasterPort &
Walker::getMasterPort(const std::string &if_name, PortID idx)
{
    if (if_name == "port")
        return port;
    else
        return MemObject::getMasterPort(if_name, idx);
}

void
Walker::WalkerState::initState(ThreadContext * _tc,
        BaseTLB::Mode _mode, bool _isTiming)
{
    assert(state == Ready);
    started = false;
    tc = _tc;
    mode = _mode;
    timing = _isTiming;
}

bool
Walker::AddEntryToPWCache(VAddr vaddr, PageTableEntry EntryToAdd, PageTableEntry base)
{
	if(usePWC==true)
	{
		switch(LevelToCheck)
		{
			case 4://l4
				if(l4cache)
					l4cache->update(EntryToAdd, (int)vaddr.longl4, base);
				break;
			case 3://l3
				if(l3cache)
					l3cache->update(EntryToAdd, (int)vaddr.longl3, base);
				break;
			case 2://l2
				if(l2cache)
					l2cache->update(EntryToAdd, (int)vaddr.longl2, base);
				break;
			default:
				break;
		}
	}
	return true;
}

bool
Walker::RemoveEntryFromPWCache(VAddr vaddr, PageTableEntry base)
{
	if(usePWC==true)
	{
		switch(LevelToCheck)
		{
			case 4://l4
				if(l4cache)
					l4cache->remove((int)vaddr.longl4, base);
				break;
			case 3://l3
				if(l3cache)
					l3cache->remove((int)vaddr.longl3, base);
				break;
			case 2://l2
				if(l2cache)
					l2cache->remove((int)vaddr.longl2, base);
				break;
			default:
				break;
		}
	}
	return true;
}


PageTableEntry
Walker::GetCachedEntry(VAddr vaddr)
{
	PageTableEntry ReturnEntry = 0;
	if(usePWC==true)
	{
		switch(LevelToCheck)
		{
			case 4://l4
				if(l4cache)
					ReturnEntry = l4cache->getnextfromCache((int)vaddr.longl4, base);
				break;
			case 3://l3
				if(l3cache)
					ReturnEntry = l3cache->getnextfromCache((int)vaddr.longl3, base);
				break;
			case 2://l2
				if(l2cache)
					ReturnEntry = l2cache->getnextfromCache((int)vaddr.longl2, base);
				break;
			default:
				ReturnEntry = 0;
				break;
		}
	}
	return ReturnEntry;
}

bool
Walker::invalidateEntry(VAddr vaddr)
{
	if(usePWC==true)
	{
		switch(LevelToCheck)
		{
			case 4://l4
				if(l4cache)
					l4cache->invalidate((int)vaddr.longl4);
				break;
			case 3://l3
				if(l3cache)
					l3cache->invalidate((int)vaddr.longl3);
				break;
			case 2://l2
				if(l2cache)
					l2cache->invalidate((int)vaddr.longl2);
				break;
			default:
				break;
		}
	}
	return true;
}

bool
Walker::invalidateAllPWCforVAddr(VAddr vaddr)
{
	if(usePWC==true)
	{
		if(l4cache)
			l4cache->invalidate((int)vaddr.longl4);
		if(l3cache)
			l3cache->invalidate((int)vaddr.longl3);
		if(l2cache)
			l2cache->invalidate((int)vaddr.longl2);
	}
	
	return true;
}



bool
Walker::flushAllPWC()
{
	if(usePWC==true)
	{
		if(l4cache)
			l4cache->invalidateAll();
		if(l3cache)
			l3cache->invalidateAll();
		if(l2cache)
			l2cache->invalidateAll();
	}
	return true;
}

void
Walker::startWalkWrapper()
{
    unsigned num_squashed = 0;
    WalkerState *currState = currStates.front();

    while ((num_squashed < numSquashable) && currState &&
        currState->translation->squashed()) {
        printf("inside while loop\n");
        currStates.pop_front();
        num_squashed++;

        DPRINTF(PageTableWalker, "Squashing table walk for address %#x\n",
            currState->req->getVaddr());

        // finish the translation which will delete the translation object
        currState->translation->finish(
            std::make_shared<UnimpFault>("Squashed Inst"),
            currState->req, currState->tc, currState->mode);

        // delete the current request
        delete currState;

        // check the next translation request, if it exists
        if (currStates.size())
            currState = currStates.front();
        else
            currState = NULL;
    }

    if (currState && !currState->wasStarted())
    {
        currState->startWalk();
    }
}

Fault
Walker::WalkerState::startWalk()
{
    Fault fault = NoFault;
    assert(!started);
    started = true;

    walker->FoundInPWCache = false;

    Addr vaddr = req->getVaddr();
    //in case of building TOC, change address to first address of TOC
    if (isWalkForBuildTOC) {
        //printf("page table walk for toc start addr %012x\n",(unsigned int)vaddr);
        TOC_offset = 0;
        start_TOC_building = 0;
    }

    setupWalk(vaddr);

    if (timing) {
        nextState = state;
        state = Waiting;
        timingFault = NoFault;
        sendPackets();
    } else {
        do {
        	if(!walker->FoundInPWCache)
        	{
        		//not present in cache
        		//should read from memory and update cache
            	walker->port.sendAtomic(read);

            	//add to cache
            	walker->AddEntryToPWCache(req->getVaddr(),read->get<uint64_t>(),walker->base);
            }
            else
            {
            	//present in cache
            	//so update read
            	read->set<uint64_t>(walker->GetCachedEntry(req->getVaddr()));
            }
            PacketPtr write = NULL;
            walker->FoundInPWCache = false;
            fault = stepWalk(write);
            assert(fault == NoFault || read == NULL);
            state = nextState;
            nextState = Ready;
            if (write)
                walker->port.sendAtomic(write);
        } while (read);
        state = Ready;
        nextState = Waiting;
    }
    return fault;
}

Fault
Walker::WalkerState::startFunctional(Addr &addr, unsigned &logBytes)
{
    Fault fault = NoFault;
    assert(!started);
    started = true;
    setupWalk(addr);

    do {
        walker->port.sendFunctional(read);
        // On a functional access (page table lookup), writes should
        // not happen so this pointer is ignored after stepWalk
        PacketPtr write = NULL;
        fault = stepWalk(write);
        assert(fault == NoFault || read == NULL);
        state = nextState;
        nextState = Ready;
    } while (read);
    logBytes = entry.logBytes;
    addr = entry.paddr;

    return fault;
}

Fault
Walker::WalkerState::stepWalk(PacketPtr &write)
{
    assert(state != Ready && state != Waiting);
    Fault fault = NoFault;
    write = NULL;
    PageTableEntry pte;
    if (dataSize == 8)
        pte = read->get<uint64_t>();
    else
        pte = read->get<uint32_t>();

    VAddr vaddr = entry.vaddr;
    bool uncacheable = pte.pcd;
    Addr nextRead = 0;
    bool toc_i = true;
    bool doWrite = false;
    bool doTLBInsert = false;
    bool doEndWalk = false;
    bool badNX = pte.nx && mode == BaseTLB::Execute && enableNX;
    switch(state) {
      case LongPML4:
        DPRINTF(PageTableWalker,
                "Got long mode PML4 entry %#016x.\n", (uint64_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl3 * dataSize;
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if(walker->usePWC)
        {
        	walker->LevelToCheck = 3;
        	walker->base = pte;
        	walker->l3totalaccess++;
        	walker->l3misses++;
        	if(walker->l3cache && walker->l3cache->presentInCache(vaddr.longl3, walker->base))
        	{
        		walker->FoundInPWCache = true;
        		walker->l3hits++;
        		walker->l3misses--;
        	}
        }
        entry.noExec = pte.nx;
        nextState = LongPDP;
        break;
      case LongPDP:
        DPRINTF(PageTableWalker,
                "Got long mode PDP entry %#016x.\n", (uint64_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl2 * dataSize;
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if(walker->usePWC)
        {
        	walker->LevelToCheck = 2;
        	walker->base = pte;
        	walker->l2totalaccess++;
        	walker->l2misses++;
        	if(walker->l2cache && walker->l2cache->presentInCache(vaddr.longl2, walker->base))
        	{
        		walker->FoundInPWCache = true;
        		walker->l2hits++;
        		walker->l2misses--;
        	}
        }
        nextState = LongPD;
        break;
      case LongPD:
        DPRINTF(PageTableWalker,
                "Got long mode PD entry %#016x.\n", (uint64_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            walker->LevelToCheck = 1;
            entry.logBytes = 12;
            nextRead =
                ((uint64_t)pte & (mask(40) << 12)) + vaddr.longl1 * dataSize;
            nextState = LongPTE;
            //iceberg TOC page walk for 4k pages
            if (isWalkForBuildTOC) {
                TOC_start_address = pte;
            }
            break;
        } else {
            // 2 MB page
            walker->LevelToCheck = 1;
            entry.logBytes = 21;
            entry.paddr = (uint64_t)pte & (mask(31) << 21);
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            //entry.vaddr = entry.vaddr & ~((2 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            if (isWalkForBuildTOC) {
                //printf("toc ended in 2mb page for vaddr %012x\n",(unsigned int)(entry.vaddr));
                entry.TOC = (uint8_t *)malloc(walker->toc_size*sizeof(uint8_t));
                for (int i=0; i<walker->toc_size; i++)
                    entry.TOC[i] = 1;
                entry.logBytes = 12 + walker->toc_bits;
                entry.uncacheable = uncacheable;
                entry.global = pte.g;
                entry.paddr = 0;
            } else {
                entry.TOC = NULL;
            }
            entry.isHugePage = 1;
            break;
        }
      case LongPTE:
        DPRINTF(PageTableWalker,
                "Got long mode PTE entry %#016x.\n", (uint64_t)pte);
        //need to revist toc population logic
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            if (!isWalkForBuildTOC) {
                doEndWalk = true;
                fault = pageFault(pte.p);
                break;
            } else {
                //printf("page fault in building toc");
                toc_i = false;
            }
        }
        if (!isWalkForBuildTOC) {
            walker->LevelToCheck = 1;
            entry.paddr = (uint64_t)pte & (mask(40) << 12);
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
            entry.TOC = NULL;
            doTLBInsert = true;
            doEndWalk = true;
        } else {
            entry.global = pte.g;
            entry.uncacheable = uncacheable;
            entry.paddr = 0;
            if (TOC_offset == 0) {
                entry.logBytes = 12 + walker->toc_bits;
                start_TOC_building = 1;
                entry.TOC = (uint8_t *)malloc(walker->toc_size*sizeof(uint8_t));
                for (int i=0; i<walker->toc_size; i++) {
                    entry.TOC[i] = 0;
                }
            }
            if (TOC_offset == walker->toc_size - 1) {
                doEndWalk = true;
                doTLBInsert = true;
            }
            entry.TOC[TOC_offset] = toc_i;
            TOC_offset++;
            nextState = LongPTE;
        }
        entry.isHugePage = 0;
        break;
      case PAEPDP:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PDP entry %#08x.\n", (uint32_t)pte);
        nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.pael2 * dataSize;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        nextState = PAEPD;
        break;
      case PAEPD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            entry.logBytes = 12;
            nextRead = ((uint64_t)pte & (mask(40) << 12)) + vaddr.pael1 * dataSize;
            nextState = PAEPTE;
            break;
        } else {
            // 2 MB page
            entry.logBytes = 21;
            entry.paddr = (uint64_t)pte & (mask(31) << 21);
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((2 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            break;
        }
      case PAEPTE:
        DPRINTF(PageTableWalker,
                "Got legacy mode PAE PTE entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = entry.writable && pte.w;
        entry.user = entry.user && pte.u;
        if (badNX || !pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.paddr = (uint64_t)pte & (mask(40) << 12);
        entry.uncacheable = uncacheable;
        entry.global = pte.g;
        entry.patBit = bits(pte, 7);
        entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
        doTLBInsert = true;
        doEndWalk = true;
        break;
      case PSEPD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PSE PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        if (!pte.ps) {
            // 4 KB page
            entry.logBytes = 12;
            nextRead =
                ((uint64_t)pte & (mask(20) << 12)) + vaddr.norml2 * dataSize;
            nextState = PTE;
            break;
        } else {
            // 4 MB page
            entry.logBytes = 21;
            entry.paddr = bits(pte, 20, 13) << 32 | bits(pte, 31, 22) << 22;
            entry.uncacheable = uncacheable;
            entry.global = pte.g;
            entry.patBit = bits(pte, 12);
            entry.vaddr = entry.vaddr & ~((4 * (1 << 20)) - 1);
            doTLBInsert = true;
            doEndWalk = true;
            break;
        }
      case PD:
        DPRINTF(PageTableWalker,
                "Got legacy mode PD entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        // 4 KB page
        entry.logBytes = 12;
        nextRead = ((uint64_t)pte & (mask(20) << 12)) + vaddr.norml2 * dataSize;
        nextState = PTE;
        break;
      case PTE:
        DPRINTF(PageTableWalker,
                "Got legacy mode PTE entry %#08x.\n", (uint32_t)pte);
        doWrite = !pte.a;
        pte.a = 1;
        entry.writable = pte.w;
        entry.user = pte.u;
        if (!pte.p) {
            doEndWalk = true;
            fault = pageFault(pte.p);
            break;
        }
        entry.paddr = (uint64_t)pte & (mask(20) << 12);
        entry.uncacheable = uncacheable;
        entry.global = pte.g;
        entry.patBit = bits(pte, 7);
        entry.vaddr = entry.vaddr & ~((4 * (1 << 10)) - 1);
        doTLBInsert = true;
        doEndWalk = true;
        break;
      default:
        panic("Unknown page table walker state %d!\n");
    }
    if (doEndWalk && !isWalkForDeleteinPTWC) {
        if (doTLBInsert)
            if (!functional)
            {
                walker->tlb->insert_wrapper(entry.vaddr, entry, isWalkForBuildTOC);
            }
        endWalk();
    } else {
        PacketPtr oldRead = read;
        //If we didn't return, we're setting up another read.
        Request::Flags flags = oldRead->req->getFlags();
        flags.set(Request::UNCACHEABLE, uncacheable);
        if (isWalkForBuildTOC && start_TOC_building==1) {
            nextRead =
                ((uint64_t)TOC_start_address & (mask(40) << 12)) + ((vaddr.longl1 & (mask(9 - walker->toc_bits) << walker->toc_bits)) + TOC_offset) * dataSize;
        }
        RequestPtr request =
            new Request(nextRead, oldRead->getSize(), flags, walker->masterId);
        read = new Packet(request, MemCmd::ReadReq);
        read->allocate();
        // If we need to write, adjust the read packet to write the modified
        // value back to memory.
        if (doWrite) {
            write = oldRead;
            write->set<uint64_t>(pte);
            write->cmd = MemCmd::WriteReq;
        } else {
            write = NULL;
            delete oldRead->req;
            delete oldRead;
        }
    }
    return fault;
}

void
Walker::WalkerState::endWalk()
{
    nextState = Ready;
    delete read->req;
    delete read;
    read = NULL;
}

void
Walker::WalkerState::setupWalk(Addr vaddr)
{
    VAddr addr = vaddr;
    CR3 cr3 = tc->readMiscRegNoEffect(MISCREG_CR3);
    // Check if we're in long mode or not
    Efer efer = tc->readMiscRegNoEffect(MISCREG_EFER);
    dataSize = 8;
    Addr topAddr;
    if (efer.lma) {
        // Do long mode.
        state = LongPML4;
        topAddr = (cr3.longPdtb << 12) + addr.longl4 * dataSize;
        if(walker->usePWC)
        {
        	walker->LevelToCheck = 4;
	    	walker->base = cr3.longPdtb;
	    	walker->l4totalaccess++;
	    	walker->l4misses++;
    		if(walker->l4cache && walker->l4cache->presentInCache(addr.longl4, walker->base))
    		{
    	    	walker->FoundInPWCache = true;
    	    	walker->l4hits++;
    	    	walker->l4misses--;
    		}
    	}
        enableNX = efer.nxe;
    } else {
        // We're in some flavor of legacy mode.
        CR4 cr4 = tc->readMiscRegNoEffect(MISCREG_CR4);
        if (cr4.pae) {
            // Do legacy PAE.
            state = PAEPDP;
            topAddr = (cr3.paePdtb << 5) + addr.pael3 * dataSize;
            enableNX = efer.nxe;
        } else {
            dataSize = 4;
            topAddr = (cr3.pdtb << 12) + addr.norml2 * dataSize;
            if (cr4.pse) {
                // Do legacy PSE.
                state = PSEPD;
            } else {
                // Do legacy non PSE.
                state = PD;
            }
            enableNX = false;
        }
    }

    nextState = Ready;
    entry.vaddr = vaddr;

    Request::Flags flags = Request::PHYSICAL;
    if (cr3.pcd)
        flags.set(Request::UNCACHEABLE);
    RequestPtr request = new Request(topAddr, dataSize, flags,
                                     walker->masterId);
    read = new Packet(request, MemCmd::ReadReq);
    read->allocate();
}

void
Walker::WalkerState::handle_iceberg_tlb_miss()
{
    //walker->tlb->update_iceberg_stats(mode);
    walker->start(tc, translation, req, mode, false, true);
}

bool
Walker::WalkerState::recvPacket(PacketPtr pkt)
{
    assert(pkt->isResponse());
    assert(inflight);
    assert(state == Waiting);
    inflight--;
    bool endInSendPacket = false;
    if (pkt->isRead()) {
        // should not have a pending read it we also had one outstanding
        assert(!read);

        // @todo someone should pay for this
        pkt->headerDelay = pkt->payloadDelay = 0;

        state = nextState;
        nextState = Ready;
        PacketPtr write = NULL;
        read = pkt;

        if(walker->usePWC && !walker->FoundInPWCache && !isWalkForDeleteinPTWC)
        {
        	//not found in page walk cache
        	//so, updated pwc with read value
        	PageTableEntry nextToCache = read->get<uint64_t>();
        	VAddr va = req->getVaddr();
            walker->AddEntryToPWCache(va,nextToCache,walker->base);
        }
        
        /*if(walker->usePWC && walker->FoundInPWCache)
        {
        	VAddr va = req->getVaddr();
        	PageTableEntry cachedEntry = walker->GetCachedEntry(va);
        	read->set<uint64_t>(cachedEntry);
        }*/
        
        walker->FoundInPWCache = false;

        walker->stepwalkcount1++;
        timingFault = stepWalk(write);
        if(timingFault != NoFault && walker->usePWC)
        {
        	VAddr va = req->getVaddr();
        	walker->invalidateEntry(va);
        }
        state = Waiting;
        assert(timingFault == NoFault || read == NULL);
        if (write) {
            writes.push_back(write);
        }
        endInSendPacket = sendPackets();
    } else {
        endInSendPacket = sendPackets();
    }
    if (inflight == 0 && read == NULL && writes.size() == 0 && !endInSendPacket) {
        state = Ready;
        nextState = Waiting;
        if(!isWalkForDeleteinPTWC) {
        	if (timingFault == NoFault) {
            	/*
            	 * Finish the translation. Now that we know the right entry is
            	 * in the TLB, this should work with no memory accesses.
            	 * There could be new faults unrelated to the table walk like
            	 * permissions violations, so we'll need the return value as
            	 * well.
            	 */
            	bool delayedResponse;
            	//printf("walk completed in recvpacket\n");
            	Fault fault = walker->tlb->translate(req, tc, translation, mode,
                                                 delayedResponse, true,
                                                 true /*do/don't iceberg tlb lookup*/,
                                                 false /*do not add iceberg pagewalk if iceberg miss*/);
            	//assert(!delayedResponse);
            	// Let the CPU continue.
            	if (delayedResponse) {
            	    //iceberg TLB miss
            	    //handle here
            	    //printf("recv iceberg for vanilla miss %016lx\n", req->getVaddr());
            	    handle_iceberg_tlb_miss();
            	} else
            	    translation->finish(fault, req, tc, mode);

            	walker->totalPageWalkTicks += curTick() - walkStartTick;
        	} else {
            	// There was a fault during the walk. Let the CPU know.
            	translation->finish(timingFault, req, tc, mode);
        	}
        }
        return true;
    }

    return false;
}

bool
Walker::WalkerState::sendPackets()
{
	bool isEnd = false;
    //If we're already waiting for the port to become available, just return.
    if (retrying)
        return false;

    //Reads always have priority
    if (read) {
        //check in page table walk cache
        while(walker->usePWC && walker->FoundInPWCache)
        {
        	//found in page table walk cache
        	VAddr va = req->getVaddr();
        	PageTableEntry cachedEntry = walker->GetCachedEntry(va);
        	read->set<uint64_t>(cachedEntry);

        	//remove entry if flag set
        	if(isWalkForDeleteinPTWC)
        	{
        		walker->RemoveEntryFromPWCache(va, walker->base);
        	}

	        read->headerDelay = read->payloadDelay = 0;

	        state = nextState;
    	    nextState = Ready;
    	    PacketPtr write = NULL;

    	    walker->FoundInPWCache = false;
            walker->stepwalkcount2++;
	        timingFault = stepWalk(write);
	        if (write) {
	            writes.push_back(write);
	        }
	        if(nextState == Ready)
				isEnd = true;
        }
	    if(!isEnd)
	    {
	    	PacketPtr pkt = read;
	    	read = NULL;
	    	state = Waiting;
	    	inflight++;

        	if (!walker->sendTiming(this, pkt)) {
            	retrying = true;
            	read = pkt;
            	inflight--;
            	return false;
        	}
        }
    }
    //Send off as many of the writes as we can.
    while (writes.size()) {
        PacketPtr write = writes.back();
        writes.pop_back();
        inflight++;
        if (!walker->sendTiming(this, write)) {
            retrying = true;
            writes.push_back(write);
            inflight--;
            return false;
        }
    }

	//final memory access hit in page table walk cache
    if(isEnd && walker->usePWC && writes.size() == 0)
    {
    	state = Ready;
        nextState = Waiting;
        if(!isWalkForDeleteinPTWC) {
        	if (timingFault == NoFault) {
            	// Finish the translation. Now that we know the right entry is
            	// in the TLB, this should work with no memory accesses.
            	// There could be new faults unrelated to the table walk like
            	// permissions violations, so we'll need the return value as
            	// well.
            	bool delayedResponse;
            	Fault fault = walker->tlb->translate(req, tc, translation, mode,
                                                 delayedResponse, true,
                                                 true, /*do/don't iceberg tlb lookup*/
                                                 false  /*do not walk on iceberg miss*/);
            	//assert(!delayedResponse);
            	if (delayedResponse) {
            	    //iceberg TLB miss
            	    //handle here
            	    //printf("send iceberg for vanilla miss %016lx\n", req->getVaddr());
            	    handle_iceberg_tlb_miss();
            	} else
            	    // Let the CPU continue.
            	    translation->finish(fault, req, tc, mode);

				walker->totalPageWalkTicks += curTick() - walkStartTick;
        	}
        }

		//when last mem access for page walk is a PTWCache hit,
		//we remove this element from currstates to proceed with processing other elements in queue
		walker->callSchedWalk(this);
		return true;
    }
    return false;
}

bool
Walker::WalkerState::isRetrying()
{
    return retrying;
}

bool
Walker::WalkerState::isTiming()
{
    return timing;
}

bool
Walker::WalkerState::wasStarted()
{
    return started;
}

void
Walker::WalkerState::retry()
{
    retrying = false;
    sendPackets();
}

Fault
Walker::WalkerState::pageFault(bool present)
{
	walker->numpgfaults++;
	walker->tlb->update_iceberg_stats(mode);
    DPRINTF(PageTableWalker, "Raising page fault.\n");
    HandyM5Reg m5reg = tc->readMiscRegNoEffect(MISCREG_M5_REG);
    if (mode == BaseTLB::Execute && !enableNX)
        mode = BaseTLB::Read;
    return std::make_shared<PageFault>(entry.vaddr, present, mode,
                                       m5reg.cpl == 3, false);
}

void
Walker::regStats()
{
    using namespace Stats;

    MemObject::regStats();

    registerDumpCallback(new WalkerDumpCallback(this));
}

void Walker::showStats()
{
	//open stats file
	FILE *pwclog = fopen("pwclogs","a");
	//if file opens successfully
	if(pwclog)
	{
		//write to pwclogs
		using namespace Stats;
		if(totalPageWalks > 0)
		{
			fprintf(pwclog,"============================================================\n");
			fprintf(pwclog,"%s\n",name().c_str());
			fprintf(pwclog,"total page walks:%lld\n",totalPageWalks);
			fprintf(pwclog,"average ticks for page walk:%.4f\n",(float)totalPageWalkTicks/(float)totalPageWalks);
			fprintf(pwclog,"total page faults:%lld\n",numpgfaults);
			fprintf(pwclog,"total stepwalk count1:%lld\n",stepwalkcount1);
			fprintf(pwclog,"total stepwalk count2:%lld\n",stepwalkcount2);
		}

		if(usePWC && l4totalaccess > 0LL && l3totalaccess > 0LL && l2totalaccess > 0LL)
		{
			fprintf(pwclog,"l4:\ntotal access:%lld\nhits:%lld\nmisses:%lld\n",l4totalaccess, l4hits, l4misses);
			if(l4totalaccess != 0)
				fprintf(pwclog,"hit rate:%.2f\nmiss rate:%.2f\n",((double)l4hits/(double)l4totalaccess)*100,((double)l4misses/(double)l4totalaccess)*100);

			fprintf(pwclog,"l3:\ntotal access:%lld\nhits:%lld\nmisses:%lld\n",l3totalaccess, l3hits, l3misses);
			if(l3totalaccess != 0)
				fprintf(pwclog,"hit rate:%.2f\nmiss rate:%.2f\n",((double)l3hits/(double)l3totalaccess)*100,((double)l3misses/(double)l3totalaccess)*100);

			fprintf(pwclog,"l2:\ntotal access:%lld\nhits:%lld\nmisses:%lld\n",l2totalaccess, l2hits, l2misses);
			if(l2totalaccess != 0)
				fprintf(pwclog,"hit rate:%.2f\nmiss rate:%.2f\n",((double)l2hits/(double)l2totalaccess)*100,((double)l2misses/(double)l2totalaccess)*100);

			fprintf(pwclog,"============================================================\n");
		}
		//close stats file
		fclose(pwclog);

		//resetting stats
		totalPageWalks = 0LL;
		totalPageWalkTicks = 0LL;

		l4totalaccess = 0LL;
        l4hits = 0LL;
        l4misses = 0LL;

        l3totalaccess = 0LL;
        l3hits = 0LL;
        l3misses = 0LL;

        l2totalaccess = 0LL;
        l2hits = 0LL;
        l2misses = 0LL;

        numpgfaults = 0LL;
        stepwalkcount1 = 0LL;
        stepwalkcount2 = 0LL;
	}
}

/* end namespace X86ISA */ }

X86ISA::Walker *
X86PagetableWalkerParams::create()
{
    return new X86ISA::Walker(this);
}
