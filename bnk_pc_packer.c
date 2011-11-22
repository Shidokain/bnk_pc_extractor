/**
 * bnk_pc_packer.c - a simple packer of .bnk_pc files found in Saints
 * Row: The Third. Depends on extracting metadata from archives first.
 *
 * Copyright (c) 2011 Michael Lelli
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <direct.h>

typedef unsigned __int8 u8;
typedef unsigned __int16 u16;
typedef unsigned __int32 u32;
typedef unsigned __int64 u64;

#pragma pack(push,1)

// all values are little-endian

// k#: Value is predictable or can be computed, but use may not be known
// u#: Value use is unknown and cannot be predicted or computed

struct _header
{
	u64 magic; // "VWSBPC  "
	u32 k1; // usually 0x00000000
	u32 k2; // usually 0x00010002
	u32 u1; // unknown; maybe an ID?
	u32 k3; // pointer to the (count + 2)th entry?
		// essentially sizeof(header) + ((count + 1) * 16)
	u32 count; // number of entry items
};

struct _entry
{
	u32 u1; // unknown; seems to increase with each entry, maybe an ID?
	u64 offset; // offset of data in the file, including 28-byte header.
		    // each file is padded with null bytes to the next offset
		    // that's a multiple of 0x800, including padding after the
		    // last file
	u32 length; // file length
};

#pragma pack(pop)

typedef struct _header header;
typedef struct _entry entry;

#define PADDING_LENGTH (0x800)

const static u8 padding[PADDING_LENGTH] = { 0 };

void padFile(FILE *f)
{
	fwrite(padding, PADDING_LENGTH - (ftell(f) % PADDING_LENGTH), 1, f);
}

int main(int argc, char *argv[])
{
	FILE *f = NULL;
	FILE *out = NULL;
	FILE *sb = NULL;
	u32 i;
	header head;
	entry *entries = NULL;
	int r = 0;
	char *soundboot = NULL;
	char *soundboot2 = NULL;

	if (argc > 4)
	{
		f = fopen(argv[1], "r");

		if (f == NULL)
		{
			printf("log file can't be opened\n");
			goto error;
		}
	}
	else
	{
		printf("usage: %s log.txt file1.wav file2.wav ... output.bnk_pc\nlog.txt is generated by bnk_pc_extractor\n", argv[0]);
		goto error;
	}

	head.magic = 0x2020435042535756ULL;

	fscanf(f, "HEADER\n");
	fscanf(f, "magic:  \"VWSBPC  \"\n");
	fscanf(f, "k1:     0x%08X\n", &(head.k1));
	fscanf(f, "k2:     0x%08X\n", &(head.k2));
	fscanf(f, "u1:     0x%08X\n", &(head.u1));
	fscanf(f, "k3:     0x%08X\n", &(head.k3));
	fscanf(f, "count:  0x%08X\n", &(head.count));

	if (head.count != argc - 3)
	{
		printf("expected %d files, got %d\n", head.count, argc - 3);
		goto error;
	}

	out = fopen(argv[argc - 1], "wb");
	fwrite(&head, sizeof(header), 1, out);
	entries = malloc(sizeof(entry) * head.count);

	// loop 1: go through the log file and get the unknown metadata
	for (i = 0; i < head.count; i++)
	{
		unsigned int temp;

		fscanf(f, "\n%05u:\n", &temp);

		if (i != temp)
		{
			printf("entry count miss-match: expected %u, got %u\n", i, temp);
			goto error;
		}

		fscanf(f, "u1:     0x%08X\n", &(entries[i].u1));
		fscanf(f, "offset: 0x%016llX\n", &(entries[i].offset));
		fscanf(f, "length: 0x%08X\n", &(entries[i].length));

		// write 0's for now, will come back and write actuall data later
		fwrite(padding, sizeof(entry), 1, out);
	}

	padFile(out);

	// loop 2: insert data
	for (i = 0; i < head.count; i++)
	{
		FILE *in;
		entries[i].offset = ftell(out);
		in = fopen(argv[i + 2], "rb");

		if (in == NULL)
		{
			printf("could not open %s for reading\n", argv[i + 2]);
			goto error;
		}

		fseek(in, 0, SEEK_END);
		entries[i].length = (u32) ftell(in);
		fseek(in, 0, SEEK_SET);
		u8 *buffer = malloc(entries[i].length);
		fread(buffer, entries[i].length, 1, in);
		fwrite(buffer, entries[i].length, 1, out);
		fclose(in);
		free(buffer);

		padFile(out);
	}

	// loop 3: go back and write updated entries

	fseek(out, sizeof(header), SEEK_SET);
	fwrite(entries, sizeof(entry), head.count, out);

	soundboot = malloc(strlen(argv[argc - 1]) + 1);
	soundboot2 = malloc(strlen(argv[argc - 1]) + 10);
	memcpy(soundboot, argv[argc - 1], strlen(argv[argc - 1]) + 1);
	soundboot[strrchr(soundboot, '.') - soundboot] = 0;
	sprintf(soundboot2, "%s.mbnk_pc", soundboot);
	sb = fopen(soundboot2, "wb");
	fwrite(&head, sizeof(header), 1, sb);
	fwrite(entries, sizeof(entry), head.count, sb);

end:
	free(entries);
	free(soundboot);
	free(soundboot2);

	if (f != NULL)
	{
		fclose(f);
	}

	if (out != NULL)
	{
		fclose(out);
	}

	if (sb != NULL)
	{
		fclose(sb);
	}

	return r;

error:
	r = 1;
	goto end;
}
