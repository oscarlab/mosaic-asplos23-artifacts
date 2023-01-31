#include "arch/x86/pagetable.hh"

#define PWC_TOTAL_ENTRIES 512
#define PWC_ASSOC         4

namespace X86ISA
{
	struct PWC_Entry
	{
		//valid bit
		bool valid;

		//base and index together make up tag
		PageTableEntry base;
		int index;

		//cached entry
		PageTableEntry next;
	};

	class PageWalkerCache
	{
		private:
			int num_entries;
			int assoc;
			PWC_Entry **CachedEntries;
			int getIndex(int searchbits);
			PWC_Entry *lookup(int searchbits);
		public:
			PageWalkerCache();
			PageWalkerCache(int numEntries, int associativity);
			bool presentInCache(int searchbits, PageTableEntry base);
			PageTableEntry getnextfromCache(int searchbits, PageTableEntry base);
			void update(PageTableEntry newEntry, int searchbits, PageTableEntry base);
			void remove(int searchbits, PageTableEntry base);
			void invalidate(int searchbits);
			void invalidateAll();
			int get_numentries();
			int get_assoc();
			int get_indexbits();
	};
}
