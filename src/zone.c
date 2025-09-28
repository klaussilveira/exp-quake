/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "quakedef.h"

#define DYNAMIC_SIZE 0xc000
#define ZONEID 0x1d4a11

//============================================================================
// Simple Zone Allocator
//============================================================================

/*
 * Our blocks look like:
 *   [ZONEID][user data...]
 */
static inline int* Z_GetHeader(void* ptr)
{
    return ((int*)ptr) - 1;
}

/*
========================
Z_Free
========================
*/
void Z_Free(void* ptr)
{
    if (!ptr) {
        return;
    }

    int* zblock = Z_GetHeader(ptr);
    if (zblock[0] != ZONEID) {
        Sys_Error("Z_Free: freed a pointer without ZONEID");
    }

    free(zblock);
}

/*
========================
Z_Malloc
========================
*/
void* Z_Malloc(int size)
{
    int* zblock = (int*)malloc(size + sizeof(int));
    if (!zblock) {
        Sys_Error("Z_Malloc: failed on allocation of %i bytes", size);
    }

    memset(zblock, 0, size + sizeof(int));
    zblock[0] = ZONEID;

    return (zblock + 1);
}

/*
========================
Z_Realloc
========================
*/
void* Z_Realloc(void* ptr, int new_size)
{
    if (!ptr) {
        return Z_Malloc(new_size);
    }

    int* oldblock = Z_GetHeader(ptr);
    if (oldblock[0] != ZONEID) {
        Sys_Error("Z_Realloc: pointer missing ZONEID");
    }

    int* newblock = (int*)realloc(oldblock, new_size + sizeof(int));
    if (!newblock) {
        Sys_Error("Z_Realloc: failed to allocate %d bytes", new_size);
    }

    newblock[0] = ZONEID;

    return newblock + 1;
}

//============================================================================
// Hunk Allocator
//============================================================================

#define HUNK_SENTINAL 0x1df001ed

typedef struct
{
    int sentinal;
    int size;
    char name[8];
} hunk_t;

typedef struct hunk_type_s {
    byte* hunkbuffer;
    int maxmb;
    int lowmark;
    int used;
} hunk_type_t;

hunk_type_t high_hunk = { NULL, 32 * 1024 * 1024, 0, 0 };
hunk_type_t low_hunk = { NULL, 1024 * 1024 * 1024, 0, 0 };

void* hunk_tempptr = NULL;

void Hunk_TempFree(void)
{
    if (hunk_tempptr) {
        free(hunk_tempptr);
        hunk_tempptr = NULL;
    }
}

void* Hunk_TempAlloc(int size)
{
    Hunk_TempFree();
    hunk_tempptr = malloc(size);

    return hunk_tempptr;
}

void* Hunk_TypeAlloc(hunk_type_t* ht, int size, char* name)
{
    hunk_t* hp;

    if (!ht->hunkbuffer) {
        ht->hunkbuffer = mmap(NULL, ht->maxmb, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ht->hunkbuffer == MAP_FAILED) {
            Sys_Error("Hunk_TypeAlloc: mmap failed for %d bytes (errno=%d)", ht->maxmb, errno);
        }

        ht->lowmark = 0;
        ht->used = 0;
    }

    size = sizeof(hunk_t) + ((size + 15) & ~15);

    while (ht->lowmark + size >= ht->used) {
        if (mprotect(ht->hunkbuffer + ht->used, 65536, PROT_READ | PROT_WRITE) != 0) {
            Sys_Error("Hunk_TypeAlloc: mprotect commit failed at offset %d (errno=%d)", ht->used, errno);
        }

        ht->used += 65536;
    }

    hp = (hunk_t*)(ht->hunkbuffer + ht->lowmark);
    ht->lowmark += size;

    memcpy(hp->name, name, 8);
    hp->sentinal = HUNK_SENTINAL;
    hp->size = size;

    memset((hp + 1), 0, size - sizeof(hunk_t));

    return (hp + 1);
}

void Hunk_TypeFreeToLowMark(hunk_type_t* ht, int mark)
{
    memset(ht->hunkbuffer + mark, 0, ht->used - mark);
    ht->lowmark = mark;
}

int Hunk_TypeLowMark(hunk_type_t* ht)
{
    return ht->lowmark;
}

void Hunk_Check(void)
{
}

void* Hunk_AllocName(int size, char* name)
{
    return Hunk_TypeAlloc(&low_hunk, size, name);
}

void* Hunk_Alloc(int size)
{
    return Hunk_TypeAlloc(&low_hunk, size, "unused");
}

int Hunk_LowMark(void)
{
    return Hunk_TypeLowMark(&low_hunk);
}

void Hunk_FreeToLowMark(int mark)
{
    Hunk_TypeFreeToLowMark(&low_hunk, mark);
}

int Hunk_HighMark(void)
{
    Hunk_TempFree();

    return Hunk_TypeLowMark(&high_hunk);
}

void Hunk_FreeToHighMark(int mark)
{
    Hunk_TempFree();
    Hunk_TypeFreeToLowMark(&high_hunk, mark);
}

void* Hunk_HighAllocName(int size, char* name)
{
    Hunk_TempFree();

    return Hunk_TypeAlloc(&high_hunk, size, name);
}

//============================================================================
// Cache Allocator (unchanged)
//============================================================================

typedef struct cache_system_s {
    int size;
    cache_user_t* user;
    char name[MAX_OSPATH];
    struct cache_system_s* next;
    struct cache_system_s* prev;
} cache_system_t;

cache_system_t* cache_head = NULL;
cache_system_t* cache_tail = NULL;

void* Cache_Check(cache_user_t* c)
{
    return c->data;
}

void Cache_Free(cache_user_t* c)
{
    cache_system_t* cs = ((cache_system_t*)c) - 1;

    if (cs->prev) {
        cs->prev->next = cs->next;
    } else {
        cache_head = cs->next;
    }

    if (cs->next) {
        cs->next->prev = cs->prev;
    }

    cache_tail = cs->prev;

    cs->user->data = NULL;
    free(cs);
}

void* Cache_Alloc(cache_user_t* c, int size, char* name)
{
    cache_system_t* cs = NULL;

    for (cs = cache_head; cs; cs = cs->next) {
        if (!strcmp(cs->name, name)) {
            return cs->user->data;
        }
    }

    if (c->data) {
        Sys_Error("Cache_Alloc: already allocated");
    }

    if (size <= 0) {
        Sys_Error("Cache_Alloc: size %i", size);
    }

    cs = (cache_system_t*)malloc(sizeof(cache_system_t) + size);
    if (!cs) {
        Sys_Error("Cache_Alloc: malloc failed");
    }

    if (!cache_head) {
        cache_head = cs;
        cs->prev = NULL;
    } else {
        cache_tail->next = cs;
        cs->prev = cache_tail;
    }

    cache_tail = cs;
    cs->next = NULL;

    strncpy(cs->name, name, sizeof(cs->name) - 1);
    cs->name[sizeof(cs->name) - 1] = '\0';
    cs->size = size;
    cs->user = c;

    c->data = (cs + 1);

    return c->data;
}

//============================================================================
// Memory_Init
//============================================================================

void Memory_Init()
{
    free(host_parms.membase);
    host_parms.memsize = low_hunk.maxmb;
}
