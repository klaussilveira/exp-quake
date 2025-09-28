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

void Memory_Init();

void Z_Free(void* ptr);
void* Z_Malloc(int size); // returns 0 filled memory
void* Z_Realloc(void* ptr, int new_size);

void* Hunk_Alloc(int size); // returns 0 filled memory
void* Hunk_AllocName(int size, char* name);

void* Hunk_HighAllocName(int size, char* name);

int Hunk_LowMark(void);
void Hunk_FreeToLowMark(int mark);

int Hunk_HighMark(void);
void Hunk_FreeToHighMark(int mark);

void* Hunk_TempAlloc(int size);

void Hunk_Check(void);

typedef struct cache_user_s {
    void* data;
} cache_user_t;

void Cache_Flush(void);

void* Cache_Check(cache_user_t* c);
// returns the cached data, and moves to the head of the LRU list
// if present, otherwise returns NULL

void Cache_Free(cache_user_t* c);

void* Cache_Alloc(cache_user_t* c, int size, char* name);
// Returns NULL if all purgable data was tossed and there still
// wasn't enough room.
