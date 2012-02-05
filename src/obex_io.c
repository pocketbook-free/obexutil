/**
	\file apps/obex_io.c
	Some useful disk-IO functions.
	OpenOBEX test applications and sample code.

	Copyright (c) 1999 Dag Brattli, All Rights Reserved.
	Copyright (c) 2000 Pontus Fuchs, All Rights Reserved.

	OpenOBEX is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with OpenOBEX. If not, see <http://www.gnu.org/>.
 */

#include <openobex/obex.h>

#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "obexutil.h"
#include "obex_io.h"

extern obex_t *handle;
int obex_protocol_type = OBEX_PROTOCOL_GENERIC;

//
// Get the filesize in a "portable" way
//
int get_filesize(const char *filename)
{
	struct stat stats;
	/*  Need to know the file length */
	stat(filename, &stats);
	return (int) stats.st_size;
}


//
// Read a file and alloc a buffer for it
//
uint8_t* easy_readfile(const char *filename, int *file_size)
{
	int actual;
	int fd;
	uint8_t *buf;

	*file_size = get_filesize(filename);
	printf("name=%s, size=%d\n", filename, *file_size);

	fd = open(filename, O_RDONLY, 0);

	if (fd == -1) {
		return NULL;
	}
	
	if(! (buf = malloc(*file_size)) )	{
		return NULL;
	}

	actual = read(fd, buf, *file_size);
	close(fd); 

	*file_size = actual;
	return buf;
}


//
//
//
obex_object_t *build_object_from_file(obex_t *handle, const char *filename, uint32_t creator_id)
{
	obex_headerdata_t hdd;
	uint8_t unicode_buf[200];
	int namebuf_len;
 	obex_object_t *object;
	//uint32_t creator_id;
	int file_size;
	char *name = NULL;
	uint8_t *buf;


	buf = easy_readfile(filename, &file_size);
	if(buf == NULL)
		return NULL;

	/* Set Memopad as the default creator ID */
	if(creator_id == 0)
		creator_id = MEMO_PAD;

	/* Find the . in the filename */
	name = strchr(filename, '.');
	if (name) {
		name++;
		if (strcmp(name, "vcf") == 0) {
			printf( "This is a Address Book file\n");
			creator_id = ADDRESS_BOOK;
		} else if (strcmp(name, "vcs") == 0) {
			printf( "This is a Date Book file\n");
			creator_id = DATE_BOOK;
		} else if (strcmp(name, "txt") == 0) {
			printf("This is a Memo pad file\n");
			creator_id = MEMO_PAD;
		} else if (strcmp(name, "prc") == 0) {
			printf("This is a Pilot resource file\n");
			creator_id = PILOT_RESOURCE;
		}
	}
	/* Build object */
	object = OBEX_ObjectNew(handle, OBEX_CMD_PUT);

	namebuf_len = OBEX_CharToUnicode(unicode_buf, (uint8_t *) filename, sizeof(unicode_buf));

	hdd.bs = unicode_buf;
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_NAME,
				hdd, namebuf_len, 0);

	hdd.bq4 = file_size;
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_LENGTH,
				hdd, sizeof(uint32_t), 0);

#if 0
	/* Optional header for win95 irxfer, allows date to be set on file */
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_TIME2,
				(obex_headerdata_t) (uint32_t) stats.st_mtime,
				sizeof(uint32_t), 0);
#endif
	if (obex_protocol_type != 1) {
		/* Optional header for Palm Pilot */
		/* win95 irxfer does not seem to like this one */
		hdd.bq4 = creator_id;
		OBEX_ObjectAddHeader(handle, object, HEADER_CREATOR_ID,
					hdd, sizeof(uint32_t), 0);
	}

	hdd.bs = buf;
	OBEX_ObjectAddHeader(handle, object, OBEX_HDR_BODY,
				hdd, file_size, 0);

	free(buf);
	return object;
}


/*
 * Function safe_save_file ()
 *
 *    First remove path and add "/tmp/". Then save.
 *
 */
#ifndef DEFFILEMODE
#define DEFFILEMODE 0
#endif
int safe_save_file(char *name, const uint8_t *buf, int len)
{
	int fd;
	int actual;

	fd = open(name, O_RDWR | O_CREAT, DEFFILEMODE);

	if ( fd < 0) {
		perror(name);
		return -1;
	}
	
	actual = write(fd, buf, len);
	close(fd);

	return actual;
}

int unicode_to_utf8(const uint8_t *src, int srclen, char *dest, int maxsize) {

	char *dp = dest;
	int c;

	while (srclen > 0 && (dp-dest) < maxsize-4) {
		c = ((*src & 0xff) << 8) | (*(src+1) & 0xff);
		if (c == 0) break;
		if (c <= 0x7f) {
			*(dp++) = c;
		} else if (c <= 0x7ff) {
			*(dp++) = 0xc0 | ((c >> 6) & 0x3f);
			*(dp++) = 0x80 | (c & 0x3f);
		} else {
			*(dp++) = 0xe0 | ((c >> 12) & 0x0f);
			*(dp++) = 0x80 | ((c >> 6) & 0x3f);
			*(dp++) = 0x80 | (c & 0x3f);
		}
		src+=2;
		srclen-=2;
	}
	*dp = 0;
	return (dp-dest);

}

int utf8_to_unicode(const char *src, int srclen, uint8_t *dest, int maxsize) {

	const char *sp = src;
	uint8_t *dp = dest;
	int u, c;

	while (sp-src < srclen && (dp-dest) < maxsize-2) {
		c = *sp & 0xff;
		sp++;
		if (c <= 0x7f) {
			u = c;
		} else if ((c & 0xe0) == 0xc0) {
			if ((*sp & 0xc0) != 0x80) continue;
			u = ((c & 0x1f) << 6) | (*sp & 0x3f);
			sp++;
		} else if ((c & 0xf0) == 0xe0) {
			if ((*sp & 0xc0) != 0x80) continue;
			if ((*(sp+1) & 0xc0) != 0x80) continue;
			u = ((c & 0x0f) << 12) | ((*sp & 0x3f) << 6) | (*(sp+1) & 0x3f);
			sp += 2;
		} else {
			continue;
		}
		*dp = (u >> 8) & 0xff;
		*(dp+1) = u & 0xff;
		dp += 2;
	}
	*dp = *(dp+1) = 0;
	dp += 2;
	return (dp-dest);

}

char *destination_path(char *name) {

	char buf[1024], *tname, *ext, *sp;
	struct stat st;
	int i;

	sp = strrchr(name, '/');
	if (sp == NULL) {
		sp = name;
	} else {
		sp++;
	}

	snprintf(buf, sizeof(buf), "%s/%s", STORAGEPATH, sp);
	if (stat(buf, &st) == -1) return strdup(buf);

	tname = strdup(sp);
	ext = strrchr(tname, '.');
	if (ext == NULL || strchr(ext, '/') != NULL) {
		ext = "";
	} else {
		*(ext++) = 0;
	}
	
	for (i=2; i<=99; i++) {
		snprintf(buf, sizeof(buf), "%s/%s(%i).%s", STORAGEPATH, tname, i, ext);
		if (stat(buf, &st) == -1) break;
	}

	free(tname);
	return strdup(buf);

}
