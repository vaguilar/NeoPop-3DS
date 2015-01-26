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

static BOOL
read_file_to_buffer(char *filename, _u8 *buffer, _u32 len)
{

#ifndef _3DS

    FILE *fp;
    _u32 got;

    if ((fp=fopen(filename, "rb")) == NULL)
	return FALSE;

    while ((got=fread(buffer, 1, len, fp)) < len) {
	if (feof(fp)) {
	    fclose(fp);
	    return FALSE;
	}
	if (ferror(fp) && errno != EINTR) {
	    fclose(fp);
	    return FALSE;
	}

	len -= got;
	buffer += got;
    }
	    
    if (fclose(fp) != 0)
	return FALSE;

    return TRUE;

#else

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

    //read contents
    ret=FSFILE_Read(fileHandle, &bytesRead, 0x0, buffer, size);
    if(ret || size!=bytesRead)goto exit;

    //close the file because we like being nice and tidy
    ret=FSFILE_Close(fileHandle);
    if(ret)goto exit;

	return TRUE;

	exit:
		return FALSE;

#endif
}

static BOOL
write_file_from_buffer(char *filename, _u8 *buffer, _u32 len)
{
    FILE *fp;
    _u32 written;

	printf("WRITING TO FILE: %s\n", filename);

    if ((fp=fopen(filename, "wb")) == NULL)
	return FALSE;

    while ((written=fwrite(buffer, 1, len, fp)) < len) {
	if (feof(fp)) {
	    fclose(fp);
	    return FALSE;
	}
	if (ferror(fp) && errno != EINTR) {
	    fclose(fp);
	    return FALSE;
	}

	len -= written;
	buffer += written;
    }
	    
    if (fclose(fp) != 0)
	return FALSE;

    return TRUE;
}

BOOL
read_dir_open(char *dir_name)
{
	FS_archive sdmcArchive = (FS_archive) {ARCH_SDMC, (FS_path){PATH_EMPTY, 1, (_u8*)""}};
	FSUSER_OpenArchive(NULL, &sdmcArchive);

	FS_path dir_path = (FS_path){PATH_CHAR, strlen(dir_name) + 1, dir_name};

	if (FSUSER_OpenDirectory(NULL, &dir_handle, sdmcArchive, dir_path))
		return FALSE;

	return TRUE;
}

BOOL
read_dir_next(FS_dirent *dir_entry)
{
	if (!dir_handle)
		return FALSE;

	_u32 nread = 0;
	FSDIR_Read(dir_handle, &nread, 1, dir_entry);

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

	printf("READING %d bytes to flash %s\n", len, flash_dir);

    if ((fn=system_make_file_name(flash_dir, ".ngf", FALSE)) == NULL)
	return FALSE;

	printf("system_make_file_name success\n");

    ret = read_file_to_buffer(fn, buffer, len);
    free(fn);

	printf("READ: 0x%.*x\n", len, buffer);

    return ret;
}

BOOL
system_io_flash_write(_u8* buffer, _u32 len)
{
    char *fn;
    int ret;

	printf("WRITING %d bytes to flash %s\n", len, flash_dir);

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
