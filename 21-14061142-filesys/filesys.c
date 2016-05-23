#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include<time.h>
#include "filesys.h"


#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

/*
*¹¦ÄÜ£º´òÓ¡Æô¶¯Ïî¼ÇÂ¼
*/
void ScanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret = read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i = 0; i < 8; i++)
		bdptor.Oem_name[i] = buf[i+0x03];
	bdptor.Oem_name[i] = '\0';

	bdptor.BytesPerSector = RevByte(buf[0x0b],buf[0x0c]);
	bdptor.SectorsPerCluster = buf[0x0d];
	bdptor.ReservedSectors = RevByte(buf[0x0e],buf[0x0f]);
	bdptor.FATs = buf[0x10];
	bdptor.RootDirEntries = RevByte(buf[0x11],buf[0x12]);    
	bdptor.LogicSectors = RevByte(buf[0x13],buf[0x14]);
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);


	printf("Oem_name \t\t%s\n"
		"BytesPerSector \t\t%d\n"
		"SectorsPerCluster \t%d\n"
		"ReservedSector \t\t%d\n"
		"FATs \t\t\t%d\n"
		"RootDirEntries \t\t%d\n"
		"LogicSectors \t\t%d\n"
		"MedioType \t\t%d\n"
		"SectorPerFAT \t\t%d\n"
		"SectorPerTrack \t\t%d\n"
		"Heads \t\t\t%d\n"
		"HiddenSectors \t\t%d\n",
		bdptor.Oem_name,
		bdptor.BytesPerSector,
		bdptor.SectorsPerCluster,
		bdptor.ReservedSectors,
		bdptor.FATs,
		bdptor.RootDirEntries,
		bdptor.LogicSectors,
		bdptor.MediaType,
		bdptor.SectorsPerFAT,
		bdptor.SectorsPerTrack,
		bdptor.Heads,
		bdptor.HiddenSectors);
}

/*ÈÕÆÚ*/
void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2])
{
	int date;
	date = RevByte(info[0],info[1]);

	*year = ((date & MASK_YEAR)>> 9 )+1980;
	*month = ((date & MASK_MONTH)>> 5);
	*day = (date & MASK_DAY);
}

/*Ê±¼ä*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time & MASK_HOUR )>>11);
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) * 2;
}

/*
*ÎÄ¼þÃû¸ñÊ½»¯£¬±ãÓÚ±È½Ï
*/
void FileNameFormat(unsigned char *name)
{
	unsigned char *p = name;
	while(*p!='\0')
		p++;
	p--;
	while(*p==' ')
		p--;
	p++;
	*p = '\0';
}

/*²ÎÊý£ºentry£¬ÀàÐÍ£ºstruct Entry*
*·µ»ØÖµ£º³É¹¦£¬Ôò·µ»ØÆ«ÒÆÖµ£»Ê§°Ü£º·µ»Ø¸ºÖµ
*¹¦ÄÜ£º´Ó¸ùÄ¿Â¼»òÎÄ¼þ´ØÖÐµÃµ½ÎÄ¼þ±íÏî
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*¶ÁÒ»¸öÄ¿Â¼±íÏî£¬¼´32×Ö½Ú*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*³¤ÎÄ¼þÃû£¬ºöÂÔµô*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*ÃüÃû¸ñÊ½»¯£¬Ö÷Òå½áÎ²µÄ'\0'*/
		for (i=0 ;i<=10;i++)
			pentry->short_name[i] = buf[i];
		pentry->short_name[i] = '\0';

		FileNameFormat(pentry->short_name); 



		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->min),&(pentry->sec),info);  

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		pentry->FirstCluster = RevByte(buf[26],buf[27]);
		pentry->size = RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly = (buf[11] & ATTR_READONLY) ?1:0;
		pentry->hidden = (buf[11] & ATTR_HIDDEN) ?1:0;
		pentry->system = (buf[11] & ATTR_SYSTEM) ?1:0;
		pentry->vlabel = (buf[11] & ATTR_VLABEL) ?1:0;
		pentry->subdir = (buf[11] & ATTR_SUBDIR) ?1:0;
		pentry->archive = (buf[11] & ATTR_ARCHIVE) ?1:0;

		return count;
	}
}

/*
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls()
{
    int ret, offset,cluster_addr,dirFloor;
    struct Entry entry;
    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned short currentCluster;
    int i = 0;
    if((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
	perror("read entry failed");



		
    if(curdir == NULL)
		printf("Root_dir\n");
    else{

    	dirFloor = 0;
    	printf("Root_");
    	while(dirFloor < dirno-1){
    		printf("%s_",fatherdir[dirFloor+2]->short_name);
    		dirFloor++;
    	}
    	printf("%s_dir\n",curdir->short_name);
    }
		
	
    printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

    if(curdir == NULL)  /*显示根目录区*/
    {
	/*将fd定位到根目录区的起始地址*/
	if((ret = lseek(fd, ROOTDIR_OFFSET, SEEK_SET)) < 0)
		perror("lseek ROOTDIR_OFFSET failed");
	offset = ROOTDIR_OFFSET;
	/*从根目录区开始遍历，直到数据区起始地址*/
	while(offset < DATA_OFFSET)
	{
	    ret = GetEntry(&entry);
	    offset += abs(ret);
	    if(ret > 0)
	    {
		printf("%12s\t"
		"%d:%d:%d\t"
		"%d:%d:%d   \t"
		"%d\t"
		"%d\t\t"
		"%s\n",
		entry.short_name,
		entry.year,entry.month,entry.day,
		entry.hour,entry.min,entry.sec,
		entry.FirstCluster,
		entry.size,
		(entry.subdir) ? "dir":"file");
	    }
	}
    }
    else /*显示子目录*/
    {
	currentCluster = curdir->FirstCluster;
	do{
	    cluster_addr = DATA_OFFSET + (currentCluster - 2) * CLUSTER_SIZE;
	    if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
	    	perror("lseek cluster_addr failed");
	    offset = cluster_addr;
	    /*只读一簇的内容*/
	    while(offset < cluster_addr + CLUSTER_SIZE)
	    {
		ret = GetEntry(&entry);
		offset += abs(ret);
		if(ret > 0)
		{
		    printf("%12s\t"
		    "%d:%d:%d\t"
		    "%d:%d:%d   \t"
		    "%d\t"
		    "%d\t\t"
		    "%s\n",
		    entry.short_name,
		    entry.year,entry.month,entry.day,
		    entry.hour,entry.min,entry.sec,
		    entry.FirstCluster,
		    entry.size,
		    (entry.subdir) ? "dir":"file");
		}
	    }
	    currentCluster = GetFatCluster(currentCluster);
	} while (currentCluster != 0xffff);
    }
    
    return 1;
} 


/*
*¹¦ÄÜ£ºÏÔÊ¾µ±Ç°Ä¿Â¼µÄÄÚÈÝ
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*/
int fd_ls_r()
{
    int ret, offset, cluster_addr;
    struct Entry entry;
    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned short currentCluster;
    int i, j;

    for (i = 0; i < 4 * 512 * 4 / 32; i++)
	for (j = 0; j < 12; j++)
	    shortname_record[i][j] = '\0'; 
    for (i = 0; i < 4 * 512 * 4 / 32; i++)
	mode_record[i] = 0; 
    record = 0;

    currentCluster = curdir->FirstCluster;
    do{
	cluster_addr = DATA_OFFSET + (currentCluster - 2) * CLUSTER_SIZE;
    	if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
	    perror("lseek cluster_addr failed");
	offset = cluster_addr;
	while(offset < cluster_addr + CLUSTER_SIZE)
	{
	    ret = GetEntry(&entry);
	    offset += abs(ret);
	    if(ret > 0)
	    {
		for(i = 0; i < strlen(entry.short_name); i++)
		    shortname_record[record][i] = entry.short_name[i];
	        shortname_record[record][i]= '\0';
	        mode_record[record] = (entry.subdir) ? 1 : 0;
	        record++;
 	    }
	}
	currentCluster = GetFatCluster(currentCluster);
    } while (currentCluster != 0xffff);
    
    return 1;
}

/*
*²ÎÊý£ºentryname ÀàÐÍ£ºchar
£ºpentry    ÀàÐÍ£ºstruct Entry*
£ºmode      ÀàÐÍ£ºint£¬mode=1£¬ÎªÄ¿Â¼±íÏî£»mode=0£¬ÎªÎÄ¼þ
*·µ»ØÖµ£ºÆ«ÒÆÖµ´óÓÚ0£¬Ôò³É¹¦£»-1£¬ÔòÊ§°Ü
*¹¦ÄÜ£ºËÑË÷µ±Ç°Ä¿Â¼£¬²éÕÒÎÄ¼þ»òÄ¿Â¼Ïî
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
    int ret,offset,i;
    int cluster_addr;
    unsigned short currentCluster;
    char uppername[80];

    for(i = 0; i < strlen(entryname); i++)
	uppername[i] = toupper(entryname[i]);
    uppername[i] = '\0';
    /*É¨Ãè¸ùÄ¿Â¼*/
    if(curdir ==NULL)  
    {
	if((ret = lseek(fd, ROOTDIR_OFFSET, SEEK_SET)) < 0)
	    perror ("lseek ROOTDIR_OFFSET failed");
	offset = ROOTDIR_OFFSET;
	while(offset < DATA_OFFSET)
	{
	    ret = GetEntry(pentry);
	    offset += abs(ret);
 	    if (ret > 0) 
	    {
	        if(pentry->subdir == mode && strcmp((char*)pentry->short_name,  uppername) == 0)
	        return offset;
	    }
	}
	return -1;
    }
    /*É¨Ãè×ÓÄ¿Â¼*/
    else  
    {
	currentCluster = curdir->FirstCluster;
	do
	{
	    cluster_addr = DATA_OFFSET + (currentCluster - 2) * CLUSTER_SIZE;
	    if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
	    	perror("lseek cluster_addr failed");
	    offset = cluster_addr;
	    while(offset < cluster_addr + CLUSTER_SIZE)
	    {
	        ret = GetEntry(pentry);
	        offset += abs(ret);
	    	if(ret > 0)
	    	{
		    if(pentry->subdir == mode && strcmp((char*)pentry->short_name, uppername) == 0)
		    return offset;
 	    	}
	    }
	    currentCluster = GetFatCluster(currentCluster);
    	} while (currentCluster != 0xffff);
	return -1;
    }
}

/*
*²ÎÊý£ºdir£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*/
int fd_cd_f(char *dir)
{
    struct Entry * pentry;
    struct Entry * curdir_temp_temp;
    int ret;
    int i, j;
    char dir_str[1000];

    for (i = 0; i < 1000; i++)
	dir_str[i] = '\0';

    for (i = 1; i < strlen(dir) && dir[i] != '/'; i++)
    	dir_str[i - 1] = dir[i];
    dir_str[i - 1] = '\0';

    if(strcmp(dir_str, ".") == 0
   || (strcmp(dir_str, "..") == 0 && curdir_temp == NULL))
    {
	for (i = 1; i < strlen(dir) && dir[i] != '/'; i++)
    	    ;
	if (i < strlen(dir))
	{
	    for (j = 0; j < 1000; j++)
		dir_str[j] = '\0';
	    for (j = i; j < strlen(dir); j++)
		dir_str[j - i] = dir[j];
	    return fd_cd_f(dir_str);
	}
	else
	    return 1;
    }
    else if(strcmp(dir_str, "..") == 0 && curdir_temp != NULL)
    {
	curdir_temp = fatherdir_temp[dirno_temp];
	dirno_temp--; 
	for (i = 1; i < strlen(dir) && dir[i] != '/'; i++)
    	    ;
	if (i < strlen(dir))
	{
	    for (j = 0; j < 1000; j++)
		dir_str[j] = '\0';
	    for (j = i; j < strlen(dir); j++)
		dir_str[j - i] = dir[j];
	    return fd_cd_f(dir_str);
	}
	else
	    return 1;
    }
    else
    {
    	pentry = (struct Entry * )malloc(sizeof(struct Entry));
    
    	curdir_temp_temp = curdir;
    	curdir = curdir_temp;
    	ret = ScanEntry(dir_str, pentry, 1);

    	if(ret < 0)
    	{
	    curdir = curdir_temp_temp;
	    free(pentry);
	    return -1;
   	}
    	else
    	{
	    curdir = curdir_temp_temp;
	    dirno_temp++;
	    fatherdir_temp[dirno_temp] = curdir_temp;
  	    curdir_temp = pentry;
	    for (i = 1; i < strlen(dir) && dir[i] != '/'; i++)
    	    	;
	    if (i < strlen(dir))
	    {
	    	for (j = 0; j < 1000; j++)
		    dir_str[j] = '\0';
	    	for (j = i; j < strlen(dir); j++)
		    dir_str[j - i] = dir[j];
	    	return fd_cd_f(dir_str);
	    }
	    else
	    	return 1;
    	}
    }
}

/*
*²ÎÊý£ºdir£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£º¸Ä±äÄ¿Â¼µ½¸¸Ä¿Â¼»ò×ÓÄ¿Â¼
*/
int fd_cd(char *dir)
{
    struct Entry * pentry;
    int ret;
    int i, j;
    char dir_str[1000];

    if (dir[0] == '/')
    {
	for (i = 0; i < strlen(dir); i++)
	    dir_str[i] = dir[i];
	dir_str[i] = '\0';
	
	i = 0;
	while (i < strlen(dir_str)) 
	{
	    if (i + 1 < strlen(dir_str) && dir_str[i] == dir_str[i + 1])
	    {
		for (j = i + 1; j < strlen(dir_str) - 1; j++)
		    dir_str[j] = dir_str[j + 1];
		dir_str[j] = '\0';
	    }
	    else
 	    {
		i++;
		for (; i < strlen(dir_str); i++)
		    if (dir_str[i] == '/')
			break;
	    }
	}
	i--;
	if (dir_str[i] == '/')
	    dir_str[i] = '\0';
	
	if (strlen(dir_str) <= 1)
	{
	    printf("no such dir\n");
	    return -1;
	}
	
	if (strlen(dir_str) >= 5
	 && dir_str[1] == 'R' 
	 && dir_str[2] == 'o' 
	 && dir_str[3] == 'o' 
	 && dir_str[4] == 't')
	{
	    if (strlen(dir_str) == 5)
	    {
		curdir = NULL;
		dirno = 0;
		for (i = 0; i < 10; i++)
	   	    fatherdir[i] = NULL;
		return 1;
	    }
	    else if (dir_str[5] == '/')
	    {
		for (i = 5; i < strlen(dir_str); i++)
		    dir_str[i - 5] = dir_str[i];
		dir_str[i - 5] = '\0';
	    }
	    else
	    {
	    	printf("no such dir\n");
	        return -1;
	    }	
	}

	curdir_temp = NULL;
    	dirno_temp = 0;
	for (i = 0; i < 10; i++)
	    fatherdir_temp[i] = NULL;
    }
    else
    {
	dir_str[0] = '/';
	for (i = 0; i < strlen(dir); i++)
	    dir_str[i + 1] = dir[i];
	dir_str[i + 1] = '\0';

	i = 0;
	while (i < strlen(dir_str)) 
	{
	    if (i + 1 < strlen(dir_str) && dir_str[i] == dir_str[i + 1])
	    {
		for (j = i + 1; j < strlen(dir_str) - 1; j++)
		    dir_str[j] = dir_str[j + 1];
		dir_str[j] = '\0';
	    }
	    else
 	    {
		i++;
		for (; i < strlen(dir_str); i++)
		    if (dir_str[i] == '/')
			break;
	    }
	}
	i--;
	if (dir_str[i] == '/')
	    dir_str[i] = '\0';

	if (strlen(dir_str) <= 1)
	{
	    printf("no such dir\n");
	    return -1;
	}

	curdir_temp = curdir;
	dirno_temp = 0;
	for (i = 0; i < 10; i++)
	    fatherdir_temp[i] = NULL;
	dirno_temp = dirno;
	for (i = 0; i <= dirno; i++)
	    fatherdir_temp[i] = fatherdir[i];
    }

    if (fd_cd_f(dir_str) == 1)
    {
	curdir = curdir_temp;
	dirno = 0;
	for (i = 0; i < 10; i++)
	    fatherdir[i] = NULL;
	dirno = dirno_temp;
	for (i = 0; i <= dirno_temp; i++)
	    fatherdir[i] = fatherdir_temp[i];
	return 1;
    }
    else
    {
	printf("no such dir\n");
	return -1;
    }
}

/*
*²ÎÊý£ºprev£¬ÀàÐÍ£ºunsigned char
*·µ»ØÖµ£ºÏÂÒ»´Ø
*ÔÚfat±íÖÐ»ñµÃÏÂÒ»´ØµÄÎ»ÖÃ
*/
unsigned short GetFatCluster(unsigned short prev)
{
	unsigned short next;
	int index;

	index = prev * 2;
	next = RevByte(fatbuf[index],fatbuf[index+1]);

	return next;
}

/*
*²ÎÊý£ºcluster£¬ÀàÐÍ£ºunsigned short
*·µ»ØÖµ£ºvoid
*¹¦ÄÜ£ºÇå³ýfat±íÖÐµÄ´ØÐÅÏ¢
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*½«¸Ä±äµÄfat±íÖµÐ´»Øfat±í
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,512*256)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*256))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*¶Áfat±íµÄÐÅÏ¢£¬´æÈëfatbuf[]ÖÐ
*/
int ReadFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,512*256)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar
mode£¬ÀàÐÍ£ºint£¬mode=1£¬ÎªÉ¾³ýÄ¿Â¼£»mode=0£¬ÎªÉ¾³ýÎÄ¼þ
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ;É¾³ýµ±Ç°Ä¿Â¼ÏÂµÄÄ¿Â¼/ÎÄ¼þ
*/
int fd_df(char *filename, int mode)
{
    struct Entry * pentry;
    struct Entry * curdir_temp;
    int ret;
    unsigned char c;
    unsigned short seed, next;
    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned char y;
    int i, j;
    unsigned char shortname_record_temp[4 * 512 * 4 / 32][12];
    int mode_record_temp[4 * 512 * 4 / 32];
    int record_temp;

    pentry = (struct Entry * )malloc(sizeof(struct Entry));
    input_request = 0;

    /*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
    if (mode == 1)
	ret = ScanEntry(filename, pentry, 1);
    else 
	ret = ScanEntry(filename, pentry, 0);
	
    if (ret < 0)
    {
	if (mode == 1)
	    printf("no such dir\n");
	else
	    printf("no such file\n");
	free(pentry);
	return -1;
    }

    if (mode == 1) 
    {
	curdir_temp = curdir;
	curdir = pentry;
	fd_ls_r();
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	    for (j = 0; j < 12; j++)
	        shortname_record_temp[i][j] = '\0'; 
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	        mode_record_temp[i] = 0; 
    	record_temp = 0;
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	    for (j = 0; j < 12; j++)
	        shortname_record_temp[i][j] = shortname_record[i][j]; 
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	        mode_record_temp[i] = mode_record[i]; 
    	record_temp = record;
	curdir = curdir_temp;
	if (strlen(shortname_record[0]) != 0)
	{
	    curdir_temp = curdir;
	    curdir = pentry;
	    if (input_request == 0)
	    {
		printf("Dir is not empty, press Y to continue\n");
		if ((y = getchar()) != 'Y')
		    return -1;
		while (y != '\n')
		    y = getchar(); 
		input_request == 1;  
		for (i = 0; i < record_temp; i++)
		    fd_df(shortname_record_temp[i], mode_record_temp[i]);
	    }	
	    curdir = curdir_temp;
	}
    }

    /*Çå³ýfat±íÏî*/
    seed = pentry->FirstCluster;
    while ((next = GetFatCluster(seed)) != 0xffff)
    {
	ClearFatCluster(seed);
	seed = next;
    }
    ClearFatCluster(seed);
    /*Çå³ýÄ¿Â¼±íÏî*/
    c = 0xe5;
    if (lseek(fd, ret - 0x20, SEEK_SET) < 0)
	perror("lseek fd_df failed");
    if (write(fd, &c, 1) < 0)
	perror("write failed");
    if (lseek(fd, ret, SEEK_SET) < 0)
	perror("lseek fd_df failed");
    while (ret = read(fd, buf, DIR_ENTRY_SIZE)) 
    {
	if (ret < 0)
	    break;
	else 
        {
	    if (buf[0] != 0xe5 && buf[0] != 0x00 && buf[11] == 0x0f)
	    {
		if(lseek(fd, ret - 0x20, SEEK_SET) < 0)
		    perror("lseek fd_df failed");
		if(write(fd, &c, 1) < 0)
		    perror("write failed");
		if(lseek(fd, ret, SEEK_SET) < 0)
		    perror("lseek fd_df failed");
	    }
	    else
		break;
	}
    }

    free(pentry);
    if (WriteFat() < 0)
	exit(1);
    return 1;
}
/*

*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar
mode£¬ÀàÐÍ£ºint£¬mode=1£¬ÎªÉ¾³ýÄ¿Â¼£»mode=0£¬ÎªÉ¾³ýÎÄ¼þ

*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ;É¾³ýµ±Ç°Ä¿Â¼ÏÂµÄÄ¿Â¼/ÎÄ¼þ
*/
int fd_df_0(char *filename, int mode)
{
    struct Entry * pentry;
    struct Entry * curdir_temp;
    int ret;
    unsigned char c;
    unsigned short seed, next;
    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned char y;
    int i, j;
    unsigned char shortname_record_temp[4 * 512 * 4 / 32][12];
    int mode_record_temp[4 * 512 * 4 / 32];
    int record_temp;

    pentry = (struct Entry * )malloc(sizeof(struct Entry));

    /*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
    if (mode == 1)
	ret = ScanEntry(filename, pentry, 1);
    else 
	ret = ScanEntry(filename, pentry, 0);
	
    if (ret < 0)
    {
	if (mode == 1)
	    printf("no such dir\n");
	else
	    printf("no such file\n");
	free(pentry);
	return -1;
    }

    if (mode == 1) 
    {
	curdir_temp = curdir;
	curdir = pentry;
	fd_ls_r();
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	    for (j = 0; j < 12; j++)
	        shortname_record_temp[i][j] = '\0'; 
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	        mode_record_temp[i] = 0; 
    	record_temp = 0;
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	    for (j = 0; j < 12; j++)
	        shortname_record_temp[i][j] = shortname_record[i][j]; 
	for (i = 0; i < 4 * 512 * 4 / 32; i++)
	        mode_record_temp[i] = mode_record[i]; 
    	record_temp = record;	
	curdir = curdir_temp;
	if (strlen(shortname_record[0]) != 0)
	{
	    curdir_temp = curdir;
	    curdir = pentry; 
	    for (i = 0; i < record_temp; i++)
		fd_df_0(shortname_record_temp[i], mode_record_temp[i]);
	    curdir = curdir_temp;
	}
    }

    /*Çå³ýfat±íÏî*/
    seed = pentry->FirstCluster;
    while ((next = GetFatCluster(seed)) != 0xffff)
    {
	ClearFatCluster(seed);
	seed = next;
    }
    ClearFatCluster(seed);
    /*Çå³ýÄ¿Â¼±íÏî*/
    c = 0xe5;
    if (lseek(fd, ret - 0x20, SEEK_SET) < 0)
	perror("lseek fd_df failed");
    if (write(fd, &c, 1) < 0)
	perror("write failed");
    if (lseek(fd, ret, SEEK_SET) < 0)
	perror("lseek fd_df failed");
    while (ret = read(fd, buf, DIR_ENTRY_SIZE)) 
    {
	if (ret < 0)
	    break;
	else 
        {
	    if (buf[0] != 0xe5 && buf[0] != 0x00 && buf[11] == 0x0f)
	    {
		if(lseek(fd, ret - 0x20, SEEK_SET) < 0)
		    perror("lseek fd_df failed");
		if(write(fd, &c, 1) < 0)
		    perror("write failed");
		if(lseek(fd, ret, SEEK_SET) < 0)
		    perror("lseek fd_df failed");
	    }
	    else
		break;
	}
    }

    free(pentry);
    if (WriteFat() < 0)
	exit(1);
    return 1;
}
/*
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能 读取某文件的内容;
*/
int fd_of(char *filename)
{
    struct Entry * pentry;
    struct Entry * curdir_temp;
    int ret;
    unsigned char c;
    unsigned short seed, next;
    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned char y;
    int i, j;
    unsigned char shortname_record_temp[4 * 512 * 4 / 32][12];
    char databuf[CLUSTER_SIZE*4+1];
    int mode_record_temp[4 * 512 * 4 / 32];
    int record_temp,cluster_addr;

    pentry = (struct Entry * )malloc(sizeof(struct Entry));
    input_request = 0;
    /*扫描当前目录查找文件*/
	ret = ScanEntry(filename, pentry, 0);
	
    if (ret < 0)
    {
	    printf("no such file\n");
	return -1;
    }
	
    seed = pentry->FirstCluster;
    while ((next = GetFatCluster(seed)) != 0xffff)
    {
	cluster_addr = DATA_OFFSET + (seed - 2) * CLUSTER_SIZE;
	if (lseek(fd, cluster_addr, SEEK_SET) < 0)
		perror("lseek fd_of failed");
	if ((ret = read(fd, databuf, CLUSTER_SIZE)) < 0)
		perror("read data failed");
	seed = next;
    }
    cluster_addr = DATA_OFFSET + (seed - 2) * CLUSTER_SIZE;
	if (lseek(fd, cluster_addr, SEEK_SET) < 0)
		perror("lseek fd_of failed");
	if ((ret = read(fd, databuf, CLUSTER_SIZE)) < 0)
		 perror("read data failed");
	printf("%s\n",databuf);
    return 1;
}


/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar£¬´´½¨Ä¿Â¼/ÎÄ¼þµÄÃû³Æ
size£¬    ÀàÐÍ£ºint£¬Ä¿Â¼/ÎÄ¼þµÄ´óÐ¡
mode£¬    ÀàÐÍ£ºint£¬mode=1£¬Îª´´½¨Ä¿Â¼£»mode=0£¬Îª´´½¨ÎÄ¼þ
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£ºÔÚµ±Ç°Ä¿Â¼ÏÂ´´½¨Ä¿Â¼/ÎÄ¼þ
*/
int fd_cf(char *filename,int size, int mode)
{

    struct Entry * pentry;
    int ret, i = 0, cluster_addr, offset;
    unsigned short cluster, clusterno[100], currentCluster;
    unsigned char c[DIR_ENTRY_SIZE];
    int index, clustersize;
    unsigned char buf[DIR_ENTRY_SIZE];
    pentry = (struct Entry * ) malloc (sizeof(struct Entry));

    clustersize = (size / (CLUSTER_SIZE));
    if (size % (CLUSTER_SIZE) != 0)
	clustersize ++;
	
    if (mode == 1 && clustersize > 4)
	perror("size of dir is out of range");
    if (mode == 0 && clustersize > 4)
	perror("size of file is out of range");

    //É¨Ãè¸ùÄ¿Â¼£¬ÊÇ·ñÒÑ´æÔÚ¸ÃÎÄ¼þÃû
    if (mode == 1)
	ret = ScanEntry(filename,pentry,1);
    else
	ret = ScanEntry(filename,pentry,0);
	
    if (ret < 0)
    {
	/*²éÑ¯fat±í£¬ÕÒµ½¿Õ°×´Ø£¬±£´æÔÚclusterno[]ÖÐ*/
	for (cluster = 2; cluster < (512 * 256 / 2 - 1); cluster++)
	{
	    index = cluster * 2;
	    if (fatbuf[index] == 0x00 && fatbuf[index + 1] == 0x00)
	    {
		clusterno[i] = cluster;
		i++;
		if(i == clustersize)
		    break;
	    }
	}
	/*ÔÚfat±íÖÐÐ´ÈëÏÂÒ»´ØÐÅÏ¢*/
	for (i = 0; i < clustersize - 1; i++)
	{
	    index = clusterno[i] * 2;
	    fatbuf[index] = (clusterno[i + 1] &  0x00ff);
	    fatbuf[index + 1] = ((clusterno[i + 1] & 0xff00) >> 8);
	}
	/*×îºóÒ»´ØÐ´Èë0xffff*/
	index = clusterno[i] * 2;
	fatbuf[index] = 0xff;
	fatbuf[index + 1] = 0xff;

	if (curdir == NULL)  /*Íù¸ùÄ¿Â¼ÏÂÐ´ÎÄ¼þ*/
	{ 
	    if ((ret = lseek(fd, ROOTDIR_OFFSET, SEEK_SET)) < 0)
		perror("lseek ROOTDIR_OFFSET failed");
	    offset = ROOTDIR_OFFSET;
	    while (offset < DATA_OFFSET)
	    {
		if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
		    perror("read entry failed");
		offset += abs(ret);
		if (buf[0] != 0xe5 && buf[0] != 0x00)
		{
		    while (buf[11] == 0x0f)
		    {
			if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
			    perror("read root dir failed");
			offset += abs(ret);
		    }
		}
		/*ÕÒ³ö¿ÕÄ¿Â¼Ïî»òÒÑÉ¾³ýµÄÄ¿Â¼Ïî*/ 
		else
		{       
		    offset = offset - abs(ret);     
		    for (i = 0; i <= strlen(filename); i++)
		 	c[i] = toupper(filename[i]);			
		    for (; i <= 10; i++)
			c[i] = ' ';
		    if (mode == 1)
			c[11] = 0x11;
		    else
		        c[11] = 0x01;

		    time_t timep;  
   		    struct tm *p;  
   		    time(&timep);  
    		    p = gmtime(&timep); 

		    int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
		    char datac[2];
		    datac[1]=(datetmp/256);	
		    datac[0]=(datetmp%256);	
		    c[24]=datac[0];
		    c[25]=datac[1];

		    int timetmp=(((p->tm_hour+8)%24)*2048+p->tm_min*32+p->tm_sec/2);
		    char timec[2];
		    timec[1]=(timetmp/256);
		    timec[0]=(timetmp%256);
		    c[22]=timec[0];
		    c[23]=timec[1];


		    /*Ð´µÚÒ»´ØµÄÖµ*/
		    c[26] = (clusterno[0] & 0x00ff);
		    c[27] = ((clusterno[0] & 0xff00) >> 8);
		    /*Ð´ÎÄ¼þµÄ´óÐ¡*/
		    c[28] = (size & 0x000000ff);
		    c[29] = ((size & 0x0000ff00) >> 8);
	            c[30] = ((size & 0x00ff0000) >> 16);
		    c[31] = ((size & 0xff000000) >> 24);
		    if (lseek(fd, offset, SEEK_SET) < 0)
			perror("lseek fd_cf failed");
		    if (write(fd, &c, DIR_ENTRY_SIZE) < 0)
			perror("write failed");
		    free(pentry);
		    if (WriteFat() < 0)
			exit(1);
		    return 1;
		}

	    }
	}
	else 
	{
	    currentCluster = curdir->FirstCluster;
	    do
	    {
	        cluster_addr = DATA_OFFSET + (currentCluster - 2) * CLUSTER_SIZE;
	        if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
	    	    perror("lseek cluster_addr failed");
	        offset = cluster_addr;
	    	while(offset < cluster_addr + CLUSTER_SIZE)
	    	{
	            if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
		    	perror("read entry failed");
		    offset += abs(ret);
		    if (buf[0] != 0xe5 && buf[0] != 0x00)
		    {
		    	while (buf[11] == 0x0f)
		    	{
		            if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
			    	perror("read root dir failed");
			    offset += abs(ret);
		    	}
		    }
		    else
		    { 
		        offset = offset - abs(ret);      
		        for (i = 0; i <= strlen(filename); i++)
			    c[i] = toupper(filename[i]);
		    	for (; i <= 10; i++)
			    c[i]=' ';
		    	if (mode == 1)
			    c[11] = 0x11;
		    	else
			    c[11] = 0x01;
			
			time_t timep;  
   		    	struct tm *p;  
   		    	time(&timep);  
    		    	p = gmtime(&timep); 

		    	int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
		    	char datac[2];
		    	datac[1]=(datetmp/256);	
		    	datac[0]=(datetmp%256);	
		    	c[24]=datac[0];
		    	c[25]=datac[1];

		    	int timetmp=(((p->tm_hour+8)%24)*2048+p->tm_min*32+p->tm_sec/2);
		    	char timec[2];
		    	timec[1]=(timetmp/256);
		    	timec[0]=(timetmp%256);
		    	c[22]=timec[0];
		    	c[23]=timec[1];

			

		    	c[26] = (clusterno[0] & 0x00ff);
		     	c[27] = ((clusterno[0] & 0xff00) >> 8);
		    	c[28] = (size & 0x000000ff);
		    	c[29] = ((size & 0x0000ff00) >> 8);
		    	c[30] = ((size & 0x00ff0000) >> 16);
		    	c[31] = ((size & 0xff000000) >> 24);

		    	if (lseek(fd, offset, SEEK_SET) < 0)
			    perror("lseek fd_cf failed");
		    	if (write(fd, &c, DIR_ENTRY_SIZE) < 0)
			    perror("write failed");
        	        free(pentry);
		        if (WriteFat() < 0)
			    exit(1);
		    	return 1;
		    }
	    	}
	    	currentCluster = GetFatCluster(currentCluster);
    	    } while (currentCluster != 0xffff);
	}
    }
    else
    {
	if (mode == 1)
	    printf("This dirname is exist\n");
	else
	    printf("This filename is exist\n");
	free(pentry);
	return -1;
    }
	return 1;

}


/*
*参数：filename，类型：char，创建文件的名称
wdata，    类型：char，写入文件的内容
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件并向文件写入指定内容
*/
int fd_cfile(char *filename,char *wdata)
{

    struct Entry * pentry;
    int ret, i = 0, cluster_addr, offset,size;
    unsigned short cluster, clusterno[100], currentCluster;
    unsigned char c[DIR_ENTRY_SIZE];
    int index, clustersize, curclu;
    unsigned char buf[DIR_ENTRY_SIZE];
    pentry = (struct Entry * ) malloc (sizeof(struct Entry));
	size = strlen(wdata);
    clustersize = (size / (CLUSTER_SIZE));
    if (size % (CLUSTER_SIZE) != 0)
	clustersize ++;
	//printf("a\n");
    if (clustersize > 4)
	perror("size of file is out of range");

    //扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	
    if (ret < 0)
    {
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for (cluster = 2; cluster < (512 * 256 / 2 - 1); cluster++)
		{
			index = cluster * 2;
			if (fatbuf[index] == 0x00 && fatbuf[index + 1] == 0x00)
			{
			clusterno[i] = cluster;
			i++;
			if(i == clustersize)
				break;
			}
		}
		/*在fat表中写入下一簇信息*/
		for (i = 0; i < clustersize - 1; i++)
		{
			index = clusterno[i] * 2;
			fatbuf[index] = (clusterno[i + 1] &  0x00ff);
			fatbuf[index + 1] = ((clusterno[i + 1] & 0xff00) >> 8);
		}
		curclu = clusterno[0];
		/*最后一簇写入0xffff*/
		index = clusterno[i] * 2;
		fatbuf[index] = 0xff;
		fatbuf[index + 1] = 0xff;

		if (curdir == NULL)  /*往根目录下写文件*/
		{ 
			if ((ret = lseek(fd, ROOTDIR_OFFSET, SEEK_SET)) < 0)
			perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET;
			while (offset < DATA_OFFSET)
			{
			if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
				perror("read entry failed");
			offset += abs(ret);
			if (buf[0] != 0xe5 && buf[0] != 0x00)
			{
				while (buf[11] == 0x0f)
				{
				if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
					perror("read root dir failed");
				offset += abs(ret);
				}
			}
			/*找出空目录项或已删除的目录项*/ 
			else
			{       
				offset = offset - abs(ret);     
				for (i = 0; i <= strlen(filename); i++)
				c[i] = toupper(filename[i]);			
				for (; i <= 10; i++)
				c[i] = ' ';
				c[11] = 0x01;
				/*写第一簇的值*/
				
				time_t timep;  
   		    		struct tm *p;  
   		   		 time(&timep);  
    		   		 p = gmtime(&timep); 

		   		 int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
		    		char datac[2];
		    		datac[1]=(datetmp/256);	
		    		datac[0]=(datetmp%256);	
		    		c[24]=datac[0];
		    		c[25]=datac[1];

		    		int timetmp=(((p->tm_hour+8)%24)*2048+p->tm_min*32+p->tm_sec/2);
		    		char timec[2];
		    		timec[1]=(timetmp/256);
		    		timec[0]=(timetmp%256);
		   		c[22]=timec[0];
		    		c[23]=timec[1];

				
				c[26] = (clusterno[0] & 0x00ff);
				c[27] = ((clusterno[0] & 0xff00) >> 8);
				/*写文件的大小*/
				c[28] = (size & 0x000000ff);
				c[29] = ((size & 0x0000ff00) >> 8);
					c[30] = ((size & 0x00ff0000) >> 16);
				c[31] = ((size & 0xff000000) >> 24);
				if (lseek(fd, offset, SEEK_SET) < 0)
				perror("lseek fd_cf failed");
				if (write(fd, &c, DIR_ENTRY_SIZE) < 0)
				perror("write failed");
				free(pentry);
				if (WriteFat() < 0)
				exit(1);
				for(i = 0;i < clustersize;++i){
					cluster_addr = DATA_OFFSET + (curclu - 2) * CLUSTER_SIZE;
					if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
						perror("lseek cluster_addr failed");
					if((ret = write(fd, wdata + i * CLUSTER_SIZE, CLUSTER_SIZE)) < 0)
						perror("write data failed");
					curclu = fatbuf[curclu * 2] + fatbuf[curclu * 2 + 1]<<8;
				}
				return 1;
			}

			}
		}
		else 
		{
			currentCluster = curdir->FirstCluster;
			do
			{
				cluster_addr = DATA_OFFSET + (currentCluster - 2) * CLUSTER_SIZE;
				if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
					perror("lseek cluster_addr failed");
				offset = cluster_addr;
				while(offset < cluster_addr + CLUSTER_SIZE)
				{
					if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
						perror("read entry failed");
					offset += abs(ret);
					if (buf[0] != 0xe5 && buf[0] != 0x00)
					{
						while (buf[11] == 0x0f)
						{
							if ((ret = read(fd, buf, DIR_ENTRY_SIZE)) < 0)
								perror("read root dir failed");
							offset += abs(ret);
						}
					}
					else
					{ 
						offset = offset - abs(ret);      
						for (i = 0; i <= strlen(filename); i++)
							c[i] = toupper(filename[i]);
						for (; i <= 10; i++)
							c[i]=' ';
						c[11] = 0x01;

						time_t timep;  
   		    				struct tm *p;  
   		    				time(&timep);  
    		    				p = gmtime(&timep); 

		    				int datetmp=((1900+p->tm_year-1980)*512+(1+p->tm_mon)*32+ p->tm_mday);	
		    				char datac[2];
		    				datac[1]=(datetmp/256);	
		    				datac[0]=(datetmp%256);	
		    				c[24]=datac[0];
		    				c[25]=datac[1];

		    				int timetmp=(((p->tm_hour+8)%24)*2048+p->tm_min*32+p->tm_sec/2);
		   				char timec[2];
		   			 	timec[1]=(timetmp/256);
		   			 	timec[0]=(timetmp%256);
		    				c[22]=timec[0];
		    				c[23]=timec[1];

						c[26] = (clusterno[0] & 0x00ff);
						c[27] = ((clusterno[0] & 0xff00) >> 8);
						c[28] = (size & 0x000000ff);
						c[29] = ((size & 0x0000ff00) >> 8);
						c[30] = ((size & 0x00ff0000) >> 16);
						c[31] = ((size & 0xff000000) >> 24);

						if (lseek(fd, offset, SEEK_SET) < 0)
							perror("lseek fd_cf failed");
						if (write(fd, &c, DIR_ENTRY_SIZE) < 0)
							perror("write failed");
						free(pentry);
						if (WriteFat() < 0)
							exit(1);
						for(i = 0;i < clustersize;++i){
							cluster_addr = DATA_OFFSET + (curclu - 2) * CLUSTER_SIZE;
							if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
								perror("lseek cluster_addr failed");
							if((ret = write(fd, wdata + i * CLUSTER_SIZE, CLUSTER_SIZE)) < 0)
								perror("write data failed");
							curclu = fatbuf[curclu * 2] + fatbuf[curclu * 2 + 1]<<8;
						}
						return 1;
					}
				}
				currentCluster = GetFatCluster(currentCluster);
			} while (currentCluster != 0xffff);
		}
    }
    else
    {
	    printf("This filename is exist\n");
		free(pentry);
		return -1;
    }
	return 1;

}


void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <data> \tcreate a file\n\tof <filename> \t\topen a file\n\tmkdir <dirname> <size> \tcreate a dir\n\tdf <file> \t\tdelete a file\n\trm <file/dir> <mode>\tdelete a file/dir\n\trm -r <dir> \t\tdelete a dir\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	int size=0;
	char *wdata;
	char name[12];
	struct Entry * curdir_temp;

	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	ScanBootSector();
	if(ReadFat()<0)
		exit(1);

	do_usage();
	while (1)
	{
		printf(">");
		scanf("%s",input);
		if (strcmp(input, "exit") == 0) 
		{
			getchar();
			break;
		}
		else if (strcmp(input, "ls") == 0)
		{
			getchar();
			
			fd_ls();
		}
		else if(strcmp(input, "cd") == 0)
		{
			scanf("%s", name);
			getchar();
			fd_cd(name);
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			getchar();
			fd_df(name, 0);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			scanf("%s", wdata);
			getchar();
			fd_cfile(name, wdata);
		}
		else if(strcmp(input, "mkdir") == 0)
		{
			scanf("%s", name);
			scanf("%s", input);
			size = atoi(input);
			getchar();
			fd_cf(name, size, 1);
		}
		else if(strcmp(input,"of")==0){
			scanf("%s", name);
			getchar();
			fd_of(name);
		}
		else if(strcmp(input, "rm") == 0)
		{
			scanf("%s", name);
			if(strcmp(name, "-r") == 0)
			{
			    scanf("%s", input);
			    getchar();
			    fd_df_0(input, 1);
			}
			else
			{
			    scanf("%s", input);
			    size = atoi(input);
			    getchar();
			    fd_df(name, size);
			}
		}
		else if (strcmp(input, "ls:") == 0)
		{
			scanf("%s", name);
			getchar();
			curdir_temp = curdir;
			fd_cd(name);
			fd_ls();
			curdir = curdir_temp;
		}
		else
			do_usage();
	}	

	return 0;
}


