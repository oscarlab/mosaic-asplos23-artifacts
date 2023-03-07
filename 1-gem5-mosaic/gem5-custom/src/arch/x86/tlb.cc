/*
 * Copyright (c) 2007-2008 The Hewlett-Packard Development Company
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

#include "arch/x86/tlb.hh"

#include <cstring>
#include <memory>

#include "arch/generic/mmapped_ipr.hh"
#include "arch/x86/faults.hh"
#include "arch/x86/insts/microldstop.hh"
#include "arch/x86/pagetable_walker.hh"
#include "arch/x86/regs/misc.hh"
#include "arch/x86/regs/msr.hh"
#include "arch/x86/x86_traits.hh"
#include "base/trace.hh"
#include "cpu/thread_context.hh"
#include "debug/TLB.hh"
#include "mem/page_table.hh"
#include "mem/request.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"

namespace X86ISA {

TLB::TLB(const Params *p)
    : BaseTLB(p), configAddress(0),
      L1size(p->sizeL1), L1tlb(L1size), L1lruSeq(0),
      L1IBsize(p->sizeL1), L1IBtlb(L1size), L1IBlruSeq(0),
      L2size(p->sizeL2), L2tlb(L2size), L2lruSeq(0),
      set_associativity_L1(p->setAssocL1), //numcachelines(set_associativity_L1 == 0 ? 0 : L1size/set_associativity_L1),
      //set_associative_tlb_entries(numcachelines),
      simulateIceberg(p->simulateIcebergTLB),
      toc_size(p->toc_size)
{
    if (!L1size)
        fatal("TLBs must have a non-zero size.\n");

    if (set_associativity_L1 == 0) {
        for (int x = 0; x < L1size; x++) {
            L1tlb[x].trieHandle = NULL;
            L1freeList.push_back(&L1tlb[x]);
            //iceberg initialization
            L1IBtlb[x].trieHandle = NULL;
            L1IBfreeList.push_back(&L1IBtlb[x]);
        }
    }
    
    if(L2size) {
        for (int x = 0; x < L2size; x++) {
            L2tlb[x].trieHandle = NULL;
            L2freeList.push_back(&L2tlb[x]);
        }
    }

    //vanilla TLB initialization
    if (set_associativity_L1 > 0) {
        //set associative tlb
        int num_cache_lines = L1size / set_associativity_L1;
        for (int x = 0; x < num_cache_lines; x++) {
            set_associative_tlb_entries.push_back(set_associative_tlb());
            set_associative_tlb_entries[x].size = set_associativity_L1;
            set_associative_tlb_entries[x].lruSeq = 0;
            for (int y = 0; y < set_associativity_L1; y++) {
                set_associative_tlb_entries[x].entries.push_back(TlbEntry());
                set_associative_tlb_entries[x].entries[y].trieHandle = NULL;
            }
            for (std::vector<TlbEntry>::iterator it = set_associative_tlb_entries[x].entries.begin(); it != set_associative_tlb_entries[x].entries.end(); it++) {
                TlbEntry *current_entry = &(*it);
                set_associative_tlb_entries[x].freeList.push_back(current_entry);
            }
        }
    }

    //iceberg TLB initialization
    if (simulateIceberg) {
        if (set_associativity_L1 > 0) {
            //set associative tlb
            int num_cache_lines = L1IBsize / set_associativity_L1;
            for (int x = 0; x < num_cache_lines; x++) {
                set_associative_tlb_entries_iceberg.push_back(set_associative_tlb());
                set_associative_tlb_entries_iceberg[x].size = set_associativity_L1;
                for (int y = 0; y < set_associativity_L1; y++) {
                    set_associative_tlb_entries_iceberg[x].entries.push_back(TlbEntry());
                    set_associative_tlb_entries_iceberg[x].entries[y].trieHandle = NULL;
                }
                for (std::vector<TlbEntry>::iterator it = set_associative_tlb_entries_iceberg[x].entries.begin(); it != set_associative_tlb_entries_iceberg[x].entries.end(); it++) {
                    TlbEntry *current_entry = &(*it);
                    set_associative_tlb_entries_iceberg[x].freeList.push_back(current_entry);
                }
                set_associative_tlb_entries_iceberg[x].lruSeq = 0;
            }
        }
    }

    walker = p->walker;
    walker->setTLB(this);

    toc_bits = floorLog2(toc_size);

    aprox_tot_instructs = 0LL;

    totalaccessesL1 = 0LL;
    missesL1 = 0LL;
    totalreadaccessL1 = 0LL;
    totalwriteaccessL1 = 0LL;
    readmissesL1 = 0LL;
    writemissesL1 = 0LL;
    insertsL1 = 0LL;
    lruevictsL1 = 0LL;
    lruIBevictsL1 = 0LL;

    totalaccessesIBL1 = 0LL;
    missesIBL1 = 0LL;
    totalreadaccessIBL1 = 0LL;
    totalwriteaccessIBL1 = 0LL;
    readmissesIBL1 = 0LL;
    writemissesIBL1 = 0LL;

    totalaccessesL2 = 0LL;
    missesL2 = 0LL;
    totalreadaccessL2 = 0LL;
    totalwriteaccessL2 = 0LL;
    readmissesL2 = 0LL;
    writemissesL2 = 0LL;
    lruevictsL2 = 0LL;
}

int
TLB::getSetNumber(Addr va)
{
    int shiftAmt = PAGEWIDTH;
    int num_cache_lines = L1size / set_associativity_L1;
    if (simulateIceberg) {
        //page offset + tocoffset
        //eg: for toc64, 12 + 6 = 18
        shiftAmt += toc_bits;
        num_cache_lines = L1IBsize / set_associativity_L1;
    }
    return (va>>shiftAmt)%num_cache_lines;
}

void
TLB::evictLRUL1()
{
    // Find the entry with the lowest (and hence least recently updated)
    // sequence number.

    unsigned lru = 0;
    for (unsigned i = 1; i < L1size; i++) {
        if (L1tlb[i].lruSeq < L1tlb[lru].lruSeq)
            lru = i;
    }

    assert(L1tlb[lru].trieHandle);
    insertL2(L1tlb[lru].vaddr,L1tlb[lru]);
    L1trie.remove(L1tlb[lru].trieHandle);
    L1tlb[lru].trieHandle = NULL;
    L1freeList.push_back(&L1tlb[lru]);
    //lruevictsL1++;
}

void
TLB::evictLRUIBL1()
{
    //printf("iceberg tlb evictions\n");
    // Find the entry with the lowest (and hence least recently updated)
    // sequence number.

    unsigned lru = 0;
    for (unsigned i = 1; i < L1IBsize; i++) {
        if (L1IBtlb[i].lruSeq < L1IBtlb[lru].lruSeq)
            lru = i;
    }

    assert(L1IBtlb[lru].trieHandle);
    insertL2(L1IBtlb[lru].vaddr,L1tlb[lru]);
    L1IBtrie.remove(L1IBtlb[lru].trieHandle);
    L1IBtlb[lru].trieHandle = NULL;
    L1IBfreeList.push_back(&L1IBtlb[lru]);
    //lruevictsL1++;
}

void
TLB::evictLRUL2()
{
    // Find the entry with the lowest (and hence least recently updated)
    // sequence number.

    unsigned lru = 0;
    for (unsigned i = 1; i < L2size; i++) {
        if (L2tlb[i].lruSeq < L2tlb[lru].lruSeq)
            lru = i;
    }

    assert(L2tlb[lru].trieHandle);
    L2trie.remove(L2tlb[lru].trieHandle);
    L2tlb[lru].trieHandle = NULL;
    L2freeList.push_back(&L2tlb[lru]);
    //lruevictsL2++;
}

void
TLB::evictLRUL1SetAssociative(int setNumber)
{
    unsigned lru = 0;
    for (unsigned i = 1; i < set_associativity_L1; i++) {
        if (set_associative_tlb_entries[setNumber].entries[i].lruSeq < set_associative_tlb_entries[setNumber].entries[lru].lruSeq)
            lru = i;
    }

    assert(set_associative_tlb_entries[setNumber].entries[lru].trieHandle);
    set_associative_tlb_entries[setNumber].trie.remove(set_associative_tlb_entries[setNumber].entries[lru].trieHandle);
    set_associative_tlb_entries[setNumber].entries[lru].trieHandle = NULL;
    int cnt = 0;
    for (std::vector<TlbEntry>::iterator it = set_associative_tlb_entries[setNumber].entries.begin(); it != set_associative_tlb_entries[setNumber].entries.end(); it++) {
        if (cnt == lru) {
            TlbEntry *lru_entry = &(*it);
            set_associative_tlb_entries[setNumber].freeList.push_back(lru_entry);
            break;
        }
        cnt++;
    }
    lruevictsL1++;
}

void
TLB::evictLRUIBL1SetAssociative(int setNumber)
{
    unsigned lru = 0;
    for (unsigned i = 1; i < set_associativity_L1; i++) {
        if (set_associative_tlb_entries_iceberg[setNumber].entries[i].lruSeq < set_associative_tlb_entries_iceberg[setNumber].entries[lru].lruSeq)
            lru = i;
    }

    assert(set_associative_tlb_entries_iceberg[setNumber].entries[lru].trieHandle);
    set_associative_tlb_entries_iceberg[setNumber].trie.remove(set_associative_tlb_entries_iceberg[setNumber].entries[lru].trieHandle);
    set_associative_tlb_entries_iceberg[setNumber].entries[lru].trieHandle = NULL;
    //set_associative_tlb_entries_iceberg[setNumber].freeList.push_back(&set_associative_tlb_entries_iceberg[setNumber].entries[lru]);
    int cnt = 0;
    for (std::vector<TlbEntry>::iterator it = set_associative_tlb_entries_iceberg[setNumber].entries.begin(); it != set_associative_tlb_entries_iceberg[setNumber].entries.end(); it++) {
        if (cnt == lru) {
            set_associative_tlb_entries_iceberg[setNumber].freeList.push_back(&(*it));
            break;
        }
        cnt++;
    }
    lruIBevictsL1++;
}

TlbEntry *
TLB::insertL1(Addr vpn, const TlbEntry &entry)
{
    // If somebody beat us to it, just use that existing entry.
    TlbEntry *newEntry = L1trie.lookup(vpn);
    if (newEntry) {
        assert(newEntry->vaddr == (vpn & ~mask(newEntry->logBytes)));
        return newEntry;
    }

    if (L1freeList.empty())
        evictLRUL1();

    newEntry = L1freeList.front();
    L1freeList.pop_front();

    *newEntry = entry;
    newEntry->lruSeq = nextSeqL1();
    newEntry->vaddr = vpn;
    newEntry->trieHandle =
    L1trie.insert(vpn, TlbEntryTrie::MaxBits - entry.logBytes, newEntry);
    insertsL1++;
    return newEntry;
}

TlbEntry *
TLB::insertIBL1(Addr vpn, const TlbEntry &entry)
{
    //If somebody beat us to it, just use that existing entry.
    TlbEntry *newEntry = L1IBtrie.lookup(vpn);
    if (newEntry) {
        assert(newEntry->vaddr == (vpn & ~mask(newEntry->logBytes)));
        return newEntry;
    }

    if (L1IBfreeList.empty()) {
        evictLRUIBL1();
    }

    newEntry = L1IBfreeList.front();
    L1IBfreeList.pop_front();

    *newEntry = entry;
    newEntry->lruSeq = nextSeqIBL1();
    newEntry->vaddr = vpn;
    newEntry->trieHandle =
    L1IBtrie.insert(vpn, TlbEntryTrie::MaxBits - entry.logBytes, newEntry);
    return newEntry;
}

TlbEntry *
TLB::insertL2(Addr vpn, const TlbEntry &entry)
{
	if(L2size==0)
		return NULL;
    // If somebody beat us to it, just use that existing entry.
    TlbEntry *newEntry = L2trie.lookup(vpn);
    if (newEntry) {
        assert(newEntry->vaddr == (vpn & ~mask(newEntry->logBytes)));
        return newEntry;
    }

    if (L2freeList.empty())
        evictLRUL2();

    newEntry = L2freeList.front();
    L2freeList.pop_front();

    *newEntry = entry;
    newEntry->lruSeq = nextSeqL2();
    newEntry->vaddr = vpn;
    newEntry->trieHandle =
    L2trie.insert(vpn, TlbEntryTrie::MaxBits - entry.logBytes, newEntry);
    return newEntry;
}

TlbEntry *
TLB::insertL1SetAssociative(Addr vpn, const TlbEntry &entry)
{
    int setNumber = getSetNumber(vpn);
    TlbEntry *newEntry = set_associative_tlb_entries[setNumber].trie.lookup(vpn);
    if (newEntry) {
        //FIXME:problem with vanilla TLB
        //assert(newEntry->vaddr == (vpn & ~mask(newEntry->logBytes)));
        return newEntry;
    }

    if (set_associative_tlb_entries[setNumber].freeList.empty())
        evictLRUL1SetAssociative(setNumber);

    newEntry = set_associative_tlb_entries[setNumber].freeList.front();
    set_associative_tlb_entries[setNumber].freeList.pop_front();

    *newEntry = entry;
    newEntry->lruSeq = ++(set_associative_tlb_entries[setNumber].lruSeq);
    newEntry->vaddr = vpn;
    int mask = TlbEntryTrie::MaxBits - entry.logBytes;
    newEntry->trieHandle =
    set_associative_tlb_entries[setNumber].trie.insert(vpn, mask, newEntry);
    return newEntry;
}

TlbEntry *
TLB::insertIBL1SetAssociative(Addr vpn, const TlbEntry &entry)
{
    int setNumber = getSetNumber(vpn);
    TlbEntry *newEntry = set_associative_tlb_entries_iceberg[setNumber].trie.lookup(vpn);
    if (newEntry) {
        assert(newEntry->vaddr == (vpn & ~mask(newEntry->logBytes)));
        return newEntry;
    }

    if (set_associative_tlb_entries_iceberg[setNumber].freeList.empty())
        evictLRUIBL1SetAssociative(setNumber);

    newEntry = set_associative_tlb_entries_iceberg[setNumber].freeList.front();
    set_associative_tlb_entries_iceberg[setNumber].freeList.pop_front();

    *newEntry = entry;
    newEntry->lruSeq = ++(set_associative_tlb_entries_iceberg[setNumber].lruSeq);
    newEntry->vaddr = vpn;
    int mask = TlbEntryTrie::MaxBits - entry.logBytes;
    if (simulateIceberg)
        mask = TlbEntryTrie::MaxBits - (12 + toc_bits);
    newEntry->trieHandle =
    set_associative_tlb_entries_iceberg[setNumber].trie.insert(vpn, mask, newEntry);
    return newEntry;
}

TlbEntry *
TLB::insert_wrapper(Addr vpn, const TlbEntry &entry, bool is_iceberg)
{
    TlbEntry *ret = NULL;
    //L1 TLB handling
    if (is_iceberg) {
        //iceberg case
        if (set_associativity_L1 > 0)
            ret = insertIBL1SetAssociative(vpn, entry);
        else
            ret = insertIBL1(vpn, entry);
    } else {
        //vanilla case
        if (set_associativity_L1 > 0)
            ret = insertL1SetAssociative(vpn, entry);
        else
            ret = insertL1(vpn, entry);
    }
    //L2 TLB handling
    if (L2size > 0)
        ret = insertL2(vpn, entry);
    return ret;
}


TlbEntry *
TLB::lookupL1(Addr va, bool update_lru)
{
    TlbEntry *entry = L1trie.lookup(va);
    if (entry && update_lru)
        entry->lruSeq = nextSeqL1();
    return entry;
}

TlbEntry *
TLB::lookupIBL1(Addr va, bool update_lru)
{
    TlbEntry *entry = L1IBtrie.lookup(va);
    if (entry && update_lru)
        entry->lruSeq = nextSeqIBL1();
    return entry;
}

TlbEntry *
TLB::lookupL2(Addr va, bool update_lru)
{
    TlbEntry *entry = L2trie.lookup(va);
    if (entry && update_lru)
        entry->lruSeq = nextSeqL2();
    return entry;
}

TlbEntry *
TLB::lookupL1SetAssociative(Addr va, bool update_lru)
{
    TlbEntry *entry = NULL;
    int setNumber = getSetNumber(va);
    entry = set_associative_tlb_entries[setNumber].trie.lookup(va);
    if (entry && update_lru) {
        (set_associative_tlb_entries[setNumber].lruSeq)++;
        entry->lruSeq = set_associative_tlb_entries[setNumber].lruSeq;
    }
    return entry;
}

TlbEntry *
TLB::lookupIBL1SetAssociative(Addr va, bool update_lru)
{
    TlbEntry *entry = NULL;
    int setNumber = getSetNumber(va);
    entry = set_associative_tlb_entries_iceberg[setNumber].trie.lookup(va);
    if (entry && update_lru) {
        (set_associative_tlb_entries_iceberg[setNumber].lruSeq)++;
        entry->lruSeq = set_associative_tlb_entries_iceberg[setNumber].lruSeq;
    }
    return entry;
}

void
TLB::flushAll()
{
    DPRINTF(TLB, "Invalidating all entries.\n");
    //vanilla L1 fully assoc TLB
    if (set_associativity_L1 == 0) {
        for (unsigned i = 0;  i < L1size; i++) {
            if (L1tlb[i].trieHandle) {
                L1trie.remove(L1tlb[i].trieHandle);
                L1tlb[i].trieHandle = NULL;
                L1freeList.push_back(&L1tlb[i]);
            }
        }
        //iceberg L1 fully assoc TLB
        for (unsigned i = 0; i < L1IBsize; i++) {
            if (L1IBtlb[i].trieHandle) {
                L1IBtrie.remove(L1IBtlb[i].trieHandle);
                L1IBtlb[i].trieHandle = NULL;
                L1IBfreeList.push_back(&L1IBtlb[i]);
            }
        }
    }
    //vanila L2 fully assoc TLB
    for (unsigned i = 0; i < L2size; i++) {
        if (L2tlb[i].trieHandle) {
            L2trie.remove(L2tlb[i].trieHandle);
            L2tlb[i].trieHandle = NULL;
            L2freeList.push_back(&L2tlb[i]);
        }
    }
    //vanilla set assoc TLB
    if (set_associativity_L1 > 0) {
        int num_cache_lines = L1size / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries[i].entries[j].trieHandle) {
                    set_associative_tlb_entries[i].trie.remove(set_associative_tlb_entries[i].entries[j].trieHandle);
                    set_associative_tlb_entries[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries[i].freeList.push_back(&set_associative_tlb_entries[i].entries[j]);                }
            }
        }
    }
    //iceberg set assoc TLB
    if (simulateIceberg && set_associativity_L1 > 0) {
        int num_cache_lines = L1IBsize / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries_iceberg[i].entries[j].trieHandle) {
                    set_associative_tlb_entries_iceberg[i].trie.remove(set_associative_tlb_entries_iceberg[i].entries[j].trieHandle);
                    set_associative_tlb_entries_iceberg[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries_iceberg[i].freeList.push_back(&set_associative_tlb_entries_iceberg[i].entries[j]);
                }
            }
        }
    }
    //handling for flushing pwc
    if(FullSystem)
    	walker->flushAllPWC();
}

void
TLB::setConfigAddress(uint32_t addr)
{
    configAddress = addr;
}

void
TLB::flushNonGlobal()
{
    DPRINTF(TLB, "Invalidating all non global entries.\n");
    //vanilla L1 fully assoc TLB
    if (set_associativity_L1 == 0) {
        for (unsigned i = 0; i < L1size; i++) {
            if (L1tlb[i].trieHandle && !L1tlb[i].global) {
                L1trie.remove(L1tlb[i].trieHandle);
                L1tlb[i].trieHandle = NULL;
                L1freeList.push_back(&L1tlb[i]);
            }
        }
        //iceberg L1 fully assoc TLB
        for (unsigned i = 0; i < L1IBsize; i++) {
            if (L1IBtlb[i].trieHandle && !L1IBtlb[i].global) {
                L1IBtrie.remove(L1IBtlb[i].trieHandle);
                L1IBtlb[i].trieHandle = NULL;
                L1IBfreeList.push_back(&L1IBtlb[i]);
            }
        }
    }
    //vanilla L2 fully assoc TLB
    for (unsigned i = 0; i < L2size; i++) {
        if (L2tlb[i].trieHandle && !L2tlb[i].global) {
            L2trie.remove(L2tlb[i].trieHandle);
            L2tlb[i].trieHandle = NULL;
            L2freeList.push_back(&L2tlb[i]);
        }
    }
    //FIXME: memory corruption in vanilla TLB object
    //vanilla L1 set assoc TLB
    /*if (set_associativity_L1 > 0) {
        int num_cache_lines = L1size / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries[i].entries[j].trieHandle &&
                    set_associative_tlb_entries[i].entries[j].global) {
                    set_associative_tlb_entries[i].trie.remove(set_associative_tlb_entries[i].entries[j].trieHandle);
                    set_associative_tlb_entries[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries[i].freeList.push_back(&set_associative_tlb_entries[i].entries[j]);
                }
            }
        }
    }*/
    if (set_associativity_L1 > 0) {
        int num_cache_lines = L1size / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries[i].entries[j].trieHandle) {
                    set_associative_tlb_entries[i].trie.remove(set_associative_tlb_entries[i].entries[j].trieHandle);
                    set_associative_tlb_entries[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries[i].freeList.push_back(&set_associative_tlb_entries[i].entries[j]);
                }
            }
        }
    }
    //iceberg L1 set assoc TLB
    /*if (simulateIceberg && set_associativity_L1 > 0) {
        int num_cache_lines = L1IBsize / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries_iceberg[i].entries[j].trieHandle &&
                    set_associative_tlb_entries_iceberg[i].entries[j].global) {
                    set_associative_tlb_entries_iceberg[i].trie.remove(set_associative_tlb_entries_iceberg[i].entries[j].trieHandle);
                    set_associative_tlb_entries_iceberg[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries_iceberg[i].freeList.push_back(&set_associative_tlb_entries_iceberg[i].entries[j]);
                }
            }
        }
    }*/
    if (simulateIceberg && set_associativity_L1 > 0) {
        int num_cache_lines = L1IBsize / set_associativity_L1;
        for (unsigned i = 0; i < num_cache_lines; i++) {
            for (unsigned j = 0; j < set_associativity_L1; j++) {
                if (set_associative_tlb_entries_iceberg[i].entries[j].trieHandle) {
                    set_associative_tlb_entries_iceberg[i].trie.remove(set_associative_tlb_entries_iceberg[i].entries[j].trieHandle);
                    set_associative_tlb_entries_iceberg[i].entries[j].trieHandle = NULL;
                    set_associative_tlb_entries_iceberg[i].freeList.push_back(&set_associative_tlb_entries_iceberg[i].entries[j]);
                }
            }
        }
    }
    //handling for flushing pwc
    if(FullSystem)
    	walker->flushAllPWC();
}

bool
TLB::tocAllInvalid(uint8_t *toc)
{
    int i;
    for (i=0; i<toc_size; i++) {
        if (toc[i]!=0)
            return false;
    }
    return true;
}

void
TLB::demapPage(Addr va, uint64_t asn)
{
    //vanilla L1 fully assoc TLB
    if (set_associativity_L1 == 0) {
        TlbEntry *entry = L1trie.lookup(va);
        if (entry) {
            L1trie.remove(entry->trieHandle);
            entry->trieHandle = NULL;
            L1freeList.push_back(entry);
        }
        //iceberg L1 fully assoc TLB
        TlbEntry *ib_entry = L1IBtrie.lookup(va);
        if (ib_entry) {
            VAddr vaddr = va;
            uint8_t tocOffset = (vaddr>>12) & (toc_size - 1);
            ib_entry->TOC[tocOffset] = 0;
            if (tocAllInvalid(ib_entry->TOC)) {
                L1IBtrie.remove(ib_entry->trieHandle);
                ib_entry->trieHandle = NULL;
                L1IBfreeList.push_back(ib_entry);
            }
        }
    }
    //vanilla L2 fully assoc TLB
   TlbEntry *entryL2 = L2trie.lookup(va);
    if (entryL2) {
        L2trie.remove(entryL2->trieHandle);
        entryL2->trieHandle = NULL;
        L2freeList.push_back(entryL2);
    }
    //vanilla L1 set assoc TLB
    if (set_associativity_L1 > 0) {
        int setNumber = getSetNumber(va);
        TlbEntry *entrySA = set_associative_tlb_entries[setNumber].trie.lookup(va);
        if (entrySA) {
            set_associative_tlb_entries[setNumber].trie.remove(entrySA->trieHandle);
            entrySA->trieHandle = NULL;
            set_associative_tlb_entries[setNumber].freeList.push_back(entrySA);
        }
    }
    //iceberg L1 set assoc TLB
    if (simulateIceberg && set_associativity_L1 > 0) {
        int setNumber = getSetNumber(va);
        TlbEntry *entrySA = set_associative_tlb_entries_iceberg[setNumber].trie.lookup(va);
        if (entrySA) {
            VAddr vaddr = va;
            uint8_t tocOffset = (vaddr>>12) & (toc_size - 1);
            entrySA->TOC[tocOffset] = 0;
            if (tocAllInvalid(entrySA->TOC)) {
                set_associative_tlb_entries_iceberg[setNumber].trie.remove(entrySA->trieHandle);
                entrySA->trieHandle = NULL;
                set_associative_tlb_entries_iceberg[setNumber].freeList.push_back(entrySA);
            }
        }
    }
    //handling for removing va from pwc
    if(FullSystem)
    	walker->invalidateAllPWCforVAddr(va);
}

Fault
TLB::translateInt(RequestPtr req, ThreadContext *tc)
{
    DPRINTF(TLB, "Addresses references internal memory.\n");
    Addr vaddr = req->getVaddr();
    Addr prefix = (vaddr >> 3) & IntAddrPrefixMask;
    if (prefix == IntAddrPrefixCPUID) {
        panic("CPUID memory space not yet implemented!\n");
    } else if (prefix == IntAddrPrefixMSR) {
        vaddr = (vaddr >> 3) & ~IntAddrPrefixMask;
        req->setFlags(Request::MMAPPED_IPR);

        MiscRegIndex regNum;
        if (!msrAddrToIndex(regNum, vaddr))
            return std::make_shared<GeneralProtection>(0);

        //The index is multiplied by the size of a MiscReg so that
        //any memory dependence calculations will not see these as
        //overlapping.
        req->setPaddr((Addr)regNum * sizeof(MiscReg));
        return NoFault;
    } else if (prefix == IntAddrPrefixIO) {
        // TODO If CPL > IOPL or in virtual mode, check the I/O permission
        // bitmap in the TSS.

        Addr IOPort = vaddr & ~IntAddrPrefixMask;
        // Make sure the address fits in the expected 16 bit IO address
        // space.
        assert(!(IOPort & ~0xFFFF));
        if (IOPort == 0xCF8 && req->getSize() == 4) {
            req->setFlags(Request::MMAPPED_IPR);
            req->setPaddr(MISCREG_PCI_CONFIG_ADDRESS * sizeof(MiscReg));
        } else if ((IOPort & ~mask(2)) == 0xCFC) {
            req->setFlags(Request::UNCACHEABLE | Request::STRICT_ORDER);
            Addr configAddress =
                tc->readMiscRegNoEffect(MISCREG_PCI_CONFIG_ADDRESS);
            if (bits(configAddress, 31, 31)) {
                req->setPaddr(PhysAddrPrefixPciConfig |
                        mbits(configAddress, 30, 2) |
                        (IOPort & mask(2)));
            } else {
                req->setPaddr(PhysAddrPrefixIO | IOPort);
            }
        } else {
            req->setFlags(Request::UNCACHEABLE | Request::STRICT_ORDER);
            req->setPaddr(PhysAddrPrefixIO | IOPort);
        }
        return NoFault;
    } else {
        panic("Access to unrecognized internal address space %#x.\n",
                prefix);
    }
}

Fault
TLB::finalizePhysical(RequestPtr req, ThreadContext *tc, Mode mode) const
{
    Addr paddr = req->getPaddr();

    AddrRange m5opRange(0xFFFF0000, 0xFFFFFFFF);

    if (m5opRange.contains(paddr)) {
        req->setFlags(Request::MMAPPED_IPR | Request::GENERIC_IPR |
                      Request::STRICT_ORDER);
        req->setPaddr(GenericISA::iprAddressPseudoInst((paddr >> 8) & 0xFF,
                                                       paddr & 0xFF));
    } else if (FullSystem) {
        // Check for an access to the local APIC
        LocalApicBase localApicBase =
            tc->readMiscRegNoEffect(MISCREG_APIC_BASE);
        AddrRange apicRange(localApicBase.base * PageBytes,
                            (localApicBase.base + 1) * PageBytes - 1);

        if (apicRange.contains(paddr)) {
            // The Intel developer's manuals say the below restrictions apply,
            // but the linux kernel, because of a compiler optimization, breaks
            // them.
            /*
            // Check alignment
            if (paddr & ((32/8) - 1))
                return new GeneralProtection(0);
            // Check access size
            if (req->getSize() != (32/8))
                return new GeneralProtection(0);
            */
            // Force the access to be uncacheable.
            req->setFlags(Request::UNCACHEABLE | Request::STRICT_ORDER);
            req->setPaddr(x86LocalAPICAddress(tc->contextId(),
                                              paddr - apicRange.start()));
        }
    }

    return NoFault;
}


void
TLB::update_iceberg_stats(Mode mode)
{
    totalaccessesIBL1++;
    if (mode == Read) {
        totalreadaccessIBL1++;
    } else {
        totalwriteaccessIBL1++;
    }
    missesIBL1++;
    if (mode == Read) {
        readmissesIBL1++;
    } else {
        writemissesIBL1++;
    }
}


Fault
TLB::translate(RequestPtr req, ThreadContext *tc, Translation *translation,
        Mode mode, bool &delayedResponse, bool timing, bool isIcebergCheckNeeded,
        bool isIcebergPageWalkNeeded)
{
    Request::Flags flags = req->getFlags();
    int seg = flags & SegmentFlagMask;
    bool storeCheck = flags & (StoreCheck << FlagShift);

    delayedResponse = false;

    // If this is true, we're dealing with a request to a non-memory address
    // space.
    if (seg == SEGMENT_REG_MS) {
        return translateInt(req, tc);
    }

    Addr vaddr = req->getVaddr();
    DPRINTF(TLB, "Translating vaddr %#x.\n", vaddr);

    HandyM5Reg m5Reg = tc->readMiscRegNoEffect(MISCREG_M5_REG);

    // If protected mode has been enabled...
    if (m5Reg.prot) {
        DPRINTF(TLB, "In protected mode.\n");
        // If we're not in 64-bit mode, do protection/limit checks
        if (m5Reg.mode != LongMode) {
            DPRINTF(TLB, "Not in long mode. Checking segment protection.\n");
            // Check for a NULL segment selector.
            if (!(seg == SEGMENT_REG_TSG || seg == SYS_SEGMENT_REG_IDTR ||
                        seg == SEGMENT_REG_HS || seg == SEGMENT_REG_LS)
                    && !tc->readMiscRegNoEffect(MISCREG_SEG_SEL(seg)))
                return std::make_shared<GeneralProtection>(0);
            bool expandDown = false;
            SegAttr attr = tc->readMiscRegNoEffect(MISCREG_SEG_ATTR(seg));
            if (seg >= SEGMENT_REG_ES && seg <= SEGMENT_REG_HS) {
                if (!attr.writable && (mode == Write || storeCheck))
                    return std::make_shared<GeneralProtection>(0);
                if (!attr.readable && mode == Read)
                    return std::make_shared<GeneralProtection>(0);
                expandDown = attr.expandDown;

            }
            Addr base = tc->readMiscRegNoEffect(MISCREG_SEG_BASE(seg));
            Addr limit = tc->readMiscRegNoEffect(MISCREG_SEG_LIMIT(seg));
            bool sizeOverride = (flags & (AddrSizeFlagBit << FlagShift));
            unsigned logSize = sizeOverride ? (unsigned)m5Reg.altAddr
                                            : (unsigned)m5Reg.defAddr;
            int size = (1 << logSize) * 8;
            Addr offset = bits(vaddr - base, size - 1, 0);
            Addr endOffset = offset + req->getSize() - 1;
            if (expandDown) {
                DPRINTF(TLB, "Checking an expand down segment.\n");
                warn_once("Expand down segments are untested.\n");
                if (offset <= limit || endOffset <= limit)
                    return std::make_shared<GeneralProtection>(0);
            } else {
                if (offset > limit || endOffset > limit)
                    return std::make_shared<GeneralProtection>(0);
            }
        }
        if (m5Reg.submode != SixtyFourBitMode ||
                (flags & (AddrSizeFlagBit << FlagShift)))
            vaddr &= mask(32);
        // If paging is enabled, do the translation.
        if (m5Reg.paging) {
            DPRINTF(TLB, "Paging enabled.\n");

            // The vaddr already has the segment base applied.
            TlbEntry *entry = NULL;

            if (set_associativity_L1 > 0)
                entry = lookupL1SetAssociative(vaddr);
            else
                entry = lookupL1(vaddr);

            totalaccessesL1++;
            if (mode == Read) {
                totalreadaccessL1++;
            } else {
                totalwriteaccessL1++;
            }
            if (!entry) {
            	//L1 TLB miss
                DPRINTF(TLB, "Handling a TLB miss for "
                        "address %#x at pc %#x.\n",
                        vaddr, tc->instAddr());
                if (mode == Read) {
                    readmissesL1++;
                } else {
                    writemissesL1++;
                }
                missesL1++;
                bool isMiss = true;
                if(L2size>0)
                {
                	totalaccessesL2++;
                	if (mode == Read) {
                		totalreadaccessL2++;
            		} else {
                		totalwriteaccessL2++;
            		}
                	//lookup in L2 TLB
                	entry = lookupL2(vaddr);
                	if(entry)
                	{
                		isMiss = false;
                		//found in L2 tlb
                		//insert into L1 tlb - L2 is inclusive cache
                		insertL1(vaddr,*entry);
                	}
                	else
                	{
                		if (mode == Read) {
                    		readmissesL2++;
                		} else {
                    		writemissesL2++;
                		}
                		missesL2++;
                	}
                }
                if (isMiss)
                {
                	if (FullSystem) {
				    //printf("vanilla tlb miss for vaddr %012x\n",(unsigned int)vaddr);
				Fault fault = walker->start(tc, translation, req, mode);
				if (timing || fault != NoFault) {
				    // This gets ignored in atomic mode.
				    delayedResponse = true;
                            totalaccessesIBL1++;
                            if (mode == Read) {
                                totalreadaccessIBL1++;
                            } else {
                                totalwriteaccessIBL1++;
                            }
				    return fault;
				}
				entry = lookupL1(vaddr);
				assert(entry);
                	} else {
                    	Process *p = tc->getProcessPtr();
                    	const EmulationPageTable::Entry *pte =
                    	    p->pTable->lookup(vaddr);
                    	if (!pte && mode != Execute) {
                    	    // Check if we just need to grow the stack.
                    	    if (p->fixupStackFault(vaddr)) {
                    	        // If we did, lookup the entry for the new page.
                    	        pte = p->pTable->lookup(vaddr);
                    	    }
                    	}
                    	if (!pte) {
                    	    return std::make_shared<PageFault>(vaddr, true, mode,
                                                           true, false);
                    	} else {
                    	    Addr alignedVaddr = p->pTable->pageAlign(vaddr);
                    	    DPRINTF(TLB, "Mapping %#x to %#x\n", alignedVaddr,
                    	            pte->paddr);
                    	    entry = insertL1(alignedVaddr, TlbEntry(
                   	            p->pTable->pid(), alignedVaddr, pte->paddr,
                                pte->flags & EmulationPageTable::Uncacheable,
                                pte->flags & EmulationPageTable::ReadOnly));
                   		}
                    	DPRINTF(TLB, "Miss was serviced.\n");
                	}
                }
            }

            //iceberg tlb handling
            if (simulateIceberg &&
                 isIcebergCheckNeeded) {
                totalaccessesIBL1++;
                if (mode == Read) {
                    totalreadaccessIBL1++;
                } else {
                    totalwriteaccessIBL1++;
                }
                bool is_iceberg_miss = true;
                TlbEntry *iceberg_entry = NULL;
                if (set_associativity_L1 > 0)
                    iceberg_entry = lookupIBL1SetAssociative(vaddr);
                else
                    iceberg_entry = lookupIBL1(vaddr);
                if (iceberg_entry) {
                    //check if entry is valid
                    int toc_offset = (vaddr>>12) & mask(toc_bits);
                    if (iceberg_entry->TOC[toc_offset] == 0) {
                        missesIBL1++;
                        if (mode == Read) {
                            readmissesIBL1++;
                        } else {
                            writemissesIBL1++;
                        }
                        iceberg_entry->TOC[toc_offset] = 1;
                    }
                    is_iceberg_miss = false;
                }
                if (is_iceberg_miss) {
                    /*
                        walk page table
                        to build toc
                        and add it to iceberg tlb
                    */
                    missesIBL1++;
                    if (mode == Read) {
                        readmissesIBL1++;
                    } else {
                        writemissesIBL1++;
                    }
                    Fault fault = NoFault;
                    if (isIcebergPageWalkNeeded)
                        fault = walker->start(tc, translation, req, mode,
                    	                                            false /*not a walk for delete*/,
                    	                                            true /*walk for building toc*/);
                    if (timing /*|| fault != NoFault*/) {
                        // This gets ignored in atomic mode.
                        delayedResponse = true;
                        return fault;
                    }
                }
            }

            DPRINTF(TLB, "Entry found with paddr %#x, "
                    "doing protection checks.\n", entry->paddr);
            // Do paging protection checks.
            bool inUser = (m5Reg.cpl == 3 &&
                    !(flags & (CPL0FlagBit << FlagShift)));
            CR0 cr0 = tc->readMiscRegNoEffect(MISCREG_CR0);
            bool badWrite = (!entry->writable && (inUser || cr0.wp));
            if ((inUser && !entry->user) || (mode == Write && badWrite)) {
                // The page must have been present to get into the TLB in
                // the first place. We'll assume the reserved bits are
                // fine even though we're not checking them.
                return std::make_shared<PageFault>(vaddr, true, mode, inUser,
                                                   false);
            }
            if (storeCheck && badWrite) {
                // This would fault if this were a write, so return a page
                // fault that reflects that happening.
                return std::make_shared<PageFault>(vaddr, true, Write, inUser,
                                                   false);
            }

            Addr paddr = entry->paddr | (vaddr & mask(entry->logBytes));
            DPRINTF(TLB, "Translated %#x -> %#x.\n", vaddr, paddr);
            req->setPaddr(paddr);
            if (entry->uncacheable)
                req->setFlags(Request::UNCACHEABLE | Request::STRICT_ORDER);
        } else {
            //Use the address which already has segmentation applied.
            DPRINTF(TLB, "Paging disabled.\n");
            DPRINTF(TLB, "Translated %#x -> %#x.\n", vaddr, vaddr);
            req->setPaddr(vaddr);
        }
    } else {
        // Real mode
        DPRINTF(TLB, "In real mode.\n");
        DPRINTF(TLB, "Translated %#x -> %#x.\n", vaddr, vaddr);
        req->setPaddr(vaddr);
    }

    return finalizePhysical(req, tc, mode);
}

Fault
TLB::translateAtomic(RequestPtr req, ThreadContext *tc, Mode mode)
{
    bool delayedResponse;
    return TLB::translate(req, tc, NULL, mode, delayedResponse, false);
}

void
TLB::translateTiming(RequestPtr req, ThreadContext *tc,
        Translation *translation, Mode mode)
{
    bool delayedResponse;
    assert(translation);
    Fault fault =
        TLB::translate(req, tc, translation, mode, delayedResponse, true);
    if (!delayedResponse)
        translation->finish(fault, req, tc, mode);
}

Walker *
TLB::getWalker()
{
    return walker;
}

void
TLB::regStats()
{
    /*using namespace Stats;

    rdAccesses
        .name(name() + ".rdAccesses")
        .desc("TLB accesses on read requests");

    wrAccesses
        .name(name() + ".wrAccesses")
        .desc("TLB accesses on write requests");

    rdMisses
        .name(name() + ".rdMisses")
        .desc("TLB misses on read requests");

    wrMisses
        .name(name() + ".wrMisses")
        .desc("TLB misses on write requests");

	registerDumpCallback(new TLBDumpCallback(this));*/
	showStats();
	aprox_tot_instructs += 10000000;

}

void
TLB::showStats()
{
        if (strstr(name().c_str(), "system.detailed_cpu.dtb")) {

		//open stats file
		FILE *tlblog = fopen("tlblogs","a");

		//if file opens successfully
		if(tlblog)
		{
			//write to tlblog
			using namespace Stats;
			fprintf(tlblog,"============================================================\n");
			fprintf(tlblog,"%s\n",name().c_str());
			fprintf(tlblog,"Total TLB size:%d\n",L1size);


			if(totalaccessesL1 > 0LL && !simulateIceberg)
			{

				double total_accesses = (double)totalreadaccessL1 + (double)totalwriteaccessL1;
				double total_misses = (double)readmissesL1 + (double)writemissesL1;
				fprintf(tlblog,"approx CPU instructions %lld\n",aprox_tot_instructs);
				fprintf(tlblog,"total tlb accesses:%lld\n",totalaccessesL1);
				fprintf(tlblog,"total tlb misses:%lld\n",missesL1);
				fprintf(tlblog,"tlb miss rate:%.4f%%\n",((double)missesL1/(double)totalaccessesL1)*100);
				fprintf(tlblog,"total tlb-loads accesses:%lld\n",totalreadaccessL1);
				fprintf(tlblog,"total tlb-loads misses:%lld\n",readmissesL1);
				fprintf(tlblog,"tlb-loads miss rate:%.4f%%\n",((double)readmissesL1/(double)totalreadaccessL1)*100);
				fprintf(tlblog,"total tlb-stores accesses:%lld\n",totalwriteaccessL1);
				fprintf(tlblog,"total tlb-stores misses:%lld\n",writemissesL1);
				fprintf(tlblog,"tlb-stores miss rate:%.4f%%\n",((double)writemissesL1/(double)totalwriteaccessL1)*100);
				fprintf(tlblog,"Vanilla TLB miss rate:%.4f%%\n",((double)total_misses/(double)total_accesses)*100);
				fprintf(tlblog,"Vanilla TLB evicts %lld\n",lruevictsL1);
				fprintf(tlblog,"============================================================\n");
			}

			if (simulateIceberg) {
			    fprintf(tlblog,"============================================================\n");
			    fprintf(tlblog,"%s\n",name().c_str());


			    fprintf(tlblog,"Iceberg TLB size:%d\n",L1size);

			    if(totalaccessesIBL1 > 0LL) {

				double total_iceberg_accesses = (double)totalreadaccessIBL1 + (double)totalwriteaccessIBL1;
				double total_iceberg_misses = (double)readmissesIBL1 + (double)writemissesIBL1;
				fprintf(tlblog,"approx CPU instructions %lld\n",aprox_tot_instructs);
				fprintf(tlblog,"total Iceberg TLB accesses:%lld\n",totalaccessesIBL1);
				fprintf(tlblog,"total Iceberg TLB misses:%lld\n",missesIBL1);
				fprintf(tlblog,"Iceberg TLB miss rate:%.4f%%\n",((double)missesIBL1/(double)totalaccessesIBL1)*100);
				fprintf(tlblog,"total Iceberg tlb-loads accesses:%lld\n",totalreadaccessIBL1);
				fprintf(tlblog,"total Iceberg tlb-loads misses:%lld\n",readmissesIBL1);
				fprintf(tlblog,"Iceberg tlb-loads miss rate:%.4f%%\n",((double)readmissesIBL1/(double)totalreadaccessIBL1)*100);
				fprintf(tlblog,"total Iceberg tlb-stores accesses:%lld\n",totalwriteaccessIBL1);
				fprintf(tlblog,"total Iceberg tlb-stores misses:%lld\n",writemissesIBL1);
				fprintf(tlblog,"Iceberg tlb-stores miss rate:%.4f%%\n",((double)writemissesIBL1/(double)totalwriteaccessIBL1)*100);
				fprintf(tlblog,"Mosaic TLB miss rate:%.4f%%\n",((double)total_iceberg_misses/(double)total_iceberg_accesses)*100);
				fprintf(tlblog,"Mosaic TLB evicts %lld",lruIBevictsL1);
				fprintf(tlblog,"============================================================\n");
			    }
			}

			if(totalaccessesL2 > 0LL)
			{
				fprintf(tlblog,"L2 total tlb accesses:%lld\n",totalaccessesL2);
				fprintf(tlblog,"L2 total tlb misses:%lld\n",missesL2);
				fprintf(tlblog,"L2 tlb miss rate:%.4f%%\n",((double)missesL2/(double)totalaccessesL2)*100);
				fprintf(tlblog,"L2 total tlb-loads accesses:%lld\n",totalreadaccessL2);
				fprintf(tlblog,"L2 total tlb-loads misses:%lld\n",readmissesL2);
				fprintf(tlblog,"L2 tlb-loads miss rate:%.4f%%\n",((double)readmissesL2/(double)totalreadaccessL2)*100);
				fprintf(tlblog,"L2 total tlb-stores accesses:%lld\n",totalwriteaccessL2);
				fprintf(tlblog,"L2 total tlb-stores misses:%lld\n",writemissesL2);
				fprintf(tlblog,"L2 tlb-stores miss rate:%.4f%%\n",((double)writemissesL2/(double)totalwriteaccessL2)*100);
				fprintf(tlblog,"L2 tlb evicts %lld\n",lruevictsL2);
				fprintf(tlblog,"============================================================\n");
			}
		}
		//close stats file
		fclose(tlblog);
	}

    //resetting stats
    /*totalaccessesL1 = 0LL;
    missesL1 = 0LL;
    totalreadaccessL1 = 0LL;
    totalwriteaccessL1 = 0LL;
    readmissesL1 = 0LL;
    writemissesL1 = 0LL;
    insertsL1 = 0LL;
    lruevictsL1 = 0LL;

    totalaccessesIBL1 = 0LL;
    missesIBL1 = 0LL;
    totalreadaccessIBL1 = 0LL;
    totalwriteaccessIBL1 = 0LL;
    readmissesIBL1 = 0LL;
    writemissesIBL1 = 0LL;

    totalaccessesL2 = 0LL;
	missesL2 = 0LL;
    totalreadaccessL2 = 0LL;
    totalwriteaccessL2 = 0LL;
    readmissesL2 = 0LL;
    writemissesL2 = 0LL;
    lruevictsL2 = 0LL;

    flushAll();*/
}

void
TLB::serialize(CheckpointOut &cp) const
{
    // Only store the entries in use.
    uint32_t _size = L1size - L1freeList.size();
    SERIALIZE_SCALAR(_size);
    SERIALIZE_SCALAR(L1lruSeq);

    uint32_t _count = 0;
    for (uint32_t x = 0; x < L1size; x++) {
        if (L1tlb[x].trieHandle != NULL)
            L1tlb[x].serializeSection(cp, csprintf("Entry%d", _count++));
    }
}

void
TLB::unserialize(CheckpointIn &cp)
{
    // Do not allow to restore with a smaller tlb.
    uint32_t _size;
    UNSERIALIZE_SCALAR(_size);
    if (_size > L1size) {
        fatal("TLB size less than the one in checkpoint!");
    }

    UNSERIALIZE_SCALAR(L1lruSeq);

    for (uint32_t x = 0; x < _size; x++) {
        TlbEntry *newEntry = L1freeList.front();
        L1freeList.pop_front();

        newEntry->unserializeSection(cp, csprintf("Entry%d", x));
        newEntry->trieHandle = L1trie.insert(newEntry->vaddr,
            TlbEntryTrie::MaxBits - newEntry->logBytes, newEntry);
    }
}

BaseMasterPort *
TLB::getMasterPort()
{
    return &walker->getMasterPort("port");
}

} // namespace X86ISA

X86ISA::TLB *
X86TLBParams::create()
{
    return new X86ISA::TLB(this);
}
