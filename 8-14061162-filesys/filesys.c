#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"
#include<time.h>


#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

/*
*功能：打印启动项记录
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

/*日期*/
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

/*时间*/
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


void setTime(unsigned char *info1,
			 unsigned char *info2)
{
	time_t rawtime;
	struct tm *timeinfo;
	unsigned short hour,min,sec,t;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	hour = timeinfo->tm_hour;
	min = timeinfo->tm_min;
	sec = timeinfo->tm_sec;
	t = ((hour)<<11)|((min)<<5)|(sec);

	*info1 = (char)t&0x00ff;
	*info2 = (char)((t&0xff00)>>8);
}

void setDate(unsigned char *info1,
			 unsigned char *info2)
{
	time_t rawtime;
	struct tm *timeinfo;
	unsigned short year,mon,day,date;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	year = timeinfo->tm_year-80;
	mon = timeinfo->tm_mon+1;
	day = timeinfo->tm_mday;
	date = ((year)<<9)|((mon)<<5)|(day);

	*info1 = (char)date&0x00ff;
	*info2 = (char)((date&0xff00)>>8);
}

/*
*文件名格式化，便于比较
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


char* RealFilename(char* shortName,int subdir)
{
	int i,j=0;
	char* ret;
//	printf("shortName:%s\n",shortName);
	if(!subdir){
		ret = (char *)malloc(13*sizeof(char));
		if(strlen(shortName)<=8){
			free(ret);
			return shortName;
		}
		for(i=7;shortName[i]==' ';i--);
		j = i;
		while(j>=0){
			ret[j] = shortName[j];
			j--;
		}
		ret[++i] = '.';
		for(j=8;j<=strlen(shortName);j++){
			ret[++i] = shortName[j];
		}
		return ret;
	}
	else{
		return shortName;
	}
	
}

/*把char*文件名转变成8.3格式*/
void formatName(char* filename,unsigned char* c,int mode)
{
	int i,j;
//printf("%s\n",filename);
	if(mode==0){
		for(i=0;i<strlen(filename);i++)
		{
			if(filename[i]=='.'){
				break;
			}
			//c[i]=toupper(filename[i]);
		}			
		if(i<strlen(filename)){
			for(j=0;j<i;j++)
				c[j] = toupper(filename[j]);
			for(;j<8;j++)
				c[j] = 0x20;
			for(j=0;(j+i+1)<strlen(filename);j++)
				c[j+8] = toupper(filename[j+i+1]);
			for(;(j+8)<11;j++)
				c[j+8] = 0x20;
		}
		else{
			if(strlen(filename)>8){
				perror("file name too long");
				return ;
			}
			for(i=0;i<strlen(filename);i++)
				c[i] = toupper(filename[i]);
			for(;i<11;i++)
				c[i] = 0x20;
		}
	}
	else{
		if(strlen(filename)>11){
			perror("directory name too long");
			return ;
		}
		for(i=0;i<strlen(filename);i++)
			c[i] = toupper(filename[i]);
		for(;i<11;i++)
			c[i] = 0x20;
	}
}


/*
*参数：prev，类型：unsigned char
*返回值：下一簇
*在fat表中获得下一簇的位置
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
*参数：cluster，类型：unsigned short
*返回值：void
*功能：清除fat表中的簇信息
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*将改变的fat表值写回fat表
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
*读fat表的信息，存入fatbuf[]中
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
*清除簇中内容
*/
int ClearCluster(int index)
{
	int cluster_addr = DATA_OFFSET + (index - 2) * CLUSTER_SIZE;
	unsigned char buf[DIR_ENTRY_SIZE];
	int ret, offset = 0;
	memset(buf, 0, sizeof(char) * DIR_ENTRY_SIZE);

	if((ret = lseek(fd, cluster_addr, SEEK_SET)) < 0)
	{
		perror("lseek clusert_addr faliled");
		return -1;
	}

	offset = cluster_addr;
	while(offset < cluster_addr + CLUSTER_SIZE)
	{
		if(write(fd, buf, DIR_ENTRY_SIZE) < 0)
		{
			perror("write failed");
			return -1;
		}
		offset += DIR_ENTRY_SIZE;
	}

	return 1;
}



/*参数：entry，类型：struct Entry*
*返回值：成功，则返回偏移值；失败：返回负值
*功能：从根目录或文件簇中得到文件表项
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i;
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*读一个目录表项，即32字节*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;

	if(buf[0]==0xe5 || buf[0]== 0x00)   //0xe5:未使用（被删除） //0x00:目录结尾
		return -1*count;
	else
	{
		/*长文件名，忽略掉*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}

		/*命名格式化，主义结尾的'\0'*/
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
*参数：entryname 类型：char
：pentry    类型：struct Entry*
：mode      类型：int，mode=1，为目录表项；mode=0，为文件
*返回值：偏移值大于0，则成功；-1，则失败
*功能：搜索当前目录，查找文件或目录项
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*扫描根目录*/
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

	/*扫描子目录*/
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
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls()
{
	int ret, offset,cluster_addr;
	unsigned short index = 0xffff;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if(curdir==NULL)
		printf("Root_dir\n");
	else
		printf("%s_dir\n",(RealFilename(curdir->short_name,curdir->subdir)));
	printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*显示根目录区*/
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);
//printf("ret:%d\n",ret);
			offset += abs(ret);
			if(ret > 0)
			{
				printf("%12s\t"
					"%d:%d:%d\t"
					"%d:%d:%d   \t"
					"%d\t"
					"%d\t\t"
					"%s\n",
					RealFilename(entry.short_name,entry.subdir),
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
		index = curdir->FirstCluster;

		do{
//printf("subcur\n");
//printf("index :%d\n",index);
			cluster_addr = DATA_OFFSET + (index-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
//printf("name:%s\n",entry.short_name);
				if(ret > 0 && strcmp(entry.short_name,".")!=0 && strcmp(entry.short_name,"..")!=0)
				{
					printf("%12s\t"
						"%d:%d:%d\t"
						"%d:%d:%d   \t"
						"%d\t"
						"%d\t\t"
						"%s\n",
						RealFilename(entry.short_name,entry.subdir),
						entry.year,entry.month,entry.day,
						entry.hour,entry.min,entry.sec,
						entry.FirstCluster,
						entry.size,
						(entry.subdir) ? "dir":"file");
				}
			}
			index = GetFatCluster(index);
		}while(index!=0xffff);
	}
	return 0;
} 

/*
*检查目录是否为空
*为空返回 0,非空返回 1
*/
int CheckDir(struct Entry *pentry)
{
	int ret,offset,cluster_addr;
    struct Entry entry;
	unsigned short index = 0xffff;
	//int index;
	
    unsigned char buf[DIR_ENTRY_SIZE];
	index = pentry->FirstCluster;

	do{

		cluster_addr = DATA_OFFSET+(index - 2) * CLUSTER_SIZE;

		if ((ret = lseek(fd,cluster_addr,SEEK_SET)) < 0)
			perror("lseek cluster_addr failed");
		offset = cluster_addr;

		while (offset < cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry);
			offset += abs(ret);
			if (ret > 0 && strcmp(entry.short_name,".")!=0 && strcmp(entry.short_name,"..")!=0){
				return 1;
			}
		}
	
		index = GetFatCluster(index);
	}while( index != 0xffff);
	return 0;
}

/*
*删除目录
*/
int ClearDir(struct Entry *pentry)
{
	int ret,offset,cluster_addr;
    struct Entry entry;
	unsigned char c;
	unsigned short seed,next;
	unsigned short index = 0xffff;

    unsigned char buf[DIR_ENTRY_SIZE];
    unsigned char attribute;
	
	index = pentry->FirstCluster;
	do{
		cluster_addr = DATA_OFFSET+(index - 2) * CLUSTER_SIZE;

		if ((ret = lseek(fd,cluster_addr,SEEK_SET)) < 0)
			perror("lseek cluster_addr failed");
		offset = cluster_addr;

		while (offset < cluster_addr +CLUSTER_SIZE){
			
			ret = GetEntry(&entry);
			offset += abs(ret);

			if (ret > 0){

				if(entry.subdir == 1 && CheckDir(&entry) == 1)
				{

					if(strcmp(entry.short_name,".")!=0 && strcmp(entry.short_name,"..")!=0)
   						ClearDir(&entry);
				}
//printf("not empty\n");
				/*清空Fat*/
				if(strcmp(entry.short_name,".")!=0 && strcmp(entry.short_name,"..")!=0)
				{
					seed = entry.FirstCluster;
					while((next = GetFatCluster(seed)) != 0xffff)
					{

						ClearFatCluster(seed);
						seed = next;
					}
					ClearFatCluster(seed);

				}
	
				/*清空目录项*/
				c = 0xe5;
				if(ret>=32 && lseek(fd,offset-0x20,SEEK_SET) < 0)
					perror("lseek uf_ClearDir1 failed");
				if(ret>=32 && write(fd,&c,1) <  0)
					perror("write failed");

				if(ret>=64 && lseek(fd,offset-0x40+11,SEEK_SET)<0)
					perror("lseek fd_ClearDir2 failed");
				if(ret>=64 && read(fd,&attribute,1)<0)
					perror("read failed");
				if(ret>=64 && attribute==0x0f)	
					if(lseek(fd,offset-0x40,SEEK_SET)<0)
						perror("lseek fd_ClearDir3 failed");
					if(write(fd,&c,1)<0)
						perror("write failed");

				if (WriteFat()<0)
			   		 exit(1);
				if(lseek(fd,offset,SEEK_SET) < 0)
					perror("lseek fd_ClearDir4 failed");
			}
		}
	
		index = GetFatCluster(index);
	}while(index != 0xffff);
	
	return 1;
}

int fd_dd(char *dirname,int flag)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;
	char mark[10];
	unsigned char attribute;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	
	ret = ScanEntry(dirname,pentry,1);
	if(ret<0){
		perror("no such dir");
		free(pentry);
		return -1;
	}
	
	/*目录为空*/
	if(CheckDir(pentry)==0){		
		ClearDir(pentry);
		seed = pentry->FirstCluster;

		while((next = GetFatCluster(seed)) != 0xffff)
		{
			ClearFatCluster(seed);
			seed = next;
		}

		ClearFatCluster(seed);
		c = 0xe5;
		
		if(lseek(fd,ret-0x20,SEEK_SET) < 0)
			perror("lseek fd_dd failed");
		if(write(fd,&c,1) <  0)
			perror("write failed");
			
		if(lseek(fd,ret-0x40+11,SEEK_SET)<0)
			perror("lseek fd_dd failed");
		if(read(fd,&attribute,1)<0)
			perror("read failed");
		if(attribute==0x0f)	
			if(lseek(fd,ret-0x40,SEEK_SET)<0)
				perror("lseek fd_dd failed");
			if(write(fd,&c,1)<0)
				perror("write failed");

		free(pentry);
		if (WriteFat()<0)
	   		 exit(1);
		return 1;
	}
	
	if(flag==0){
		printf("warning: this directory is not empty.Do you want to continue?(Y/N)\n");
		scanf("%s",mark);
		if(!(strcmp(mark,"y")==0||strcmp(mark,"Y")==0))
			return 1;
	}
	ClearDir(pentry);
//printf("clear over\n");
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed)) != 0xffff)
	{
		ClearFatCluster(seed);
		seed = next;
	}
	ClearFatCluster(seed);
	c = 0xe5;

	if(lseek(fd,ret-0x20,SEEK_SET) < 0)
		perror("lseek fd_dd failed");
	if(write(fd,&c,1) <  0)
		perror("write failed");

	if(lseek(fd,ret-0x40+11,SEEK_SET)<0)
		perror("lseek fd_dd failed");
	if(read(fd,&attribute,1)<0)
		perror("read failed");
	if(attribute==0x0f)	
		if(lseek(fd,ret-0x40,SEEK_SET)<0)
			perror("lseek fd_dd failed");
		if(write(fd,&c,1)<0)
			perror("write failed");	
	
	free(pentry);
	if (WriteFat()<0)
   		 exit(1);

    return 1;
	
}

	int fd_rm(char *name,int flag)
	{
		int ret;
		struct Entry *pentry;

		pentry = (struct Entry*)malloc(sizeof(struct Entry));
		if(flag==1){
			ret = ScanEntry(name,pentry,1);
			if(ret>0){
				fd_dd(name,1);
				return 1;
			}
			else{
				free(pentry);
				perror("no such directory");
				return -1;
			}
		}
		else{
	//printf("name:%s\n",name);
			ret = ScanEntry(name,pentry,0);
			if(ret>0){
				fd_df(name);
				return 1;
			}
			ret = ScanEntry(name,pentry,1);
			if(ret>0){
				fd_dd(name,0);
				return 1;
			}
			perror("no such file or directory");
			free(pentry);
			return -1;
		}
	
	
	}

/*
*参数：dir，类型：char
*返回值：1，成功；-1，失败
*功能：改变目录到父目录或子目录
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
	/*返回上一级目录*/
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
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能;删除当前目录下的文件
*/
int fd_df(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;
	unsigned char attribute;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	/*清除fat表项*/
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed))!=0xffff)
	{
		if(next==seed){
			perror("wrong file info");
			return -1;
		}
		ClearFatCluster(seed);
		seed = next;

	}

	ClearFatCluster( seed );

	/*清除目录表项*/
	c=0xe5;


	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");  


	if(lseek(fd,ret-0x40+11,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(read(fd,&attribute,1)<0)
		perror("read failed");
	/*if(write(fd,&c,1)<0)
		perror("write failed");*/
	if(attribute==0x0f)	
		if(lseek(fd,ret-0x40,SEEK_SET)<0)
			perror("lseek fd_df failed");
		if(write(fd,&c,1)<0)
			perror("write failed");

	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}


/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf(char *filename,int size)
{

	struct Entry *pentry;
	int ret,i=0,j=0,cluster_addr,offset;
	unsigned short cluster,clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	clustersize = (size / (CLUSTER_SIZE));

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
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

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]*2;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写文件*/
		{ 

			if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET; /*根目录区起始地址*/
			while(offset < DATA_OFFSET)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE/*目录项大小*/))<0)  /*返回读取的字节数，出错返回-1*/
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


				/*找出空目录项或已删除的目录项*/ 
				else
				{       
					offset = offset-abs(ret);     
					formatName(filename,c,0);
					printf("create:");
					for(i=0;i<11;i++){
						printf("%c",c[i]);
					}
					printf("\n");
					/*for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';*/

					c[11] = 0x01;

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					/*写文件时间*/
					//time
					setTime(&c[22],&c[23]);
					//date 
					setDate(&c[24],&c[25]);

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
					formatName(filename,c,0);     
					/*for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}
					for(;i<=10;i++)
						c[i]=' ';*/

					c[11] = 0x01;

					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					/*写文件时间*/
					//time
					setTime(&c[22],&c[23]);
					//date 
					setDate(&c[24],&c[25]);

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

int fd_cf_cus(char *name, char *input)
{
	int size,ret,writed;
	unsigned short cluster;
	struct Entry *pentry;
	//创建对应的文件分配表和目录项
	size = strlen(input)+1;
	if(fd_cf(name,size)<0)
		return -1;
	//向文件里写入数据
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	ret = ScanEntry(name,pentry,0);
	cluster = pentry->FirstCluster;
	/*将fd定位到第一簇地址*/
	if((ret= lseek(fd,DATA_OFFSET+ (cluster -2)*CLUSTER_SIZE,SEEK_SET))<0)
		perror("lseek ROOTDIR_OFFSET failed");
	if(write(fd,&input,CLUSTER_SIZE)<0)
		perror("write failed");
	for(writed = CLUSTER_SIZE;(cluster=GetFatCluster(cluster))!=0XFFFF;writed+=CLUSTER_SIZE)
	{
		if((ret= lseek(fd,DATA_OFFSET+ (cluster -2)*CLUSTER_SIZE,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");
		if(write(fd,&input+writed,CLUSTER_SIZE)<0)
			perror("write failed");
	}
	return 1;
}

int fd_mkdir(char *filename)
{
/*
a) 找到空白簇，更新fat表，添加目录项
b) 此目录中自动添加此目录的目录项和其父目录的目录项，并将文件名分别设置为”.”和”..”，以便实现多级目录
*/
	struct Entry *pentry;
	int ret,i=0,j=0,cluster_addr,offset;
	unsigned short cluster,clusterno=-1;
	unsigned char c[DIR_ENTRY_SIZE]={0};
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	int size = 0;

	//扫描目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,1);
	if (ret<0)
	{
		for(cluster=2;cluster<1000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno = cluster;
				ClearCluster(clusterno);
				break;
			}

		}

		if(clusterno < 0)
		{
			perror("cannot find blank cluster");
			return -1;
		}

		/*最后一簇写入0xffff*/
		index = clusterno*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  
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
							perror("read dir failed");
						offset +=abs(ret);
					}
				}


				/*找出空目录项或已删除的目录项*/ 
				else
				{       
					offset = offset-abs(ret);     
					formatName(filename,c,1);

					c[11] = 0x11;

					/*写第一簇的值*/
					c[26] = (clusterno &  0x00ff);
					c[27] = ((clusterno & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);
					
					/*写文件时间*/
					//time
					setTime(&c[22],&c[23]);
					//date 
					setDate(&c[24],&c[25]);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(WriteFat()<0)
						exit(1);

					/*向目录所在簇中写入此目录和父目录的目录项*/
					formatName(".",c,1);
					
					if(lseek(fd,DATA_OFFSET+(clusterno-2)*CLUSTER_SIZE, SEEK_SET) < 0 )
						perror("lseek fd_mkdir failed");
					if(write(fd, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");	
					
					memset(c, 0, sizeof(char) * DIR_ENTRY_SIZE);
					formatName("..",c,1);
					
					c[11] = 0x11;
					c[26] = (0 & 0x00ff);
					c[27] = ((0 & 0xff00) >> 8);
					
					if(lseek(fd,DATA_OFFSET+(clusterno-2)*CLUSTER_SIZE+0x20, SEEK_SET) < 0 )
						perror("lseek fd_mkdir failed");
					if(write(fd, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					

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
							perror("read dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);  
					formatName(filename,c,1);    

					c[11] = 0x11;

					c[26] = (clusterno &  0x00ff);
					c[27] = ((clusterno & 0xff00)>>8);

					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);
					
					/*写文件时间*/
					//time
					setTime(&c[22],&c[23]);
					//date 
					setDate(&c[24],&c[25]);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					free(pentry);
					if(WriteFat()<0)
						exit(1);

					/*向目录所在簇中写入此目录和父目录的目录项*/
					formatName(".",c,1);
					
					if(lseek(fd,DATA_OFFSET+(clusterno-2)*CLUSTER_SIZE, SEEK_SET) < 0 )
						perror("lseek fd_mkdir failed");
					if(write(fd, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");	
					
					memset(c, 0, sizeof(char) * DIR_ENTRY_SIZE);
					formatName("..",c,1);
					
					c[11] = 0x11;
					c[26] = (curdir->FirstCluster & 0x00ff);
					c[27] = ((curdir->FirstCluster & 0xff00) >> 8);
					
					if(lseek(fd,DATA_OFFSET+(clusterno-2)*CLUSTER_SIZE+0x20, SEEK_SET) < 0 )
						perror("lseek fd_mkdir failed");
					if(write(fd, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
						

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
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> {[-e <size>]<content>}\tcreate a file\n\tdf <file>\t\tdelete a file\n\tmkdir <file>\t\tcreat a new folder\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[10];
	int size=0;
	char name[12];
	char directoryName[12];
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
			if(strcmp(input,"-e")==0)
			{
				scanf("%s",input);
				size = atoi(input);
				fd_cf(name,size);
			}
			else
				fd_cf_cus(name,input);
		}
		else if(strcmp(input,"mkdir") == 0)
		{
			scanf("%s",directoryName);
			fd_mkdir(directoryName);
		}
		else if(strcmp(input , "rm")==0)
		{
			scanf("%s",name);
			if(strcmp(name , "-r") == 0){
				scanf("%s",input);
				fd_rm(input,1);
			}
			else
				fd_rm(name,0);
		}
		else
			do_usage();
	}	

	return 0;
}



