/*
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

#ifndef __ARCH_X86_TLB_HH__
#define __ARCH_X86_TLB_HH__

#include <list>
#include <vector>

#include "arch/generic/tlb.hh"
#include "arch/x86/pagetable.hh"
#include "base/trie.hh"
#include "mem/request.hh"
#include "params/X86TLB.hh"
#include "base/callback.hh"

#define PAGEWIDTH  12
#define VADDRWIDTH 48

class ThreadContext;

namespace X86ISA
{
    class Walker;

    class TLB : public BaseTLB
    {
      protected:
        friend class Walker;

        typedef std::list<TlbEntry *> EntryList;

        uint32_t configAddress;

      public:

        typedef X86TLBParams Params;
        TLB(const Params *p);

        void takeOverFrom(BaseTLB *otlb) override {}

        int getSetNumber(Addr va);

        TlbEntry *lookupL1(Addr va, bool update_lru = true);
        TlbEntry *lookupIBL1(Addr va, bool update_lru = true);
        TlbEntry *lookupL2(Addr va, bool update_lru = true);

        TlbEntry *lookupL1SetAssociative(Addr va, bool update_lru = true);
        TlbEntry *lookupIBL1SetAssociative(Addr va, bool update_lru = true);

        void setConfigAddress(uint32_t addr);

      protected:

        EntryList::iterator lookupIt(Addr va, bool update_lru = true);

        Walker * walker;

      public:
        Walker *getWalker();

        void flushAll() override;

        void flushNonGlobal();

        bool tocAllInvalid(uint8_t *toc);
        void demapPage(Addr va, uint64_t asn) override;

      protected:
      	//variables for L1 Vanilla TLB
        uint32_t L1size;
        std::vector<TlbEntry> L1tlb;
        EntryList L1freeList;
        TlbEntryTrie L1trie;
        uint64_t L1lruSeq;

      	//variables for L1 Iceberg TLB
        uint32_t L1IBsize;
        std::vector<TlbEntry> L1IBtlb;
        EntryList L1IBfreeList;
        TlbEntryTrie L1IBtrie;
        uint64_t L1IBlruSeq;

        //variables for L2 TLB
        uint32_t L2size;
        std::vector<TlbEntry> L2tlb;
        EntryList L2freeList;
        TlbEntryTrie L2trie;
        uint64_t L2lruSeq;

        uint32_t set_associativity_L1;
        int numcachelines;

        //variables for set associative L1 vanilla TLB
        struct set_associative_tlb {
            uint32_t size;
            std::vector<TlbEntry> entries;
            EntryList freeList;
            TlbEntryTrie trie;
            uint64_t lruSeq;
        };

        //variable for set associative L1 iceberg TLB
        std::vector<struct set_associative_tlb> set_associative_tlb_entries_iceberg;

        //variable for set associative L1 vanilla TLB
        std::vector<struct set_associative_tlb> set_associative_tlb_entries;

        //flag to check if iceberg tlb needed
        bool simulateIceberg;

        //toc details for iceberg
        int toc_size, toc_bits;

        // Statistics
        Stats::Scalar rdAccesses;
        Stats::Scalar wrAccesses;
        Stats::Scalar rdMisses;
        Stats::Scalar wrMisses;

	long long int aprox_tot_instructs;
        long long int totalaccessesL1;
        long long int totalreadaccessL1;
        long long int totalwriteaccessL1;
        long long int missesL1;
        long long int readmissesL1;
        long long int writemissesL1;
        long long int insertsL1;
        long long int lruevictsL1;
        long long int lruIBevictsL1;

        //iceberg stats
        long long int totalaccessesIBL1;
        long long int totalreadaccessIBL1;
        long long int totalwriteaccessIBL1;
        long long int missesIBL1;
        long long int readmissesIBL1;
        long long int writemissesIBL1;

        long long int totalaccessesL2;
        long long int totalreadaccessL2;
        long long int totalwriteaccessL2;
        long long int missesL2;
        long long int readmissesL2;
        long long int writemissesL2;
        long long int lruevictsL2;

        Fault translateInt(RequestPtr req, ThreadContext *tc);

        Fault translate(RequestPtr req, ThreadContext *tc,
                Translation *translation, Mode mode,
                bool &delayedResponse, bool timing,
                bool isIcebergCheckNeeded = true,
                bool isIcebergPageWalkNeeded = true);

        void update_iceberg_stats(Mode mode);

      public:

        void evictLRUL1();
        void evictLRUIBL1();
        void evictLRUL2();

        void evictLRUL1SetAssociative(int setNumber);
        void evictLRUIBL1SetAssociative(int setNumber);

        uint64_t
        nextSeqL1()
        {
            return ++L1lruSeq;
        }

        uint64_t
        nextSeqIBL1()
        {
            return ++L1IBlruSeq;
        }

        uint64_t
        nextSeqL2()
        {
            return ++L2lruSeq;
        }

        Fault translateAtomic(
            RequestPtr req, ThreadContext *tc, Mode mode) override;
        void translateTiming(
            RequestPtr req, ThreadContext *tc,
            Translation *translation, Mode mode) override;

        /**
         * Do post-translation physical address finalization.
         *
         * Some addresses, for example requests going to the APIC,
         * need post-translation updates. Such physical addresses are
         * remapped into a "magic" part of the physical address space
         * by this method.
         *
         * @param req Request to updated in-place.
         * @param tc Thread context that created the request.
         * @param mode Request type (read/write/execute).
         * @return A fault on failure, NoFault otherwise.
         */
        Fault finalizePhysical(RequestPtr req, ThreadContext *tc,
                               Mode mode) const override;

        TlbEntry *insertL1(Addr vpn, const TlbEntry &entry);
        TlbEntry *insertIBL1(Addr vpn, const TlbEntry &entry);
        TlbEntry *insertL2(Addr vpn, const TlbEntry &entry);

        TlbEntry *insertL1SetAssociative(Addr vpn, const TlbEntry &entry);
        TlbEntry *insertIBL1SetAssociative(Addr vpn, const TlbEntry &entry);

        TlbEntry *insert_wrapper(Addr vpn, const TlbEntry &entry, bool is_iceberg);

        /*
         * Function to register Stats
         */
        void regStats() override;
        void showStats();

        // Checkpointing
        void serialize(CheckpointOut &cp) const override;
        void unserialize(CheckpointIn &cp) override;

        /**
         * Get the table walker master port. This is used for
         * migrating port connections during a CPU takeOverFrom()
         * call. For architectures that do not have a table walker,
         * NULL is returned, hence the use of a pointer rather than a
         * reference. For X86 this method will always return a valid
         * port pointer.
         *
         * @return A pointer to the walker master port
         */
        BaseMasterPort *getMasterPort() override;
    };

    class TLBDumpCallback : public Callback
   	{
   			TLB *tlb;
   		public:
   			TLBDumpCallback(TLB *t) : tlb(t) {}
   			virtual void process() { tlb->showStats(); }
   	};
}

#endif // __ARCH_X86_TLB_HH__
