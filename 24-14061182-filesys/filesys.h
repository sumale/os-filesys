#ifndef FILESYS_H
#define FILESYS_H

#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <string.h>
#include<ctype.h>
#include<stdlib.h>
#include <time.h>

#ifndef DEBUG
#define DEBUG
#endif

#undef DEBUG

#define DEVNAME "/home/chairui/os/data"
#define DIR_ENTRY_SIZE 32 //每个目录项的大小
#define SECTOR_SIZE 512
#define CLUSTER_SIZE 512*32


#define FAT_ONE_OFFSET 512+512
#define FAT_TWO_OFFSET 512+512+256*512
#define ROOTDIR_OFFSET 512+512+256*512+256*512
#define DATA_OFFSET  512+512+256*512+256*512+512*32


/*
#define FAT_ONE_OFFSET 512
#define FAT_TWO_OFFSET 512+239*256
#define ROOTDIR_OFFSET 512+2*239*512
#define DATA_OFFSET  512+239*512+239*512+512*32
*/

//属性位掩码
#define ATTR_READONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VLABEL 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_ARCHIVE 0x20

//时间掩码
#define MASK_HOUR 0xf800
#define MASK_MIN 0x07e0
#define MASK_SEC 0x001f

//日期掩码
#define MASK_YEAR 0xfe00
#define MASK_MONTH 0x01e0
#define MASK_DAY 0x001f

#define TRUE 1
#define FALSE 0

struct BootDescriptor_t{
    unsigned char Oem_name[9];
    int BytesPerSector;
    int SectorsPerCluster;
    int ReservedSectors;
    int FATs;
    int RootDirEntries;
    int LogicSectors;
    int MediaType;
    int SectorsPerFAT;
    int SectorsPerTrack;
    int Heads;
    int HiddenSectors;

};

struct Entry{
    unsigned char short_name[12];
    unsigned char long_name[27];
    unsigned short year,month,day,hour,min,sec;
    unsigned short FirstCluster;
    unsigned int size;

    unsigned char readonly:1;
    unsigned char hidden:1;
    unsigned char system:1;
    unsigned char vlabel:1;
    unsigned char subdir:1;
    unsigned char archive:1;
};

void do_usage();
int ud_ls();
int ud_cd(char * dir);
int ud_df(char *file_name);
int ud_cf(char *filename,int size);


void findData(unsigned short *year,
                unsigned short *month,
                unsigned short *day,
                unsigned char info[2]
              );

void findTime(unsigned short *hour,
                unsigned short *min,
                unsigned short *sec,
                unsigned char info[2]);

int ReadFat();
int WriteFat();
void ScanBootSector();
void ScanBootEntry();
int ScanEntry(char *entryname,struct Entry *pentry,int mode);
int GetEntry(struct Entry *entry);
void FileNameFormat(unsigned char * name);
unsigned short GetFatCluster(unsigned short prev);
void ClearFatCluster(unsigned short cluster);


int ud;
struct BootDescriptor_t bdptor;
struct Entry *curdir;
int dirno=0;
struct Entry *fatherdir[20];
unsigned char fatbuf[512*239];

#endif
