/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * ELPDS - Enhanced Logical & Physical Drive Status
 * Copyright (C) 1993 Arno Schaefer (incorporated FIPS code).
 * Copyright (C) 1995, 2026 Daniel Doubrovkine.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or any later version.
 *
 * This program is distributed without any warranty, including the implied
 * warranties of merchantability or fitness for a particular purpose.
 * See ../LICENSE for the full license text.
 *
 * The 2026 adaptation replaces Borland APIs with Open Watcom APIs, corrects
 * decoding and error handling, and removes interactive display delays.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

static int quiet = 0;
static int no_detail = 0;
static byte boot_sector[512];

/* INT 25h leaves an extra flags word on the stack, unlike normal interrupts. */
static unsigned absolute_read(unsigned drive, void __far *packet);
#pragma aux absolute_read = \
    "push ds" \
    "push es" \
    "pop ds" \
    "mov cx,0ffffh" \
    "int 25h" \
    "pop dx" \
    "pop ds" \
    "jc failed" \
    "xor ax,ax" \
    "failed:" \
    parm [ax] [es bx] value [ax] modify [cx dx];

#pragma pack(push, 1)
struct ReadPacket {
    dword sector;
    word count;
    void __far *buffer;
};
#pragma pack(pop)

static word read_word(const byte *p)
{
    return (word)p[0] | ((word)p[1] << 8);
}

static dword read_dword(const byte *p)
{
    return (dword)p[0] | ((dword)p[1] << 8) |
           ((dword)p[2] << 16) | ((dword)p[3] << 24);
}

static void message(const char *fmt, ...)
{
    va_list args;
    if (quiet) return;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static int __far disk_error(unsigned, unsigned, unsigned __far *)
{
    return 3; /* Fail the DOS operation rather than displaying Abort/Retry. */
}

static int bios_read(unsigned drive, byte *buffer)
{
    union REGS in, out;
    struct SREGS seg;
    memset(&in, 0, sizeof(in));
    segread(&seg);
    in.h.ah = 2;
    in.h.al = 1;
    in.h.dl = (byte)drive;
    in.h.cl = 1;
    in.x.bx = FP_OFF(buffer);
    seg.es = FP_SEG(buffer);
    int86x(0x13, &in, &out, &seg);
    return out.x.cflag ? out.h.ah : 0;
}

static const char *partition_type(byte type)
{
    switch (type) {
    case 1: return "12-bit FAT";
    case 4: return "16-bit FAT (small)";
    case 6: return "16-bit FAT (BIGDOS)";
    case 5: case 15: return "extended partition";
    default: return "other partition";
    }
}

static int physical_status(void)
{
    union REGS in, out;
    byte mbr[512];
    unsigned count, drive, i;
    memset(&in, 0, sizeof(in));
    in.h.ah = 8;
    in.h.dl = 0x80;
    int86(0x13, &in, &out);
    if (out.x.cflag || out.h.ah) {
        message("ELPDS::no BIOS hard drives available.\n");
        return 0;
    }
    count = out.h.dl;
    message("ELPDS::total physical hard drives: %u\n", count);
    message("  ADDRESS HEADS SECTORS CYLINDERS SIZE\n");
    for (drive = 0x80; drive < 0x80 + count; ++drive) {
        memset(&in, 0, sizeof(in));
        in.h.ah = 8;
        in.h.dl = (byte)drive;
        int86(0x13, &in, &out);
        if (out.x.cflag || out.h.ah) {
            fprintf(stderr, "ELPDS::cannot read geometry for 0x%X.\n", drive);
            return 21;
        }
        dword heads = (dword)out.h.dh + 1;
        dword sectors = out.h.cl & 63;
        dword cylinders = ((word)out.h.ch | ((word)(out.h.cl & 0xc0) << 2)) + 1;
        message("  0x%X    %lu    %lu      %lu    %lu KiB\n",
                drive, heads, sectors, cylinders, heads * sectors * cylinders / 2);
        int error = bios_read(drive, mbr);
        if (error) {
            fprintf(stderr, "ELPDS::MBR read failed on 0x%X: BIOS error %d.\n", drive, error);
            return 21;
        }
        if (read_word(mbr + 510) != 0xaa55) {
            fprintf(stderr, "ELPDS::invalid MBR signature on 0x%X.\n", drive);
            return 21;
        }
        for (i = 0; i < 4; ++i) {
            const byte *p = mbr + 446 + i * 16;
            if (!p[4]) continue;
            message("    (%u) %lu KiB, %s, %s (0x%02X)\n",
                    i, read_dword(p + 12) / 2,
                    p[0] == 0x80 ? "bootable" : "non bootable",
                    partition_type(p[4]), p[4]);
        }
    }
    return 0;
}

static int logical_status(unsigned drive)
{
    struct ReadPacket packet;
    struct _diskfree_t space;
    packet.sector = 0;
    packet.count = 1;
    packet.buffer = boot_sector;
    unsigned error = absolute_read(drive, &packet);
    if (error) {
        fprintf(stderr, "ELPDS::boot-sector read failed on %c: (DOS error 0x%X).\n",
                'A' + drive, error);
        return 20;
    }
    word bytes = read_word(boot_sector + 11);
    word sectors_per_fat = read_word(boot_sector + 22);
    if (read_word(boot_sector + 510) != 0xaa55 || bytes != 512 ||
        !boot_sector[13] || !boot_sector[16] || !sectors_per_fat) {
        fprintf(stderr, "ELPDS::%c: has no supported FAT12/FAT16 boot sector.\n", 'A' + drive);
        return 20;
    }
    error = _dos_getdiskfree(drive + 1, &space);
    if (error) {
        fprintf(stderr, "ELPDS::free-space query failed on %c: (DOS error %u).\n",
                'A' + drive, error);
        return 20;
    }
    dword sectors = read_word(boot_sector + 19);
    if (!sectors) sectors = read_dword(boot_sector + 32);
    message("ELPDS::logical drive:         %c:\n", 'A' + drive);
    message("ELPDS::total partition size:  %lu KiB\n", sectors / 2);
    message("ELPDS::free bytes:            %lu bytes\n",
            (dword)space.avail_clusters * space.sectors_per_cluster * space.bytes_per_sector);
    if (no_detail) return 0;

    char oem[9], label[12], filesystem[9];
    memcpy(oem, boot_sector + 3, 8); oem[8] = 0;
    memcpy(label, boot_sector + 43, 11); label[11] = 0;
    memcpy(filesystem, boot_sector + 54, 8); filesystem[8] = 0;
    message("ELPDS::jump instruction:      %02X %02X %02X\n",
            boot_sector[0], boot_sector[1], boot_sector[2]);
    message("ELPDS::oem name:              %s\n", oem);
    message("ELPDS::bytes per sector:      %u\n", bytes);
    message("ELPDS::sectors per cluster:   %u\n", boot_sector[13]);
    message("ELPDS::reserved sectors:      %u\n", read_word(boot_sector + 14));
    message("ELPDS::number of fats:        %u\n", boot_sector[16]);
    message("ELPDS::root entries:          %u\n", read_word(boot_sector + 17));
    message("ELPDS::sectors:               %lu\n", sectors);
    message("ELPDS::media descriptor:      0x%02X\n", boot_sector[21]);
    message("ELPDS::sectors per fat:       %u\n", sectors_per_fat);
    message("ELPDS::sectors per track:     %u\n", read_word(boot_sector + 24));
    message("ELPDS::drive heads:           %u\n", read_word(boot_sector + 26));
    message("ELPDS::first sector:          %lu\n", read_dword(boot_sector + 28));
    message("ELPDS::physical drive number: 0x%02X\n", boot_sector[36]);
    message("ELPDS::signature:             0x%02X\n", boot_sector[38]);
    message("ELPDS::serial number:         %04X:%04X\n",
            read_word(boot_sector + 41), read_word(boot_sector + 39));
    message("ELPDS::volume label:          %s\n", label);
    message("ELPDS::file system id:        %s\n", filesystem);
    return 0;
}

static void help(void)
{
    puts("ELPDS 1.0 DOS port (original: Daniel Doubrovkine, 1995)");
    puts("Usage: ELPDS drive: [-d] [-m] [-?]");
    puts("  -d  Omit boot-sector details.");
    puts("  -m  Suppress normal output; errors still go to stderr.");
    puts("  -?  Show help. Slash-prefixed options also work.");
    puts("Read-only BIOS disk and FAT12/FAT16 logical-drive inspection.");
    puts("Return codes: 0 success, 20 invalid/missing logical drive, 21 BIOS/device error.");
    puts("The original credits substantial code from FIPS.");
    puts("Copyright (C) 1993 Arno Schaefer; 1995, 2026 Daniel Doubrovkine.");
    puts("GPL version 2 or later; see LICENSE. No warranty.");
}

int main(int argc, char **argv)
{
    int i, result;
    if (argc < 2) { help(); return 20; }
    if (!strcmp(argv[1], "-?") || !strcmp(argv[1], "/?")) { help(); return 0; }
    for (i = 2; i < argc; ++i) {
        if (!stricmp(argv[i], "-m") || !stricmp(argv[i], "/m")) quiet = 1;
        else if (!stricmp(argv[i], "-d") || !stricmp(argv[i], "/d")) no_detail = 1;
        else if (!strcmp(argv[i], "-?") || !strcmp(argv[i], "/?")) { help(); return 0; }
        else { fprintf(stderr, "ELPDS::invalid option: %s\n", argv[i]); return 20; }
    }
    unsigned drive = (unsigned)(toupper(argv[1][0]) - 'A');
    if (strlen(argv[1]) != 2 || argv[1][1] != ':' || drive < 2 || drive > 25) {
        fprintf(stderr, "ELPDS::invalid drive: %s (use C: through Z:).\n", argv[1]);
        return 20;
    }
    _harderr(disk_error);
    message("ELPDS 1.0 (Enhanced Logical & Physical Drive Status)\n");
    message("Daniel Doubrovkine, 1995 - Open Watcom DOS port\n\n");
    result = physical_status();
    if (!result) result = logical_status(drive);
    message("ELPDS::return code: %d\n", result);
    return result;
}
