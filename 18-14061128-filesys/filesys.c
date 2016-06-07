#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
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
*¹¦ÄÜ£ºÏÔÊ¾µ±Ç°Ä¿Â¼µÄÄÚÈÝ
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*/
int fd_ls()
{
	int ret, offset,cluster_addr;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",curdir->short_name);
	printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*ÏÔÊ¾¸ùÄ¿Â¼Çø*/
	{
		/*½«fd¶¨Î»µ½¸ùÄ¿Â¼ÇøµÄÆðÊ¼µØÖ·*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*´Ó¸ùÄ¿Â¼Çø¿ªÊ¼±éÀú£¬Ö±µ½Êý¾ÝÇøÆðÊ¼µØÖ·*/
		while(offset < (DATA_OFFSET))
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

	else /*ÏÔÊ¾×ÓÄ¿Â¼*/
	{
		cluster_addr = DATA_OFFSET + (curdir->FirstCluster-2) * CLUSTER_SIZE ;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");

		offset = cluster_addr;

		/*Ö»¶ÁÒ»´ØµÄÄÚÈÝ*/
		while(offset<cluster_addr +CLUSTER_SIZE)
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
	return 0;
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
	char uppername[80];
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*É¨Ãè¸ùÄ¿Â¼*/
	if(curdir ==NULL)  
	{
		if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror ("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;


		while(offset<DATA_OFFSET)
		{
			ret = GetEntry(pentry);
			offset +=abs(ret);

			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))

				return offset;

		}
		return -1;
	}

	/*É¨Ãè×ÓÄ¿Â¼*/
	else  
	{
		cluster_addr = DATA_OFFSET + (curdir->FirstCluster -2)*CLUSTER_SIZE;
		if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		offset= cluster_addr;

		while(offset<cluster_addr + CLUSTER_SIZE)
		{
			ret= GetEntry(pentry);
			offset += abs(ret);
			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername))
				return offset;



		}
		return -1;
	}
}



/*
*²ÎÊý£ºdir£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£º¸Ä±äÄ¿Â¼µ½¸¸Ä¿Â¼»ò×ÓÄ¿Â¼
*/
int fd_cd(char *dir)
{
	struct Entry *pentry;
	int ret;

	if(!strcmp(dir,"."))
	{
		return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL)
		return 1;
	/*·µ»ØÉÏÒ»¼¶Ä¿Â¼*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		curdir = fatherdir[dirno];
		dirno--; 
		return 1;
	}
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	ret = ScanEntry(dir,pentry,1);
	if(ret < 0)
	{
		printf("no such dir\n");
		free(pentry);
		return -1;
	}
	dirno ++;
	fatherdir[dirno] = curdir;
	curdir = pentry;
	return 1;
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
	if(write(fd,fatbuf,512*250)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,512*250))<0)
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
	if(read(fd,fatbuf,512*250)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ;É¾³ýµ±Ç°Ä¿Â¼ÏÂµÄÎÄ¼þ
*/
int fd_df(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*É¨Ãèµ±Ç°Ä¿Â¼²éÕÒÎÄ¼þ*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	/*Çå³ýfat±íÏî*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}

	ClearFatCluster( seed );

	/*Çå³ýÄ¿Â¼±íÏî*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");  


	//if(lseek(fd,ret-0x40,SEEK_SET)<0)
	//	perror("lseek fd_df failed");
	//if(write(fd,&c,1)<0)
	//	perror("write failed");

	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}


/*
*²ÎÊý£ºfilename£¬ÀàÐÍ£ºchar£¬´´½¨ÎÄ¼þµÄÃû³Æ
size£¬    ÀàÐÍ£ºint£¬ÎÄ¼þµÄ´óÐ¡
*·µ»ØÖµ£º1£¬³É¹¦£»-1£¬Ê§°Ü
*¹¦ÄÜ£ºÔÚµ±Ç°Ä¿Â¼ÏÂ´´½¨ÎÄ¼þ
*/
int fd_cf(char *filename,char *Str)
{

	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	int size=strlen(Str);
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//É¨Ãè¸ùÄ¿Â¼£¬ÊÇ·ñÒÑ´æÔÚ¸ÃÎÄ¼þÃû
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
		/*²éÑ¯fat±í£¬ÕÒµ½¿Õ°×´Ø£¬±£´æÔÚclusterno[]ÖÐ*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}

		/*ÔÚfat±íÖÐÐ´ÈëÏÂÒ»´ØÐÅÏ¢*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*×îºóÒ»´ØÐ´Èë0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		for (i=0;i<clustersize;i++){
			cluster_addr = DATA_OFFSET + (clusterno[i]-2) * CLUSTER_SIZE;
			if ((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			if ((ret=write(fd,Str,CLUSTER_SIZE))<0)
				perror("write entry failed");
			Str +=CLUSTER_SIZE;
		}

		if(curdir==NULL)  /*Íù¸ùÄ¿Â¼ÏÂÐ´ÎÄ¼þ*/
		{ 

			if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}


				/*ÕÒ³ö¿ÕÄ¿Â¼Ïî»òÒÑÉ¾³ýµÄÄ¿Â¼Ïî*/ 
				else
				{       
					offset = offset-abs(ret);     
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;

					/*Ð´µÚÒ»´ØµÄÖµ*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*Ð´ÎÄ¼þµÄ´óÐ¡*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");




					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
		}
		else 
		{
			cluster_addr = (curdir->FirstCluster -2 )*CLUSTER_SIZE + DATA_OFFSET;
			if((ret= lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;
			while(offset < cluster_addr + CLUSTER_SIZE)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);      
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;

					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");




					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
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
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	int size=0;
	char name[12];
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
			break;
		else if (strcmp(input, "ls") == 0)
			fd_ls();
		else if(strcmp(input, "cd") == 0)
		{
			scanf("%s", name);
			fd_cd(name);
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			fd_df(name);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			scanf("%s", input);
			size = atoi(input);
			fd_cf(name,input);
		}
		else
			do_usage();
	}	

	return 0;
}



