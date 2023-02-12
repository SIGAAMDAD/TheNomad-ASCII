//----------------------------------------------------------
//
// Copyright (C) SIGAAMDAD 2022-2023
//
// This source is available for distribution and/or modification
// only under the terms of the SACE Source Code License as
// published by SIGAAMDAD. All rights reserved
//
// The source is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of FITNESS FOR A PARTICLAR PURPOSE. See the SACE
// Source Code License for more details. If you, however do not
// want to use the SACE Source Code License, then you must use
// this source as if it were to be licensed under the GNU General
// Public License (GPL) version 2.0 or later as published by the
// Free Software Foundation.
//
// DESCRIPTION:
//  src/g_zone.cpp
//  zone memory allocation, to prevent leaks from happening,
//  and to keep better track of allocations. It speeds stuff up
//  by allocating all the memory at the beginning, then returning
//  blocks of memory of desired size, no mallocs, callocs, or
//  reallocs (stdlib.h function calls that is) during the main
//  level loop.
//----------------------------------------------------------
#include "g_zone.h"

//
// If the program calls Z_Free on a block that doesn't have the ZONEID,
// meaning that it wasn't allocated via Z_Malloc, the allocater will
// throw an error
//

//
// When archiving and unarchiving save data, the allocater will scan the heap
// to check for heap corruption
//

//
// This allocater is a heavily modified version of z_zone.c and z_zone.h from
// varios DOOM source ports, credits to them and John Carmack/Romero for developing
// the system
//

#define MEM_ALIGN  (sizeof(void *))
#define UNOWNED    ((void *)666)
#define ZONEID     0xa21d49

// size: 40 bytes...?
typedef struct memblock_s
{
	int size;
	int tag;
	int id;
	void* user;
	
	struct memblock_s* next;
	struct memblock_s* prev;
} memblock_t;

typedef struct
{
	// total bytes allocated, including the sizeof memzone_t
	int size;

	// start/end cap for the linked list of blocks
	memblock_t blocklist;
	// the block pointer
	memblock_t* rover;
} memzone_t;

__CFUNC__ void Z_KillHeap();

memzone_t* mainzone;

__CFUNC__ void Z_KillHeap(void)
{
	free(mainzone);
}


static unsigned long log_size = 0; // once this reaches ~10 kb, it'll reset
static unsigned long free_size = 0; // same as above
static unsigned long numblocks = 0;

//
// Z_Free
//
__CFUNC__ void Z_Free(void *ptr)
{
	assert(ptr);
	memblock_t* block;
	memblock_t* other;
	
	block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));
	
	if (block->id != ZONEID) {
#ifndef _NOMAD_DEBUG
		ptr = nullptr;
		return;
#else
		N_Error("Z_Free: trying to free a pointer without ZONEID!\n");
#endif
	}
	free_size += block->size;
	if (free_size > 10000) {
		LOG_HEAP();
		free_size = 0;
	}
	if (block->tag != TAG_FREE && block->user)
		block->user = nullptr;
	if (block->user > (void *)0x100)
		block->user = nullptr;

	// mark as free
	block->user = nullptr;
	block->tag = TAG_FREE;
	block->id = 0;

	other = block->prev;
	--numblocks;

	if (other->tag == TAG_FREE) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == mainzone->rover) {
			mainzone->rover = other;
		}
		block = other;
	}

	other = block->next;
	if (other->tag == TAG_FREE) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		
		if (other == mainzone->rover) {
			mainzone->rover = block;
		}
	}
}
#ifdef TESTING
__CFUNC__ void Z_DumpHeap(void)
{
	memblock_t* block;
	fprintf(stdout, "zone size:%i   location:%p\n", mainzone->size, mainzone);
	for (block = mainzone->blocklist.next;; block = block->next) {
		fprintf (stdout, "block:%p     size:%7i     user:%p      tag:%3i\n",
			block, block->size, block->user, block->tag);
		if (block->next == &mainzone->blocklist) {
			// all blocks have been hit
			break;
		}
		if ((byte *)block+block->size != (byte *)block->next) {
			N_Error("Z_DumpHeap: block size doesn't touch next block!\n");
		}
		if (block->next->prev != block) {
			N_Error("Z_DumpHeap: next block doesn't have proper back linkage!\n");
		}
		if (!block->user && !block->next->user) {
			N_Error("Z_DumpHeap: two consecutive free blocks!\n");
		}
	}
}
#endif

#if defined(_NOMAD_DEBUG) || defined(TESTING)
__CFUNC__ void Z_FileDumpHeap(void)
{
	memblock_t* block;
	FILE* fp = fopen("Files/debug/heaplog.txt", "w");
	assert(fp);
	if (!fp)
		return;
	fprintf(fp, "zone size:%i   location:%p\n", mainzone->size, mainzone);
	for (block = mainzone->blocklist.next;; block = block->next) {
		fprintf (fp, "block:%p     size:%7i     user:%p      tag:%3i\n",
			block, block->size, block->user, block->tag);
		if (block->next == &mainzone->blocklist) {
			// all blocks have been hit
			break;
		}
		if ((byte *)block+block->size != (byte *)block->next) {
			fprintf(fp, "ERROR: block size doesn't touch next block!\n");
		}
		if (block->next->prev != block) {
			fprintf(fp, "ERROR: next block doesn't have proper back linkage!\n");
		}
		if (!block->user && !block->next->user) {
			fprintf(fp, "ERROR: two consecutive free blocks!\n");
		}
	}
	fclose(fp);
}
#endif

#ifdef TESTING
__CFUNC__ void Z_Init(int size)
{
	memblock_t* base;

	// account for header size
	size += sizeof(memzone_t);
	mainzone = (memzone_t *)((byte *)calloc(size, sizeof(byte)));
	if (!mainzone) {
		fprintf(stderr, "Z_Init: malloc failed!\n");
		exit(-1);
	}
	atexit(Z_KillHeap);
	mainzone->size = size;

	mainzone->blocklist.next = 
	mainzone->blocklist.prev = 
	base = (memblock_t *)((byte *)mainzone+sizeof(memzone_t));

	mainzone->blocklist.user = (void *)mainzone;
	mainzone->blocklist.tag = TAG_STATIC;
	mainzone->rover = base;

	base->prev = base->next = &mainzone->blocklist;
	base->user = nullptr;
	base->size = mainzone->size - sizeof(memzone_t);
	printf("Allocated Zone From %p -> %p\n", (void *)mainzone, (void *)(mainzone+mainzone->size));
}
#else
__CFUNC__ void Z_Init()
{
	memblock_t* base;
	mainzone = (memzone_t *)((byte *)calloc(heapsize, sizeof(byte)));
	if (!mainzone)
		N_Error("Z_Init: malloc failed!\n");
	
	PTR_CHECK(NULL_CHECK, mainzone);
	atexit(Z_KillHeap);
	mainzone->size = heapsize;
	
	mainzone->blocklist.next = 
	mainzone->blocklist.prev = 
	base = (memblock_t *)((byte *)mainzone+sizeof(memzone_t));

	mainzone->blocklist.user = (void *)mainzone;
	mainzone->blocklist.tag = TAG_STATIC;
	mainzone->rover = base;
	
	base->prev = base->next = &mainzone->blocklist;
	base->user = NULL;
	base->size = mainzone->size - sizeof(memzone_t);
	printf("Allocated Zone From %p -> %p\n", (void *)mainzone, (void *)(mainzone+mainzone->size));
	LOG_INFO("Initialzing Zone Allocation Daemon from addresses %p -> %p", (void *)mainzone, (void *)(mainzone+mainzone->size));
}
#endif

__CFUNC__ void Z_ClearZone(void)
{
	LOG_INFO("clearing zone");
	memblock_t*		block;
	
	// set the entire zone to one free block
	mainzone->blocklist.next =
	mainzone->blocklist.prev =
	block = (memblock_t *)( (byte *)mainzone + sizeof(memzone_t) );
	
	mainzone->blocklist.user = (void *)mainzone;
	mainzone->blocklist.tag = TAG_STATIC;
	mainzone->rover = block;
	
	block->prev = block->next = &mainzone->blocklist;
	
	// a free block.
	block->tag = TAG_FREE;
	
	block->size = mainzone->size - sizeof(memzone_t);
}

// Z_Malloc: garbage collection and zone block allocater that returns a block of free memory
// from within the zone without calling malloc
__CFUNC__ void* Z_Malloc(int size, int tag, void* user)
{
	assert(size > 0);
	assert(tag > -1);
	if (!user) {
		LOG_WARN("NULL user pointer given to Z_Malloc, returning NULL");
		return NULL;
	}
	memblock_t* rover;
	memblock_t* userblock;
	memblock_t* base;
	memblock_t* start;
	int space;

	size = (size + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1);
	
	// accounting for header size
	size += sizeof(memblock_t);
	
	base = mainzone->rover;
	
	// checking behind the rover
	if (!base->prev->user)
		base = base->prev;
	
	rover = base;
	start = base->prev;
	
	do {
		if (rover == start) {
			N_Error("Z_Malloc: failed on allocation of %i bytes because zone isn't\n"
				"big enough! zone size: %i\n", size, mainzone->size);
			return NULL;
		}
		if (rover->user) {
			if (rover->tag < TAG_PURGELEVEL) {
				// hit a block that can't be purged, so move the base past it
				base = rover = rover->next;
			}
			else {
				// free the rover block (adding to the size of the base)
				// the rover can be the base block
				base = base->prev;
				Z_Free((byte *)rover+sizeof(memblock_t));
				base = base->next;
				rover = base->next;
			}
		}
		else {
			rover = rover->next;
		}
	} while (base->user || base->size < size);
	
	space = base->size - size;
	
	if (space > 64) {
		userblock = (memblock_t *)((byte *)base+size);
		assert(userblock);
		userblock->size = space;
		userblock->user = nullptr;
		userblock->tag = TAG_FREE;
		userblock->prev = base;
		userblock->next = base->next;
		userblock->next->prev = userblock;
	
		base->next = userblock;
		base->size = size;
	}
	if (user) {
		// mark as an in use block
		base->user = user;
		user = (void *)((byte *)base+sizeof(memblock_t));
	}
	else {
		if (tag >= TAG_PURGELEVEL) {
			N_Error("Z_Malloc: an owner is required for purgable blocks\n");
			return nullptr;
		}
		// mark as in used, but unowned
		base->user = UNOWNED;
		LOG_WARN("block at %p is used, but currently unowned", base);
	}
	base->tag = tag;

	// next allocation will start looking here
	mainzone->rover = base->next;
	base->id = ZONEID;
	log_size += base->size;
	if (log_size > 10000) {
		LOG_HEAP();
		log_size = 0;
	}
	++numblocks;
	return (void *)((byte *)base+sizeof(memblock_t));
}

__CFUNC__ void* Z_Realloc(void *user, int nsize, int tag)
{
	PTR_CHECK(NULL_CHECK, user);
	void *ptr = Z_Malloc(nsize, tag, ptr);
	memblock_t* block = (memblock_t *)((byte *)user - sizeof(memblock_t));
	free_size += block->size;
	log_size += nsize;
	++numblocks;
	if (log_size > 10000) {
		LOG_HEAP();
		log_size = 0;
	}
	memcpy(ptr, user, nsize <= block->size ? nsize : block->size);
	Z_Free(user);
	return ptr;
}

__CFUNC__ void* Z_Calloc(void *user, int nelem, int elemsize, int tag)
{
	PTR_CHECK(NULL_CHECK, user);
	log_size += nelem * elemsize;
	++numblocks;
	if (log_size > 10000) {
		LOG_HEAP();
		log_size = 0;
	}
	return (nelem*=elemsize) ? memset((Z_Malloc)(nelem, tag, user), 0, nelem) : NULL;
}

__CFUNC__ void Z_FreeTags(int lowtag, int hightag)
{
	int numblocks = 0;
	int size = 0;
	memblock_t*	block;
    memblock_t*	next;
	
    for (block = mainzone->blocklist.next; 
		block != &mainzone->blocklist; block = next) {
		// get link before freeing
		next = block->next;
		
		// free block?
		if (block->tag == TAG_FREE) {
			continue;
		}
		if (block->tag >= lowtag && block->tag <= hightag) {
			++numblocks;
			size += block->size;
			Z_Free ((byte *)block+sizeof(memblock_t));
		}
	}
	LOG_FREETAGS(lowtag, hightag, numblocks, size);
}

__CFUNC__ void Z_CheckHeap(void)
{
	memblock_t* block;

	for (block = mainzone->blocklist.next;; block = block->next) {
		if (block->next == &mainzone->blocklist) {
			// all blocks have been hit
			break;
		}
		if ((byte *)block+block->size != (byte *)block->next) {
			N_Error("Z_CheckHeap: block size doesn't touch next block!\n");
		}
		if (block->next->prev != block) {
			N_Error("Z_CheckHeap: next block doesn't have proper back linkage!\n");
		}
		if (!block->user && !block->next->user) {
			N_Error("Z_CheckHeap: two consecutive free blocks!\n");
		}
	}
	LOG_INFO("heap check successful");
}
__CFUNC__ void Z_ChangeTag2(void *ptr, int tag, const char *file, int line)
{
	PTR_CHECK(NULL_CHECK, ptr);
	PTR_CHECK(NULL_CHECK, file);
    memblock_t*	block;
    block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));
    if (block->id != ZONEID)
        N_Error("%s:%i: Z_ChangeTag: block without a ZONEID!",
                file, line);

    if (tag >= TAG_PURGELEVEL && !block->user)
        N_Error("%s: %i: Z_ChangeTag: an owner is required "
                "for purgable blocks", file, line);

	LOG_INFO("changing tag of ptr %p to %i, old tag was %i", ptr, tag, block->tag);
    block->tag = tag;
}

__CFUNC__ void Z_ChangeUser(void *ptr, void *user)
{
	PTR_CHECK(NULL_CHECK, ptr);
	PTR_CHECK(NULL_CHECK, user);
	memblock_t*	block;
	
	block = (memblock_t *) ((byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		N_Error("Z_ChangeUser: tried to change user for invalid block!\n");
		return;
	}
	LOG_INFO("changing user of ptr %p to %p, old user was %p", ptr, user, block->user);
	block->user = user;
	user = ptr;
}

#ifndef TESTING
__CFUNC__ constexpr unsigned long Z_ZoneSize(void) { return heapsize; }
#endif
