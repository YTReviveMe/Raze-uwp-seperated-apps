/*
 ** i_system.mm
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2018 Alexey Lysiuk
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
 */

#include "i_common.h"

#include <fnmatch.h>
#include <sys/sysctl.h>

#include "gameconfigfile.h"
#include "i_sound.h"
#include "i_system.h"
#include "st_console.h"
#include "v_text.h"
#include "cmdlib.h"
#include "printf.h"


double PerfToSec, PerfToMillisec;

static void CalculateCPUSpeed()
{
	long long frequency;
	size_t size = sizeof frequency;

	if (0 == sysctlbyname("machdep.tsc.frequency", &frequency, &size, nullptr, 0) && 0 != frequency)
	{
		PerfToSec = 1.0 / frequency;
		PerfToMillisec = 1000.0 / frequency;

		//if (!batchrun)
		{
			Printf("CPU speed: %.0f MHz\n", 0.001 / PerfToMillisec);
		}
	}
}

void I_Init(void)
{
	CalculateCPUSpeed();
}

void I_SetIWADInfo()
{
}


void I_DebugPrint(const char *cp)
{
	NSLog(@"%s", cp);
}


void I_PrintStr(const char* const message)
{
	FConsoleWindow::GetInstance().AddText(message);

	// Strip out any color escape sequences before writing to output
	char* const copy = new char[strlen(message) + 1];
	const char* srcp = message;
	char* dstp = copy;

	while ('\0' != *srcp)
	{
		if (TEXTCOLOR_ESCAPE == *srcp)
		{
			if ('\0' != srcp[1])
			{
				srcp += 2;
			}
			else
			{
				break;
			}
		}
		else if (0x1d == *srcp || 0x1f == *srcp) // Opening and closing bar character
		{
			*dstp++ = '-';
			++srcp;
		}
		else if (0x1e == *srcp) // Middle bar character
		{
			*dstp++ = '=';
			++srcp;
		}
		else
		{
			*dstp++ = *srcp++;
		}
	}

	*dstp = '\0';

	fputs(copy, stdout);
	delete[] copy;
	fflush(stdout);
}


void Mac_I_FatalError(const char* const message);

void I_ShowFatalError(const char *message)
{
	Mac_I_FatalError(message);
}


int I_PickIWad(WadStuff* const wads, const int numwads, const bool showwin, const int defaultiwad)
{
	if (!showwin)
	{
		return defaultiwad;
	}

	I_SetMainWindowVisible(false);

	extern int I_PickIWad_Cocoa(WadStuff*, int, bool, int);
	const int result = I_PickIWad_Cocoa(wads, numwads, showwin, defaultiwad);

	I_SetMainWindowVisible(true);

	return result;
}


bool I_WriteIniFailed()
{
	printf("The config file %s could not be saved:\n%s\n", GameConfig->GetPathName(), strerror(errno));
	return false; // return true to retry
}


static const char *pattern;

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1080
static int matchfile(struct dirent *ent)
#else
static int matchfile(const struct dirent *ent)
#endif
{
	return fnmatch(pattern, ent->d_name, FNM_NOESCAPE) == 0;
}

void* I_FindFirst(const char* const filespec, findstate_t* const fileinfo)
{
	FString dir;
	
	const char* const slash = strrchr(filespec, '/');

	if (slash)
	{
		pattern = slash+1;
		dir = FString(filespec, slash - filespec + 1);
	}
	else
	{
		pattern = filespec;
		dir = ".";
	}

	fileinfo->current = 0;
	fileinfo->count = scandir(dir.GetChars(), &fileinfo->namelist, matchfile, alphasort);

	if (fileinfo->count > 0)
	{
		return fileinfo;
	}

	return (void*)-1;
}

int I_FindNext(void* const handle, findstate_t* const fileinfo)
{
	findstate_t* const state = static_cast<findstate_t*>(handle);

	if (state->current < fileinfo->count)
	{
		return ++state->current < fileinfo->count ? 0 : -1;
	}

	return -1;
}

int I_FindClose(void* const handle)
{
	findstate_t* const state = static_cast<findstate_t*>(handle);

	if (handle != (void*)-1 && state->count > 0)
	{
		for (int i = 0; i < state->count; ++i)
		{
			free(state->namelist[i]);
		}

		free(state->namelist);
		state->namelist = NULL;
		state->count = 0;
	}

	return 0;
}

int I_FindAttr(findstate_t* const fileinfo)
{
	dirent* const ent = fileinfo->namelist[fileinfo->current];
	bool isdir;

	if (DirEntryExists(ent->d_name, &isdir))
	{
		return isdir ? FA_DIREC : 0;
	}

	return 0;
}


void I_PutInClipboard(const char* const string)
{
	NSPasteboard* const pasteBoard = [NSPasteboard generalPasteboard];
	NSString* const stringType = NSStringPboardType;
	NSArray* const types = [NSArray arrayWithObjects:stringType, nil];
	NSString* const content = [NSString stringWithUTF8String:string];

	[pasteBoard declareTypes:types
					   owner:nil];
	[pasteBoard setString:content
				  forType:stringType];
}

FString I_GetFromClipboard(bool returnNothing)
{
	if (returnNothing)
	{
		return FString();
	}

	NSPasteboard* const pasteBoard = [NSPasteboard generalPasteboard];
	NSString* const value = [pasteBoard stringForType:NSStringPboardType];

	return FString([value UTF8String]);
}


unsigned int I_MakeRNGSeed()
{
	return static_cast<unsigned int>(arc4random());
}


TArray<FString> I_GetGogPaths()
{
	// GOG's Doom games are Windows only at the moment
	return TArray<FString>();
}

