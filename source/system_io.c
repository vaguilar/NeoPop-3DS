/* $NiH: system_io.c,v 1.15 2004/07/25 10:40:51 wiz Exp $ */
/*
  system_io.c -- read/write flash files 
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

#include "NeoPop-SDL.h"

char *state_dir;
char *flash_dir;
int use_rom_name;
int state_slot;

Handle dir_handle;

_u32
strlen2 (u16 *str) {
	_u32 length = 0;
	while (*str) {
		length++;
		str++;
	}
	return length;
}

static BOOL
read_file_to_buffer(char *filename, _u8 *buffer, _u32 len)
{
    _u64 size;
    _u32 bytesRead;
    Handle fileHandle;

    //setup SDMC archive
    FS_Archive sdmcArchive=(FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (_u8*)""}};

    //create file path struct
    FS_Path filePath=fsMakePath(PATH_ASCII, filename);

    //open file
    Result ret=FSUSER_OpenFileDirectly(&fileHandle, sdmcArchive, filePath, FS_OPEN_READ, FS_ATTRIBUTE_ARCHIVE);
    //check for errors : exit if there is one
    if(ret)goto exit;

    //get file size
    ret=FSFILE_GetSize(fileHandle, &size);
    if(ret)goto exit;

    //read contents
    ret=FSFILE_Read(fileHandle, &bytesRead, 0x0, buffer, size);
    if(ret || size!=bytesRead)goto exit;

    //close the file because we like being nice and tidy
    ret=FSFILE_Close(fileHandle);
    if(ret)goto exit;

	return TRUE;

	exit:
		system_debug_printf("\nERROR: %#010x\n\n", ret);
		return FALSE;
}

static BOOL
write_file_from_buffer(char *filename, _u8 *buffer, _u32 len)
{
	Handle fileHandle;
	_u32 byteswritten = 0;

	//setup SDMC archive
	FS_Archive sdmcArchive=(FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (_u8*)""}};

	FS_Path filePath = fsMakePath(PATH_ASCII, filename);

	Result ret = FSUSER_OpenFileDirectly(&fileHandle, sdmcArchive, filePath, FS_OPEN_CREATE|FS_OPEN_WRITE, FS_ATTRIBUTE_ARCHIVE);
	if (ret) goto exit;

	ret = FSFILE_SetSize(fileHandle, (_u64)len);
	if (ret) goto exit;

	ret = FSFILE_Write(fileHandle, &byteswritten, 0, (_u32*)buffer, len, FS_WRITE_FLUSH);
	if (ret) goto exit;

	ret = FSFILE_Close(fileHandle);
	if(ret) goto exit;

	return TRUE;

	exit:
		system_debug_printf("\nERROR: %#010x\n\n", ret);
		return FALSE;
}

BOOL
read_dir_open(char *dir_name)
{
	FS_Archive sdmcArchive = (FS_Archive) {ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (_u8*)""}};
	FSUSER_OpenArchive(&sdmcArchive);

	FS_Path dir_path = (FS_Path){PATH_ASCII, strlen(dir_name) + 1, dir_name};

	if (FSUSER_OpenDirectory(&dir_handle, sdmcArchive, dir_path))
		return FALSE;

	return TRUE;
}

BOOL
read_dir_next(FS_DirectoryEntry *dir_entry)
{
	if (!dir_handle)
		return FALSE;

	_u32 nread = 0;
	_u32 hidden_file = 0;

	do {
		FSDIR_Read(dir_handle, &nread, 1, dir_entry);
		hidden_file = 	nread &&
						strlen2(dir_entry->name) > 3 &&
						dir_entry->name[0] == '.' &&
						dir_entry->name[1] == '_';
	} while (hidden_file);

	if (!nread)
		return FALSE;

	return TRUE;
}

BOOL
read_dir_close()
{
	if (dir_handle) {
		FSDIR_Close(dir_handle);
		return TRUE;
	}

	return FALSE;
}

_s64
get_file_size(char *filename) {

    _s64 size;
    Handle fileHandle;

    //setup SDMC archive
    FS_Archive sdmcArchive=(FS_Archive){ARCHIVE_SDMC, (FS_Path){PATH_EMPTY, 1, (_u8*)""}};
    //create file path struct
    FS_Path filePath=fsMakePath(PATH_ASCII, filename);

    //open file
    Result ret=FSUSER_OpenFileDirectly(&fileHandle, sdmcArchive, filePath, FS_OPEN_READ, FS_ATTRIBUTE_ARCHIVE);
    //check for errors : exit if there is one
    if(ret)goto exit;

    //get file size
    ret=FSFILE_GetSize(fileHandle, &size);
    if(ret)goto exit;

    //close the file because we like being nice and tidy
    ret=FSFILE_Close(fileHandle);
    if(ret)goto exit;

	return size;

	exit:
		return -1;
}

static BOOL
validate_dir(const char *path)
{
    char *s, *p, *q;
    struct stat sb;

    if (stat(path, &sb) == 0) {
	if ((sb.st_mode & S_IFDIR) == S_IFDIR)
	    return TRUE;
	else {
	    fprintf(stderr, "Not a directory: `%s'", path);
	    return FALSE;
	}
    }

    s = strdup(path);

    q = s+strlen(s);
    while (q[-1] == '/')
	--q;
    *q = 0;

    p = s+1;
    for (;;) {
	if ((q=strchr(p, '/')) == NULL)
	    q = p+strlen(p);
	*q = '\0';

	if (stat(s, &sb) == -1) {
	    if (errno == ENOENT) {
		printf("mkdir(%s)\n", s);
		if (mkdir(s, 0777) == -1) {
		    fprintf(stderr, "Directory `%s' does not exist and "
			    "creation failed: %s\n", path, strerror(errno));
		    return FALSE;
		}
	    }
	    else {
		fprintf(stderr, "Directory `%s' does not exist and "
			"cannot stat ancestor: %s\n", path, strerror(errno));
		return FALSE;
	    }
	}
	if ((sb.st_mode & S_IFDIR) != S_IFDIR) {
	    fprintf(stderr, "Ancestor not a directory: `%s'", path);
	    return FALSE;
	}

	if (q-s == strlen(path))
	    break;
	*q = '/';
	p = q+1;
    }

    return TRUE;
}

void
system_state_load(void)
{
    char *fn, ext[5];

    sprintf(ext, ".ng%c", (state_slot ? state_slot+'0' : 's'));
    if ((fn=system_make_file_name(state_dir, ext, FALSE)) == NULL)
	return;
    state_restore(fn);
    free(fn);

    return;
}

void
system_state_save(void)
{
    char *fn, ext[5];

    sprintf(ext, ".ng%c", (state_slot ? state_slot+'0' : 's'));
    if ((fn=system_make_file_name(state_dir, ext, TRUE)) == NULL)
	return;
    state_store(fn);
    free(fn);

    return;
}

BOOL
system_io_rom_read(char *filename, _u8 *buffer, _u32 len)
{
    return read_file_to_buffer(filename, buffer, len);
}

BOOL
system_io_flash_read(_u8* buffer, _u32 len)
{
    char *fn;
    int ret;

    if ((fn=system_make_file_name(flash_dir, ".ngf", FALSE)) == NULL)
	return FALSE;

	printf("system_make_file_name success\n");

    ret = read_file_to_buffer(fn, buffer, len);
    free(fn);

    return ret;
}

BOOL
system_io_flash_write(_u8* buffer, _u32 len)
{
    char *fn;
    int ret;

    if ((fn=system_make_file_name(flash_dir, ".ngf", TRUE)) == NULL)
	return FALSE;

	printf("system_make_file_name success\n");

    ret = write_file_from_buffer(fn, buffer, len);
    free(fn);
    return ret;
}

BOOL
system_io_state_read(char *filename, _u8 *buffer, _u32 len)
{
    return read_file_to_buffer(filename, buffer, len);
}

BOOL
system_io_state_write(char *filename, _u8 *buffer, _u32 len)
{
    return write_file_from_buffer(filename, buffer, len);
}

char *
system_make_file_name(const char *dir, const char *ext, int writing)
{
    char *fname, *name, *home, *p;
    int len;

    if (use_rom_name)
	name = rom.name;
    else
	name = rom.filename;
    len = strlen(dir)+strlen(name)+strlen(ext)+2;

    home = NULL;
    if (strncmp(dir, "~/", 2) == 0) {
	home = getenv("HOME");
	if (home == NULL)
	    return NULL;
	len += strlen(home)-1;
    }

    if ((fname=malloc(len)) == NULL)
	return NULL;

    if (strncmp(dir, "~/", 2) == 0)
	sprintf(fname, "%s%s", home, dir+1);
    else
	strcpy(fname, dir);

    if (writing && !validate_dir(fname))
	return NULL;

    /* XXX: maybe replace all but [-_A-Za-z0-9] */
    p = fname+strlen(fname);
    sprintf(p, "/%s%s", name, ext);
    while (*(++p))
	if (*p == '/')
	    *p = '_';

    return fname;
}
