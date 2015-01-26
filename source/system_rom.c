/* $NiH: system_rom.c,v 1.15 2004/07/12 00:28:15 dillo Exp $ */
/*
  system_rom.c -- ROM loading support
  Copyright (C) 2002-2004 Thomas Klausner and Dieter Baron

  This file is part of NeoPop-SDL, a NeoGeo Pocket emulator
  The author can be contacted at <wiz@danbala.tuwien.ac.at>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include "config.h"

#include "NeoPop-SDL.h"
#ifdef HAVE_LIBZ
#include "unzip.h"
#endif

static BOOL rom_load(char *);

static BOOL
rom_load(char *filename)
{
    struct stat st;
#ifdef HAVE_LIBZ
    char name[1024];
    unzFile z;
    unz_file_info zfi;
    int l, err;
#endif

#ifdef _3DS

    _u64 size;
    _u32 bytesRead;
    Handle fileHandle;

    //setup SDMC archive
    FS_archive sdmcArchive=(FS_archive){ARCH_SDMC, (FS_path){PATH_EMPTY, 1, (_u8*)""}};
    //create file path struct (note : FS_makePath actually only supports PATH_CHAR, it will change in the future)
    FS_path filePath=FS_makePath(PATH_CHAR, filename);

    //open file
    Result ret=FSUSER_OpenFileDirectly(NULL, &fileHandle, sdmcArchive, filePath, FS_OPEN_READ, FS_ATTRIBUTE_NONE);
    //check for errors : exit if there is one
    if(ret)goto exit;

    //get file size
    ret=FSFILE_GetSize(fileHandle, &size);
    if(ret)goto exit;

	//set file size in struct
	st.st_size = size;

    //close the file because we like being nice and tidy
    ret=FSFILE_Close(fileHandle);
    if(ret)goto exit;

#else

    if (stat(filename, &st) == -1) {
	system_message("%s `%s': %s", system_get_string(IDS_EROMFIND),
		       filename, strerror(errno));
	return FALSE;
    }

#endif

#ifdef HAVE_LIBZ
    if ((z=unzOpen(filename)) != NULL) {
	for (err=unzGoToFirstFile(z); err==0; err=unzGoToNextFile(z)) {
	    if (unzGetCurrentFileInfo(z, &zfi, name, sizeof(name),
				      NULL, 0, NULL, 0) != UNZ_OK)
		continue;
	    if (zfi.size_filename > sizeof(name))
		continue;
	    l = strlen(name);
	    if (l < 4)
		continue;
	    if (strcasecmp(name+l-4, ".ngp") == 0
		|| strcasecmp(name+l-4, ".ngc") == 0
		|| strcasecmp(name+l-4, ".npc") == 0) {
		rom.length = zfi.uncompressed_size;
		rom.data = (char *)calloc(rom.length, 1);
		
		if ((unzOpenCurrentFile(z) != UNZ_OK)
		    || (unzReadCurrentFile(z, rom.data, rom.length)
			!= rom.length)) {
		    free(rom.data);
		    rom.data = NULL;
		    system_message("%s `%s'", system_get_string(IDS_EROMOPEN),
				   filename);
		    unzCloseCurrentFile(z);
		    unzClose(z);
		    return FALSE;
		}
		if (unzCloseCurrentFile(z) != UNZ_OK) {
		    free(rom.data);
		    rom.data = NULL;
		    system_message("%s `%s'", system_get_string(IDS_EROMOPEN),
				   filename);
		    unzClose(z);
		    return FALSE;
		}
		unzClose(z);
		
		return TRUE;
	    }
	}
	unzClose(z);
	system_message("%s `%s': no rom found",
		       system_get_string(IDS_EROMOPEN), filename);
	return FALSE;
    }
#endif

    rom.length = st.st_size;
    rom.data = (char *)calloc(rom.length, 1);

    if (system_io_rom_read(filename, rom.data, rom.length))
	return TRUE;

	exit:
    system_message("%s `%s': %s", system_get_string(IDS_EROMOPEN),
		   filename, strerror(errno));
    free(rom.data);
    rom.data = NULL;
    return FALSE;
}

void
system_rom_changed(void)
{
    char title[128];

    (void)snprintf(title, sizeof(title), PROGRAM_NAME " - %s", rom.name);

    /* set window caption */
    //SDL_WM_SetCaption(title, NULL);
}
    
BOOL
system_rom_load(char *filename)
{
    char *fn;
    BOOL ret;

    /* Remove old ROM from memory */
    system_rom_unload();

    ret = rom_load(filename);

    if (ret == FALSE)
	return FALSE;

    memset(rom.filename, 0, sizeof(rom.filename));
    if ((fn=strrchr(filename, '/')) == NULL)
	fn = filename;
    else
	*fn++ = '\0';

    /* don't copy extension */
    strncpy(rom.filename, fn, min(sizeof(rom.filename), strlen(fn)-4));

    rom_loaded();
    system_rom_changed();

    return TRUE;
}

void
system_rom_unload(void)
{
    rom_unload();

    /* reset window caption */
    //SDL_WM_SetCaption(PROGRAM_NAME, NULL);
}
