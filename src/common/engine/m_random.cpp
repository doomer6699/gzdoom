/*
** m_random.cpp
** Random number generators
**
**---------------------------------------------------------------------------
** Copyright 2002-2009 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** This file employs the techniques for improving demo sync and backward
** compatibility that Lee Killough introduced with BOOM. However, none of
** the actual code he wrote is left. In contrast to BOOM, each RNG source
** in ZDoom is implemented as a separate class instance that provides an
** interface to the high-quality Mersenne Twister. See
** <http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/index.html>.
**
** As Killough's description from m_random.h is still mostly relevant,
** here it is:
**   killough 2/16/98:
**
**   Make every random number generator local to each control-equivalent block.
**   Critical for demo sync. The random number generators are made local to
**   reduce the chances of sync problems. In Doom, if a single random number
**   generator call was off, it would mess up all random number generators.
**   This reduces the chances of it happening by making each RNG local to a
**   control flow block.
**
**   Notes to developers: if you want to reduce your demo sync hassles, follow
**   this rule: for each call to P_Random you add, add a new class to the enum
**   type below for each block of code which calls P_Random. If two calls to
**   P_Random are not in "control-equivalent blocks", i.e. there are any cases
**   where one is executed, and the other is not, put them in separate classes.
*/

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#include "doomstat.h"
#include "doomdef.h"
#include "m_random.h"
#include "serializer.h"
#include "m_crc32.h"
#include "c_dispatch.h"
#include "printf.h"

// MACROS ------------------------------------------------------------------

#define RAND_ID MAKE_ID('r','a','N','d')

// TYPES -------------------------------------------------------------------

// Doom's original random number generator.

//
// M_Random
// Returns a 0-255 number
//
unsigned char rndtable[256] = {
	0,	 8, 109, 220, 222, 241, 149, 107,  75, 248, 254, 140,  16,	66 ,
	74,  21, 211,  47,	80, 242, 154,  27, 205, 128, 161,  89,	77,  36 ,
	95, 110,  85,  48, 212, 140, 211, 249,	22,  79, 200,  50,	28, 188 ,
	52, 140, 202, 120,	68, 145,  62,  70, 184, 190,  91, 197, 152, 224 ,
	149, 104,  25, 178, 252, 182, 202, 182, 141, 197,	4,	81, 181, 242 ,
	145,  42,  39, 227, 156, 198, 225, 193, 219,  93, 122, 175, 249,   0 ,
	175, 143,  70, 239,  46, 246, 163,	53, 163, 109, 168, 135,   2, 235 ,
	25,  92,  20, 145, 138,  77,  69, 166,	78, 176, 173, 212, 166, 113 ,
	94, 161,  41,  50, 239,  49, 111, 164,	70,  60,   2,  37, 171,  75 ,
	136, 156,  11,	56,  42, 146, 138, 229,  73, 146,  77,	61,  98, 196 ,
	135, 106,  63, 197, 195,  86,  96, 203, 113, 101, 170, 247, 181, 113 ,
	80, 250, 108,	7, 255, 237, 129, 226,	79, 107, 112, 166, 103, 241 ,
	24, 223, 239, 120, 198,  58,  60,  82, 128,   3, 184,  66, 143, 224 ,
	145, 224,  81, 206, 163,  45,  63,	90, 168, 114,  59,	33, 159,  95 ,
	28, 139, 123,  98, 125, 196,  15,  70, 194, 253,  54,  14, 109, 226 ,
	71,  17, 161,  93, 186,  87, 244, 138,	20,  52, 123, 251,	26,  36 ,
	17,  46,  52, 231, 232,  76,  31, 221,	84,  37, 216, 165, 212, 106 ,
	197, 242,  98,	43,  39, 175, 254, 145, 190,  84, 118, 222, 187, 136 ,
	120, 163, 236, 249
};

int 	prndindex = 0;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

FRandom pr_exrandom("EX_Random", false, false);

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FCRandom M_Random;

// Global seed. This is modified predictably to initialize every RNG.
uint32_t rngseed;

// Static RNG marker. This is only used when the RNG is set for each new game.
uint32_t staticrngseed;
bool use_staticrng;

// Allows checking or staticly setting the global seed.
CCMD(rngseed)
{
	if (argv.argc() == 1)
	{
		Printf("Usage: rngseed get|set|clear\n");
		return;
	}
	if (stricmp(argv[1], "get") == 0)
	{
		Printf("rngseed is %d\n", rngseed);
	}
	else if (stricmp(argv[1], "set") == 0)
	{
		if (argv.argc() == 2)
		{
			Printf("You need to specify a value to set\n");
		}
		else
		{
			staticrngseed = atoi(argv[2]);
			use_staticrng = true;
			Printf("Static rngseed %d will be set for next game\n", staticrngseed);
		}
	}
	else if (stricmp(argv[1], "clear") == 0)
	{
		use_staticrng = false;
		Printf("Static rngseed cleared\n");
	}
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

FRandom *FRandom::RNGList, *FRandom::CRNGList;
static TDeletingArray<FRandom *> NewRNGs, NewCRNGs;

// CODE --------------------------------------------------------------------

unsigned int P_Random (void)
{
	prndindex = (prndindex+1)&0xff;
	return rndtable[prndindex];
}

void M_ClearRandom (void)
{
	prndindex = 0;
}

//==========================================================================
//
// FRandom - Nameless constructor
//
// Constructing an RNG in this way means it won't be stored in savegames.
//
//==========================================================================

FRandom::FRandom (bool client)
: NameCRC (0), bClient(client), useOldRNG (false)
{
#ifndef NDEBUG
	Name = NULL;
#endif
	if (bClient)
	{
		Next = CRNGList;
		CRNGList = this;
	}
	else
	{
		Next = RNGList;
		RNGList = this;
	}
	Init(0);
}

//==========================================================================
//
// FRandom - Named constructor
//
// This is the standard way to construct RNGs.
//
//==========================================================================

FRandom::FRandom (const char *name, bool client, bool useold) : bClient(client), useOldRNG(useold)
{
	NameCRC = CalcCRC32 ((const uint8_t *)name, (unsigned int)strlen (name));
#ifndef NDEBUG
	Name = name;
	// A CRC of 0 is reserved for nameless RNGs that don't get stored
	// in savegames. The chance is very low that you would get a CRC of 0,
	// but it's still possible.
	assert (NameCRC != 0);
#endif

	// Insert the RNG in the list, sorted by CRC
	FRandom **prev = (bClient ? &CRNGList : &RNGList), * probe = (bClient ? CRNGList : RNGList);

	while (probe != NULL && probe->NameCRC < NameCRC)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}

#ifndef NDEBUG
	if (probe != NULL)
	{
		// Because RNGs are identified by their CRCs in save games,
		// no two RNGs can have names that hash to the same CRC.
		// Obviously, this means every RNG must have a unique name.
		assert (probe->NameCRC != NameCRC);
	}
#endif

	Next = probe;
	*prev = this;
	Init(0);
}

//==========================================================================
//
// FRandom - Destructor
//
//==========================================================================

FRandom::~FRandom ()
{
	FRandom *rng, **prev;

	FRandom *last = NULL;

	prev = bClient ? &CRNGList : &RNGList;
	rng = bClient ? CRNGList : RNGList;

	while (rng != NULL && rng != this)
	{
		last = rng;
		rng = rng->Next;
	}

	if (rng != NULL)
	{
		*prev = rng->Next;
	}
}

//==========================================================================
//
// FRandom::GetRandom()
//
// Returns either an old PRNG value or an SFMT value
//
//==========================================================================

unsigned int FRandom::GetRandom()
{
	if (useOldRNG && (compatflags2 & COMPATF2_OLD_RANDOM_GENERATOR))
		return P_Random();
	else return GenRand32();
}

//==========================================================================
//
// FRandom :: StaticClearRandom
//
// Initialize every RNGs. RNGs are seeded based on the global seed and their
// name, so each different RNG can have a different starting value despite
// being derived from a common global seed.
//
//==========================================================================

void FRandom::StaticClearRandom ()
{
	// go through each RNG and set each starting seed differently
	for (FRandom *rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
	{
		rng->Init(rngseed);
	}

	for (FRandom* rng = FRandom::CRNGList; rng != NULL; rng = rng->Next)
	{
		rng->Init(rngseed);
	}
}

//==========================================================================
//
// FRandom :: Init
//
// Initialize a single RNG with a given seed.
//
//==========================================================================

void FRandom::Init(uint32_t seed)
{
	// [RH] Use the RNG's name's CRC to modify the original seed.
	// This way, new RNGs can be added later, and it doesn't matter
	// which order they get initialized in.
	SFMTObj::Init(NameCRC, seed);
}

//==========================================================================
//
// FRandom :: StaticWriteRNGState
//
// Stores the state of every RNG into a savegame.
//
//==========================================================================

void FRandom::StaticWriteRNGState (FSerializer &arc)
{
	FRandom *rng;

	arc("rngseed", rngseed);

	if (arc.BeginArray("rngs"))
	{
		for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
		{
			// Only write those RNGs that have names
			if (rng->NameCRC != 0)
			{
				if (arc.BeginObject(nullptr))
				{
					arc("crc", rng->NameCRC)
						("index", rng->idx)
						.Array("u", rng->sfmt.u, SFMT::N32)
						.EndObject();
				}
			}
		}
		arc.EndArray();
	}
}

//==========================================================================
//
// FRandom :: StaticReadRNGState
//
// Restores the state of every RNG from a savegame. RNGs that were added
// since the savegame was created are cleared to their initial value.
//
//==========================================================================

void FRandom::StaticReadRNGState(FSerializer &arc)
{
	FRandom *rng;

	arc("rngseed", rngseed);

	// Call StaticClearRandom in order to ensure that SFMT is initialized
	FRandom::StaticClearRandom ();

	if (arc.BeginArray("rngs"))
	{
		int count = arc.ArraySize();

		for (int i = 0; i < count; i++)
		{
			if (arc.BeginObject(nullptr))
			{
				uint32_t crc;
				arc("crc", crc);

				for (rng = FRandom::RNGList; rng != NULL; rng = rng->Next)
				{
					if (rng->NameCRC == crc)
					{
						arc("index", rng->idx)
							.Array("u", rng->sfmt.u, SFMT::N32);
						break;
					}
				}
				arc.EndObject();
			}
		}
		arc.EndArray();
	}
}

//==========================================================================
//
// FRandom :: StaticFindRNG
//
// This function attempts to find an RNG with the given name.
// If it can't it will create a new one. Duplicate CRCs will
// be ignored and if it happens map to the same RNG.
// This is for use by DECORATE.
//
//==========================================================================

FRandom *FRandom::StaticFindRNG (const char *name, bool client)
{
	uint32_t NameCRC = CalcCRC32 ((const uint8_t *)name, (unsigned int)strlen (name));

	// Use the default RNG if this one happens to have a CRC of 0.
	if (NameCRC == 0) return client ? &M_Random : &pr_exrandom;

	// Find the RNG in the list, sorted by CRC
	FRandom **prev = (client ? &CRNGList : &RNGList), *probe = (client ? CRNGList : RNGList);

	while (probe != NULL && probe->NameCRC < NameCRC)
	{
		prev = &probe->Next;
		probe = probe->Next;
	}
	// Found one so return it.
	if (probe == NULL || probe->NameCRC != NameCRC)
	{
		// A matching RNG doesn't exist yet so create it.
		probe = new FRandom(name, client);

		// Store the new RNG for destruction when ZDoom quits.
		if (client)
			NewCRNGs.Push(probe);
		else
			NewRNGs.Push(probe);
	}
	return probe;
}

void FRandom::SaveRNGState(TArray<FRandom>& backups)
{
	for (auto cur = RNGList; cur != nullptr; cur = cur->Next)
		backups.Push(*cur);
}

void FRandom::RestoreRNGState(TArray<FRandom>& backups)
{
	unsigned int i = 0u;
	for (auto cur = RNGList; cur != nullptr; cur = cur->Next)
		*cur = backups[i++];

	backups.Clear();
}

//==========================================================================
//
// FRandom :: StaticPrintSeeds
//
// Prints a snapshot of the current RNG states. This is probably wrong.
//
//==========================================================================

#ifndef NDEBUG
void FRandom::StaticPrintSeeds ()
{
	FRandom *rng = RNGList;

	while (rng != NULL)
	{
		int idx = rng->idx < SFMT::N32 ? rng->idx : 0;
		Printf ("%s: %08x .. %d\n", rng->Name, rng->sfmt.u[idx], idx);
		rng = rng->Next;
	}
}

CCMD (showrngs)
{
	FRandom::StaticPrintSeeds ();
}
#endif

