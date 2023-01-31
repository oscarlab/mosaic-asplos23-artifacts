#include "arch/x86/pagetablewalker_cache.hh"

namespace X86ISA {

PageWalkerCache::PageWalkerCache()
{
	num_entries = PWC_TOTAL_ENTRIES;
	assoc = PWC_ASSOC;
	CachedEntries = (PWC_Entry **)malloc(num_entries);

	int i;
	for(i=0;i<num_entries;i++)
	{
		CachedEntries[i] = (PWC_Entry *)malloc(assoc * sizeof(PWC_Entry));
		int j;
		for(j=0;j<assoc;j++)
			CachedEntries[i][j].valid = false;
	}
}

PageWalkerCache::PageWalkerCache(int numEntries, int associativity)
{
	num_entries = numEntries;
	assoc = associativity;
	CachedEntries = (PWC_Entry **)malloc(num_entries * sizeof(PWC_Entry *));

	int i;
	for(i=0;i<num_entries;i++)
	{
		CachedEntries[i] = (PWC_Entry *)malloc(assoc * sizeof(PWC_Entry));
		int j;
		for(j=0;j<assoc;j++)
			CachedEntries[i][j].valid = false;
	}
}

int
PageWalkerCache::getIndex(int searchbits)
{
	return searchbits % num_entries;
}

PWC_Entry *
PageWalkerCache::lookup(int searchbits)
{
	return CachedEntries[getIndex(searchbits)];
}

bool
PageWalkerCache::presentInCache(int searchbits, PageTableEntry base)
{
	bool retVal = false;
	PWC_Entry* activated_entry = lookup(searchbits);
	int i;
	for(i=0;i<assoc;i++)
	{
		if(activated_entry[i].valid && activated_entry[i].base == base && activated_entry[i].index == searchbits)
		{
			retVal = true;
			break;
		}
	}
	return retVal;
}

PageTableEntry
PageWalkerCache::getnextfromCache(int searchbits, PageTableEntry base)
{
	PWC_Entry entryToRet;
	entryToRet.valid = false;
	entryToRet.next = 0;

	PWC_Entry* activated_entry = lookup(searchbits);
	int i;
	for(i=0;i<assoc;i++)
		if(activated_entry[i].valid && activated_entry[i].base == base && activated_entry[i].index == searchbits)
		{
			entryToRet = activated_entry[i];
			//lru in assoc list
			while(i>0)
			{
				activated_entry[i] = activated_entry[i-1];
				i--;
			}
			activated_entry[0] = entryToRet;
			break;
		}
	return entryToRet.next;
}

void
PageWalkerCache::update(PageTableEntry next, int searchbits, PageTableEntry base)
{
	PWC_Entry newEntry;
	newEntry.valid = true;
	newEntry.base = base;
	newEntry.index = searchbits;
	newEntry.next = next;
	int index = getIndex(searchbits);
	int i = assoc - 1;
	while(i>0)
	{
		CachedEntries[index][i] = CachedEntries[index][i-1];
		i--;
	}
	CachedEntries[index][0] = newEntry;
}

void
PageWalkerCache::invalidate(int searchbits)
{
	int i;
	for(i=0;i<assoc;i++)
		CachedEntries[getIndex(searchbits)][i].valid = false;
}

void
PageWalkerCache::remove(int searchbits, PageTableEntry base)
{
	if(presentInCache(searchbits,base))
	{
		PWC_Entry* activated_entry = lookup(searchbits);
		int i;
		for(i=0;i<assoc;i++)
		{
			if(activated_entry[i].valid && activated_entry[i].base == base && activated_entry[i].index == searchbits)
			{
				while(i<assoc-1)
				{
					activated_entry[i] = activated_entry[i+1];
					i++;
				}
				activated_entry[i].valid = false;
				break;
			}
		}
	}
}

void
PageWalkerCache::invalidateAll()
{
	int i,j;
	for(i=0;i<num_entries;i++)
		for(j=0;j<assoc;j++)
			CachedEntries[i][j].valid = false;
}

int
PageWalkerCache::get_numentries()
{
	return num_entries;
}

int
PageWalkerCache::get_assoc()
{
	return assoc;
}

}
