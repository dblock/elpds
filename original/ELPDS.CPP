#include <stdio.h>
#include <conio.h>
#include <dos.h>
#include <process.h>
#include <string.h>
#include <bios.h>
#include <stdarg.h>

#define DISK_INT 0x13
#define RESET_DISK 0
#define READ_SECTOR 2
#define WRITE_SECTOR 3
#define VERIFY_SECTOR 4
#define GET_DRIVE_PARAMS 8
#define GET_DISK_TYPE 0x15

#define IGNORE  0
#define RETRY   1
#define ABORT   2

typedef unsigned char byte;
typedef unsigned int word;
typedef unsigned long dword;
typedef enum {false,true} boolean;
int myerror=0;
char logical_drive[2];
dword heads;
dword sectors;
dword cylinders;
dword log_sector;
dword mcb_cylinder;
dword mcb_head;
dword mcb_sector;
	byte jump_instruction[3];		// EB xx 90 or E9 xx xx
	char oem_name[9];
	word bytes_per_sector;          // usually 512
	byte sectors_per_cluster;       // may differ
	word reserved_sectors;          // usually 1 (bootsector)
	byte no_of_fats;                // usually 2
	word no_of_rootdir_entries;     // usually 512 for HDs (?), 224 for HD-Floppies, 112 for DD-Floppies
	word no_of_sectors;             // 0 on BIGDOS partitions
	byte media_descriptor;          // usually F8h
	word sectors_per_fat;           // depends on partition size
	word sectors_per_track;         // depends on drive
	word drive_heads;               // dto.
	dword hidden_sectors;           // first sector of partition or 0 for FDs
	dword no_of_sectors_long;       // number of sectors on BIGDOS partitions
	byte phys_drive_no;             // 80h or 81h
	byte signature;                 // usually 29h
	dword serial_number_right;            // random
	dword serial_number_left;
	char volume_label[12];
	char file_system_id[9];
	dword total_size;
static char *err_msg[] = {
     "write protect",
     "unknown unit",
     "drive not ready",
     "unknown command",
     "data error (CRC)",
     "bad request",
     "seek error",
     "unknown media type",
     "sector not found",
     "printer out of paper",
     "write fault",
     "read fault",
     "general failure",
     "reserved",
     "reserved",
     "invalid disk change",
     "no partition defined"
};

int buf[500];
int sector_read(int drive);
int physicalhd_sector_read(int drived);
int get_disk_type (int drive_number);
int get_no_of_drives (void);
int free_space(int drive);
void physical_get_geometry (int number);
error_win(char *msg);
int handler(int errval,int ax,int bp,int si);
void help_me(void);
void sector_show(void);
void suspend_it(int seconds);
void printf (char *fmt,...);

int option_messages=0;
int option_page=0;

int main(int argc, char *argv[])
{
    clrscr();


    int n;for(n=2;n<argc;n++)
	{
	strupr(argv[n]);

	if (strcmp(argv[n],"-M")==0) {option_messages=1;}
	else
	if (strcmp(argv[n],"/M")==0) {option_messages=1;}
	else
	if (strcmp(argv[n],"-D")==0) {option_page=1;}
	else
	if (strcmp(argv[n],"/d")==0) {option_page=1;}
	else
	if (strcmp(argv[n],"-?")==0) {help_me();}
	else
	if (strcmp(argv[n],"/?")==0) {help_me();}
	else
	{
	textcolor(LIGHTRED);
	cprintf("ELPDS::invaid option: %s",argv[n]);
	help_me();
	}
	}


    textcolor(WHITE);
    textbackground(BLACK);
    cprintf("\n\rELPDS 1.0 (Enhanced Logical & Physical Drive Status) Daniel Doubrovkine(c)(1995)");
    cprintf("           email welcome: dblock@.infomaniak.ch\n\r");
    strupr(argv[1]);
    if (argc<2) help_me();
    if ((argv[1][0]<'C')||(argv[1][0]>'Z')||(argv[1][1]!=':')||(strlen(argv[1])>2)||(strlen(argv[1])<=1))
	{
	 textcolor(LIGHTRED);
	 cprintf("ELPDS::init::drive:failed: invalid input %s (can be used for HDs only)\n\r",argv[1]);
	 help_me();
	 textcolor(WHITE);
	 }

    logical_drive[0]=argv[1][0];
    logical_drive[1]='\0';
    if (option_messages==0)printf("ELPDS::init:: drive: ");
    if (option_messages==0)cprintf("%s:\n\r",logical_drive);
    if (option_messages==0)printf("ELPDS::init:: installed C++ extended error handler\n");
    harderr(handler);

    if (option_messages==0)
    {
    printf("ELPDS::total physical hard drives: ");
    cprintf("%d\r\n", get_no_of_drives());
    printf("  ADDRESS HEADS SECTORS CYLINDERS     SIZE\n");
    }
    for (int i=0x80; i<0x80 + get_no_of_drives(); i++) if (get_disk_type(i) == 3)
	{
	if (option_messages==0)cprintf(" þ 0x%x ",i);
	physical_get_geometry (i);
	}
    sector_read(logical_drive[0]-'A');
    if (myerror==0) free_space(logical_drive[0]-'A');
    if (option_page==0)
    if (myerror==0)
	{
	if (option_messages==0)suspend_it(5);
	if (option_messages==0)sector_show();
	}



    textcolor(WHITE);
    cprintf("ELPDS::return code: ");
    cprintf("%d\r\n",myerror);
    textcolor(7);
    return (myerror);
}

void suspend_it(int seconds)
{
printf("\n");
for (int a=seconds; a>=0; a--)
{
   asm{
	 xor ax,ax
	 mov ah,0x0C
	 int 0x21
      }
   printf("...suspended for %d seconds (ESC to interrupt, SPACE to pause)...", a);
   gotoxy(1,wherey());
   sleep(1);
   asm{
      push ax
      xor ax,ax
      mov ah,1
      int 0x16
      jz key_in
      cmp al,0x1B
      jne key_in
      mov a,0
      }
key_in:
      asm{
	 cmp al,' '
	 jne key_pause
	 }
	 printf("...ELPDS has been suspended indefinetely, press any key to continue...");
	 asm{
	 xor ax,ax
	 mov ah,0x0C
	 int 0x21
	 xor ax,ax
	 int 0x16
	 }
	 a=0;
key_pause:
      asm{
	 xor ax,ax
	 mov ah,0x0C
	 int 0x21
      }
      asm pop ax
      if (a==0) break;
}
}

int sector_read(int drive)
{
  char bp[512];
  if (absread(drive, 1, 0, &bp) != 0)
  {
     return(0);
  }

printf("\n");

	memcpy (jump_instruction,bp,3);
	memcpy (oem_name,bp+3,8);
	oem_name[8]=0;
	bytes_per_sector = *(bp+0xb) | (*(bp+0xc) << 8);
	sectors_per_cluster = *(bp+0xd);
	reserved_sectors = *(bp+0xe) | (*(bp+0xf) << 8);
	no_of_fats = *(bp+0x10);
	no_of_rootdir_entries = *(bp+0x11) | (*(bp+0x12) << 8);
	no_of_sectors = *(bp+0x13) | (*(bp+0x14) << 8);
	media_descriptor = *(bp+0x15);
	sectors_per_fat = *(bp+0x16) | (*(bp+0x17) << 8);
	sectors_per_track = *(bp+0x18) | (*(bp+0x19) << 8);
	drive_heads = *(bp+0x1a) | (*(bp+0x1b) << 8);
	hidden_sectors = (dword) *(bp+0x1c) | ((dword) *(bp+0x1d) << 8) | ((dword) *(bp+0x1e) << 16) | ((dword) *(bp+0x1f) << 24);
	no_of_sectors_long = (dword) *(bp+0x20) | ((dword) *(bp+0x21) << 8) | ((dword) *(bp+0x22) << 16) | ((dword) *(bp+0x23) << 24);
	phys_drive_no = *(bp+0x24);
	signature = *(bp+0x26);
	serial_number_left = (dword) *(bp+0x29) | ((dword) *(bp+0x2a) << 8);
	serial_number_right = (dword) *(bp+0x27) | ((dword) *(bp+0x28) << 8);
	memcpy (volume_label,bp+0x2b,11);
	volume_label[11] = 0;
	memcpy (file_system_id,bp+0x36,8);
	file_system_id[8] = 0;
	if (no_of_sectors==0) total_size=no_of_sectors_long*bytes_per_sector;
	else total_size=no_of_sectors*bytes_per_sector;

return(0);
}

void sector_show(void)
{
textcolor(7);
clrscr();
free_space(logical_drive[0]-'A');
printf("ELPDS::jump instruction:      %X %X %X\n",jump_instruction[0],jump_instruction[1],jump_instruction[2]);
printf("ELPDS::oem name:              %s\n",oem_name);
printf("ELPDS::bytes per sector:      %u\n",bytes_per_sector);
printf("ELPDS::sectors per cluster:   %u\n",sectors_per_cluster);
printf("ELPDS::reserved sectors:      %u\n",reserved_sectors);
printf("ELPDS::number of fats:        %u\n",no_of_fats);
printf("ELPDS::root entries:          %u\n",no_of_rootdir_entries);
if(no_of_sectors!=0) printf("ELPDS::sectors:               %d\n",no_of_sectors);
if(no_of_sectors==0) printf("ELPDS::sectors (BIGDOS):      %ld\n",no_of_sectors_long);
printf("ELPDS::bytes per sector:      %u\n",bytes_per_sector);
printf("ELPDS::media descriptor:      0x%X\n",media_descriptor);
printf("ELPDS::sectors per fat:       %u\n",sectors_per_fat);
printf("ELPDS::sectors per track:     %u\n",sectors_per_track);
printf("ELPDS::drive heads:           %u\n",drive_heads);
printf("ELPDS::first sector:          %u\n",hidden_sectors);
printf("ELPDS::physical drive number: 0x%X\n",phys_drive_no);
printf("ELPDS::signature:             0x%X\n",signature);
printf("ELPDS::serial number:         %.4lX:%.4lX\n",serial_number_left,serial_number_right);
printf("ELPDS::volume label:          %s\n",volume_label);
printf("ELPDS::file system id:        %s\n",file_system_id);
}

int physicalhd_sector_read(int drived)
{

  struct diskinfo_t dinfo;
  int result;
  static char dbuf[512];

  dinfo.drive =  drived;    /* drive number for A: */
  dinfo.head  =  0;    /* disk head number */
  dinfo.track =  0;    /* track number */
  dinfo.sector  =  1;  /* sector number */
  dinfo.nsectors =  1; /* sector count */
  dinfo.buffer = dbuf; /* data buffer */

  _bios_disk(_DISK_READ, &dinfo);

	byte bootable;                  // 80h or 0
	char bootable_txt[4];
	byte start_head;                // location of first sector (bootsector)
	word start_cylinder;
	byte start_sector;
	byte system;			// 1 = 12-bit FAT
					// 4 = 16-bit FAT & 16-bit sector number
					// 6 = 16-bit FAT & 32-bit sector number (BIGDOS)
	byte end_head;                  // location of last sector
	word end_cylinder;
	byte end_sector;
	dword start_sector_abs;         // = start_cylinder * heads * sectors
					// + start_head * sectors + start_sector - 1
	dword no_of_sectors_abs;        // = end_cylinder * heads * sectors + end_head * sectors
					// + end_sector - start_sector_abs
  for (int i=0;i<4;i++)
	{
		textcolor(GREEN);
		textbackground(BLACK);

		int pi=(0x1be+16*i);
		bootable=dbuf[pi];
		system=dbuf[pi+4];
		 if (system!=0)
			{
			if(i!=0)printf("\n");

			if (option_messages==0) cprintf("   þ(%c) ",i+'0');


		   // = end_cylinder * heads * sectors + end_head * sectors
		   // + end_sector - start_sector_abs

			textcolor(CYAN);
			unsigned long no_of_sectors_abs=(dword) dbuf[pi+15] << 24 | (dword)dbuf[pi+14] << 16 | (dword)dbuf[pi+13] << 8 | (dword)dbuf[pi+12];
			if (option_messages==0)cprintf("%4.ld MB ",no_of_sectors_abs*512/1024000);
			textcolor(GREEN);
			}
		 if (option_messages==0)
			{
		 if (bootable==0x80) cprintf("bootable, ");
		 if ((system!=0)&&(bootable!=0x80)) cprintf("non bootable, ");
		 if (system==0x01) cprintf("12-bit FAT");
		 if (system==0x04) cprintf("12-bit FAT & 16-bit sector number");
		 if (system==0x06) cprintf("16-bit FAT & 32-bit sector number (BIGDOS)");
		 if ((system!=0)&&(system!=0x06)&&(system!=0x04)&&(system!=0x01)) cprintf("extended partition",system);

		textcolor(WHITE);
		textbackground(BLACK);
			}

	}
  printf("\n");
  return (0);
}

int get_disk_type (int drive_number)
{
	union REGS regs;
	regs.h.ah = GET_DISK_TYPE;
	regs.h.dl = drive_number;
	int86 (DISK_INT,&regs,&regs);
	if (regs.x.cflag) return 0;
	return (regs.h.ah);
}

int get_no_of_drives (void)
{
	union REGS regs;

	regs.h.ah = GET_DRIVE_PARAMS;
	regs.h.dl = 0x80;
	int86 (DISK_INT,&regs,&regs);
	if (regs.h.ah != 0) return (1);
	return (regs.h.dl);
}

int free_space(int drive)
{
	struct dfree free;
	long avail;
	getdfree(drive+1, &free);
	if (myerror!=0) return(-1);

	if (free.df_sclus == 0xFFFF)
	{
	   textcolor(LIGHTRED);
	   cprintf("ELPDS::inexistant partition: ");
	   textcolor(WHITE);
	   cprintf("%c:\n\r",drive+'A');
	   myerror=20;
	   return(-1);
	}

	avail =  (long) free.df_avail
		 * (long) free.df_bsec
		 * (long) free.df_sclus;

	printf("ELPDS::logical drive:         %c:\n",'A' + drive);
	printf("ELPDS::total partition size:  %ld bytes (%ld MB)\n",total_size,total_size/1024000);
	printf("ELPDS::free bytes:            %ld bytes (%ld MB)\n",avail,(avail/1024000));
	return(0);
}

void physical_get_geometry (int number)
{
	union REGS regs;
	regs.h.ah = GET_DRIVE_PARAMS;
	regs.h.dl = number;
	int86 (DISK_INT,&regs,&regs);
	if (regs.h.ah != 0) return;
	heads=(dword) regs.h.dh + 1;
	sectors=(dword) regs.h.cl & 0x3f;
	cylinders=((dword) regs.h.ch | (((dword) regs.h.cl << 2) & 0x300)) + 1;
	log_sector=0;
	mcb_cylinder=log_sector / (heads * sectors);
	mcb_head=(log_sector - (mcb_cylinder * heads * sectors)) / sectors;
	mcb_sector=log_sector - (mcb_cylinder * heads * sectors) - (mcb_head * sectors) + 1;
	if (option_messages==0)cprintf("    %.2ld     %.2ld     %.4ld     %.10ld bytes (%ld MB) \n\r",heads,sectors,cylinders,
		heads*sectors*cylinders*512,heads*sectors*cylinders*512/1024000);
	physicalhd_sector_read(number);
}

error_win(char *msg)
{
    int retval;
    cputs(msg);
    retval=ABORT;
    return(retval);
}

#pragma warn -par

int handler(int errval,int ax,int bp,int si)
{
    static char msg[80];
    unsigned di;
    int drive;
    int errorno;

    di= _DI;
    if (ax < 0)
    {
       error_win("ELPDS::unexpected device error...aborting.\n");
       myerror=21;
       hardretn(ABORT);
    }
    drive = ax & 0x00FF;
    errorno = di & 0x00FF;
    sprintf(msg, "ELPDS::error: %s on drive %c\r\n",
	    err_msg[errorno], 'A' + drive);
	    myerror=errorno;
    hardretn(error_win(msg));
//    hardresume(error_win(msg)); //this quits to DOS
    return ABORT;
 }
#pragma warn +par

void help_me(void)
{
    option_messages=0;
    printf("\nDisclaimer: The author claims no responsibility for damages caused by any");
    printf("\nuse of this software, as well as third party modifications of the");
    printf("\noriginal code of ELPDS destributed freely to encourage and help developpers.");
    printf("\n\nELPDS is totally  free, but it does entirely  remain  the property of it's");
    printf("\nauthor, and thus cannot be sold! Any illegal sale of this software will be ");
    printf("\nprosecuted at maximum extent possible under the federal criminal law of the");
    printf("\nHelvetic Confederation.");
    printf("\nsyntax: ELPDS drive: <options>");
    printf("\n <options>    -d = no partition detail (thus no delay)");
    printf("\n              -m = (almost) no messages");
    printf("\n              -? = this help");
    printf("\n        ex: ELPDS C:");
    printf("\nELPDS uses a huge part of FIPS code (Mr. Schaeffer), All Rights Reserved, etc...");
    printf("ELPDS does not attempt to write anything to the drive, it is actually made");
    printf("\nfor checking if a partition exists, and if it is formatted or not.");
    printf("\n\npress any key for return codes...");
    getch();
    clrscr();
    printf("\nreturn codes:");
    printf("\n\t00: eVeRyTHiNG WeNT oK!");
    printf("\n\t01: disk is write protected (not implemented yet)");
    printf("\n\t02: drive not ready         (not implemented yet)");
    printf("\n\t03: unknown command	    (not implemented yet)");
    printf("\n\t04: data error (CRC)			       ");
    printf("\n\t05: bad request		    (not implemented yet)");
    printf("\n\t06: seek error 				       ");
    printf("\n\t07: unknown media type	  		       ");
    printf("\n\t08: sector not found			       ");
    printf("\n\t09: printer out of paper    (WHaT'S THiS DoiN' HeRe?");
    printf("\n\t10: write fault		    (not implemented yet)");
    printf("\n\t11: read fault (an empty CD ROM drive shows this too)");
    printf("\n\t12: general failure 			       ");
    printf("\n\t13: reserved				       ");
    printf("\n\t14: reserved				       ");
    printf("\n\t15: invalid disk change			       ");
    printf("\n\t16: no partition defined    (error code 20 replaces this)");
    printf("\n\t20: partition does not exist		       ");
    printf("\n\t21: unexpected device error (other than a disk error)");
    exit(0);
}

void printf (char *fmt,...)
{
	if(option_messages==1) return;
	va_list ap;
	va_start (ap,fmt);
	vprintf (fmt,ap);
	va_end (ap);
}


