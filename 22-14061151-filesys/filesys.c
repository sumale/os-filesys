#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"
#include<time.h>//jsz add


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

	if(buf[0]==0xe5 || buf[0]== 0x00)
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
 * 参数：mode，类型：int
 * 返回值：，成功；-1，失败
 * 功能：显示所要查看目录下的内容。
 * 说明：mode值1为时，查看当前目录信息；mode值0为时，查看所有的非根目录信息
 */
int fd_ls(int mode)
{
    int ret,offset,cluster_addr;
    int i;
    struct Entry entry;
    unsigned char buf[DIR_ENTRY_SIZE];

    if((ret=read(fd,buf,DIR_ENTRY_SIZE))<0)
        perror("read entry failed");

    if(mode==1)
    {
        printf("All_Non-Root_dir\n");
        printf("\tname\t\tdate\t\ttime\t\tcluster\tsize\tattr\n");

        for(i=0;i<100;i++)//数据区的大小
        //for(i=0;i<379;i++)//数据区的大小
        {
            cluster_addr=DATA_OFFSET+i*CLUSTER_SIZE;
            if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
                perror("lseek cluster_addr failed");

            offset=cluster_addr;

            /*只读一簇的内容*/
            while(offset<cluster_addr+CLUSTER_SIZE)
            {
                ret=GetEntry(&entry);
                offset+=abs(ret);

                if(ret>0)
                {
                    printf("%16s\t"
                        "%d-%d-%d\t"
                        "%d:%d:%d  \t"
                        "%d\t"
                        "%d\t"
                        "%s\n",
                        entry.short_name,
                        entry.year,entry.month,entry.day,
                        entry.hour,entry.min,entry.sec,
                        entry.FirstCluster,
                        entry.size,
                        (entry.subdir)?"dir":"file");
                }
            }
        }
        return 1;
    }

    if(curdir==NULL)
        printf("Root_dir\n");
    else
        printf("%s_dir\n",curdir->short_name);
    printf("\tname\t\tdate\t\ttime\t\tcluster\tsize\tattr\n");

    if(curdir==NULL)    //显示根目录区
    {
        /*将fd定位到根目录区的起始地址*/
        if((ret=lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
            perror("lseek ROOTDIR_OFFSET failed");

        offset=ROOTDIR_OFFSET;

        /*从根目录区开始遍历，直到数据区起始地址*/
        while(offset<DATA_OFFSET)
        {
            ret=GetEntry(&entry);
            offset+=abs(ret);
            if(ret>0)
            {
                printf("%16s\t"
                    "%d-%d-%d\t"
                    "%d:%d:%d  \t"
                    "%d\t"
                    "%d\t"
                    "%s\n",
                    entry.short_name,
                    entry.year,entry.month,entry.day,
                    entry.hour,entry.min,entry.sec,
                    entry.FirstCluster,
                    entry.size,
                    (entry.subdir)?"dir":"file");
            }
        }
    }
    else                //显示子目录
    {
        cluster_addr=DATA_OFFSET+(curdir->FirstCluster-2)*CLUSTER_SIZE;
        if((ret=lseek(fd,cluster_addr,SEEK_SET))<0)
            perror("lseek cluster_addr failed");

        offset=cluster_addr;

        /*只读一簇的内容*/
        while(offset<cluster_addr+CLUSTER_SIZE)
        {
            ret=GetEntry(&entry);
            offset+=abs(ret);

            if(ret>0)
            {
                printf("%16s\t"
                    "%d-%d-%d\t"
                    "%d:%d:%d  \t"
                    "%d\t"
                    "%d\t"
                    "%s\n",
                    entry.short_name,
                    entry.year,entry.month,entry.day,
                    entry.hour,entry.min,entry.sec,
                    entry.FirstCluster,
                    entry.size,
                    (entry.subdir)?"dir":"file");
            }
        }
    }

    return 0;
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
int WriteFat()//jsz note 将fatbuf[]整体写回
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
*读fat表的信息，存入fatbuf[]中
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


	if(lseek(fd,ret-0x20+FAT_TWO_OFFSET-FAT_ONE_OFFSET,SEEK_SET)<0)//jsz change ret-0x40
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
int ud_cf(char *filename,char *input)//lyc modified
{
	int size=strlen(input)+1;
	struct Entry *pentry;//jsz note 指向目录项的指针
	int ret,i=0,cluster_addr,offset;//jsz note cluster_addr是簇地址
	unsigned short cluster,clusterno[100];//jsz note clusterno是文件将占的簇号
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;//jsz note index为簇号对应的fat表中的位置（因为fat表有两张，所）
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));//jsz note 为目录项分配空间


	clustersize = (size / (CLUSTER_SIZE));//jsz note 计算文件所占的簇的个数

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	//保存创建目录的时间
	time_t timep;
    struct tm *p;
    time(&timep);
    int createtime;
    int createdate;
    p =localtime(&timep);//此函数获得的tm结构体的时间，是已经进行过时区转化为本地时间
    createtime=p->tm_hour*2048+p->tm_min*32+p->tm_sec/2;
    createdate=(p->tm_year+1900-1980)*512+(p->tm_mon+1)*32+p->tm_mday;
    
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)//jsz note FAT表的信息已经读入到fatbuf[]数组中了
			{
				clusterno[i] = cluster;//jsz note 将找到的空的簇号记录到clusterno[]中

				i++;
				if(i==clustersize)//jsz note 如果已经达到了文件所要占的簇的个数
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
		fatbuf[index] = 0xff;//jsz note fat表的每一项占两个16进制位，8个二进制位
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写文件*/
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
				else//找出空目录项或已删除的目录项
				{       
					offset = offset-abs(ret);     
					for(i=0;i<=strlen(filename);i++)
					{
						c[i]=toupper(filename[i]);
					}			
					for(;i<=10;i++)
						c[i]=' ';

					c[11] = 0x01;

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);
					//此处记录时间
					c[0x16]=(createtime&0x00ff);
					c[0x17]=((createtime&0xff00)>>8);
					c[0x18]=(createdate&0x00ff);
					c[0x19]=((createdate&0xff00)>>8);
					/*写文件的大小*/
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
					for(int j=0;j<clustersize;j++)//真实写入
					{
						if(lseek(fd,clusterno[j]*CLUSTER_SIZE,SEEK_SET)<0)
						{
							perror("lseek failed");
							exit(1);
						}
						if(write(fd,input+CLUSTER_SIZE*j,(j==clustersize-1)?(size-CLUSTER_SIZE*j):CLUSTER_SIZE)<0)
						{
							perror("write failed");
							exit(1);
						}
					}
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
					//记录簇值
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);
					//此处记录时间
					c[0x16]=(createtime&0x00ff);
					c[0x17]=((createtime&0xff00)>>8);
					c[0x18]=(createdate&0x00ff);
					c[0x19]=((createdate&0xff00)>>8);
					//记录大小
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
					for(int j=0;j<clustersize;j++)//真实写入
					{
						if(lseek(fd,clusterno[j]*CLUSTER_SIZE,SEEK_SET)<0)
						{
							perror("lseek failed");
							exit(1);
						}
						if(write(fd,input+CLUSTER_SIZE*j,(j==clustersize-1)?(size-CLUSTER_SIZE*j):CLUSTER_SIZE)<0)
						{
							perror("write failed");
							exit(1);
						}
					}
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
int ud_rf(char *filename)
{
	struct Entry *pentry;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	int ret=ScanEntry(filename,pentry,0);
	int cluster;
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}
	else
	{
		cluster=pentry->FirstCluster;
		int index;
		char buf[CLUSTER_SIZE];
		while(cluster!=0xffff)
		{
			index=cluster*2;
			if(lseek(fd,CLUSTER_SIZE*cluster,SEEK_SET)<0)
			{
				perror("lseek failed");
				exit(1);
			}
			if(read(fd,buf,CLUSTER_SIZE)<0)
			{
				perror("read failed");
				exit(1);
			}
			printf("%s",buf);
			cluster=fatbuf[index+1]<<8|fatbuf[index];
		}
		printf("\n");
	}
}
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
	//根据dir查找目录并移动，’/‘开头为绝对路径，其他开头为相对路径
	int i=0;
	int pos;
	int length=strlen(dir);
	if(dir[i]=='/'){//绝对路径
		pos=1;
		struct Entry *saveCurDir=curdir;
		struct Entry *saveFatherDir[10];
		int savedirno=dirno;
		for(int j=0;j<=dirno;j++){
			saveFatherDir[j]=fatherdir[j];
		}
		curdir=NULL;
		for(i=1;i<=length;i++){
			if(dir[i]=='/'||(dir[i]=='\0'&&dir[i-1]!='\0')){
				dir[i]='\0';
				ret = ScanEntry(dir+pos,pentry,1);
				if(ret < 0)
				{
					printf("no such dir\n");
					curdir=saveCurDir;
					dirno=savedirno;
					for(int j=0;j<=dirno;j++){
						fatherdir[j]=saveFatherDir[j];
					}
					free(pentry);
					return -1;
				}
				dirno ++;
				fatherdir[dirno] = curdir;
				curdir = pentry;
				pos=i+1;
			}
		}
		return 1;
	}
	else{//相对路径
		struct Entry *saveCurDir=curdir;//保存现场，失败时务必复原
		int savedirno=dirno;
		pos=0;
		for(i=0;i<=length;i++){
			if(dir[i]=='/'||(dir[i]=='\0'&&dir[i-1]!='\0')){
				dir[i]='\0';
				ret = ScanEntry(dir+pos,pentry,1);
				if(ret < 0)
				{
					printf("no such dir\n");
					curdir=saveCurDir;
					dirno=savedirno;
					free(pentry);
					return -1;
				}
				dirno ++;
				fatherdir[dirno] = curdir;
				curdir = pentry;
				pos=i+1;
			}
		}
		return 1;
	}
}

/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf(char *filename,int size)
{

	struct Entry *pentry;//jsz note 指向目录项的指针
	int ret,i=0,cluster_addr,offset;//jsz note cluster_addr是簇地址
	unsigned short cluster,clusterno[100];//jsz note clusterno是文件将占的簇号
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;//jsz note index为簇号对应的fat表中的位置（因为fat表有两张，所）
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));//jsz note 为目录项分配空间


	clustersize = (size / (CLUSTER_SIZE));//jsz note 计算文件所占的簇的个数

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	
	//保存创建目录的时间
	time_t timep;
    struct tm *p;
    time(&timep);
    int createtime;
    int createdate;
    p =localtime(&timep);//此函数获得的tm结构体的时间，是已经进行过时区转化为本地时间
    createtime=p->tm_hour*2048+p->tm_min*32+p->tm_sec/2;
    createdate=(p->tm_year+1900-1980)*512+(p->tm_mon+1)*32+p->tm_mday;
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<1000;cluster++)
		{
			index = cluster *2;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)//jsz note FAT表的信息已经读入到fatbuf[]数组中了
			{
				clusterno[i] = cluster;//jsz note 将找到的空的簇号记录到clusterno[]中

				i++;
				if(i==clustersize)//jsz note 如果已经达到了文件所要占的簇的个数
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
		fatbuf[index] = 0xff;//jsz note fat表的每一项占两个16进制位，8个二进制位
		fatbuf[index+1] = 0xff;

		if(curdir==NULL)  /*往根目录下写文件*/
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


				/*找出空目录项或已删除的目录项*/
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

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					//此处记录时间
					c[0x16]=(createtime&0x00ff);
					c[0x17]=((createtime&0xff00)>>8);
					c[0x18]=(createdate&0x00ff);
					c[0x19]=((createdate&0xff00)>>8);

					/*写文件的大小*/
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
					
					//此处记录时间
					c[0x16]=(createtime&0x00ff);
					c[0x17]=((createtime&0xff00)>>8);
					c[0x18]=(createdate&0x00ff);
					c[0x19]=((createdate&0xff00)>>8);
					
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
/*jsz add
*参数：dirname，类型：char，创建目录的名称
*返回值：1，成功；-1，失败
*功能：在当前目录下创建目录
*/
int fd_mkdir(char *dirname)
{
	struct Entry *pentry;//jsz note 指向目录项的指针
	int ret,i=0,cluster_addr,offset;//jsz note cluster_addr簇地址
	unsigned short cluster,clusterno=0;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));//jsz note 为目录项分配空间

	//保存创建目录的时间
	time_t timep;
    struct tm *p;
    time(&timep);
    int createtime;
    int createdate;
    p =localtime(&timep); //此函数获得的tm结构体的时间，是已经进行过时区转化为本地时间
    createtime=p->tm_hour*2048+p->tm_min*32+p->tm_sec/2;
    createdate=(p->tm_year+1900-1980)*512+(p->tm_mon+1)*32+p->tm_mday;
	//...
	//printf("#debug# start fd_mkdir\n");
	ret=ScanEntry(dirname,pentry,1);//查找根目录，是否已存在
	if(ret>=0){
		printf("This directory is exist\n");
		if(pentry!=NULL){
			free(pentry);
			pentry=NULL;
		}
		return -1;
	}
	/*查询fat表，找到空白簇，保存在clusterno中*/
	for(cluster=2;cluster<1000;cluster++)
	{
		index = cluster *2;
		if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)//jsz note FAT表的信息已经读入到fatbuf[]数组中了
		{
			clusterno = cluster;//jsz note 将找到的空的簇号记录到clusterno中
			break;
		}
	}
	if(clusterno==0){
		printf("!!!clusters are full\n");
		return -1;
	}
	//找到了空的簇
	index = clusterno*2;
	//写入FFFF
	fatbuf[index] = 0xff;
	fatbuf[index+1] = 0xff;
	//在当前目录下添加该子目录的目录项
	if(curdir==NULL){//将目录项写入根目录
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
			/*找出空目录项或已删除的目录项*/
			else
			{      
				offset = offset-abs(ret);    
				for(i=0;i<=strlen(dirname);i++)
				{
					c[i]=toupper(dirname[i]);
				}			
				for(;i<=10;i++)
					c[i]=' ';
				c[11] = 0x10;//jsz note 注意这里修改为0x10代表目录，因为第四位代表子目录，二进制的10000＝0x10
				//此处记录时间
				c[0x16]=(createtime&0x00ff);
				c[0x17]=((createtime&0xff00)>>8);
				c[0x18]=(createdate&0x00ff);
				c[0x19]=((createdate&0xff00)>>8);
				//...
				/*写第一簇的值*/
				c[26] = (clusterno &  0x00ff);
				c[27] = ((clusterno & 0xff00)>>8);
				/*写目录的大小*/
				int size=0x00000000;//jsz note 目录的大小为0
				c[28] = (size &  0x000000ff);
				c[29] = ((size & 0x0000ff00)>>8);
				c[30] = ((size& 0x00ff0000)>>16);
				c[31] = ((size& 0xff000000)>>24);
				if(lseek(fd,offset,SEEK_SET)<0)
					perror("lseek fd_mkdir failed");
				if(write(fd,&c,DIR_ENTRY_SIZE)<0)//将数组c的内容写入data中
					perror("write failed");
				if(pentry!=NULL){
					free(pentry);
					pentry=NULL;
				}
				if(WriteFat()<0)//将修改过的FAT表写回
					exit(1);
				return 1;
			}
		}
	}
	else{//往当前目录中写
		//printf("#debug#write to curdir\n");
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
				for(i=0;i<=strlen(dirname);i++)
				{
					c[i]=toupper(dirname[i]);
				}
				for(;i<=10;i++)
					c[i]=' ';
				c[11] = 0x10;//文件属性是子目录
				//此处记录时间
				c[0x16]=(createtime&0x00ff);
				c[0x17]=((createtime&0xff00)>>8);
				c[0x18]=(createdate&0x00ff);
				c[0x19]=((createdate&0xff00)>>8);
				//...
				c[26] = (clusterno &  0x00ff);
				c[27] = ((clusterno & 0xff00)>>8);
				int size=0x00000000;//目录的大小为0
				c[28] = (size &  0x000000ff);
				c[29] = ((size & 0x0000ff00)>>8);
				c[30] = ((size& 0x00ff0000)>>16);
				c[31] = ((size& 0xff000000)>>24);
				if(lseek(fd,offset,SEEK_SET)<0)
					perror("lseek fd_cf failed");
				if(write(fd,&c,DIR_ENTRY_SIZE)<0)
					perror("write failed");
				if(pentry!=NULL){
					free(pentry);
					pentry=NULL;
				}
				if(WriteFat()<0)
					exit(1);
				return 1;
			}
		}
	}
}
/*jsz add
mode=1 搜索目录	mode=0 搜索文件
*功能：搜索当前目录，查找文件或目录项,返回目录项的位置
*/
int searchfileindir(struct Entry *fatherentry,char* entryname,int mode)
{
	int ret,offset,i;
	int cluster_addr;
	char uppername[80];
	struct Entry *pentry;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	//如果fatherentry为null，则搜索根目录
	if(fatherentry ==NULL)  
	{
		int ret;
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
	//否则搜索faherentry目录
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	
	cluster_addr = DATA_OFFSET + (fatherentry->FirstCluster -2)*CLUSTER_SIZE;
	if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
		perror("lseek cluster_addr failed");
	offset= cluster_addr;

	while(offset<cluster_addr + CLUSTER_SIZE)
	{
		ret= GetEntry(pentry);
		offset += abs(ret);
		if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername)){
			free(pentry);
			return offset;
		}
	}
	free(pentry);
	return -1;
}
//jsz add 删除单个文件或者目录项信息
void deletefile(int ret,struct Entry *pentry)
{
	unsigned char c;
	unsigned short seed,next;
	int flag=1;
	/*清除fat表项*/
	seed = pentry->FirstCluster;
	
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;
		if(seed*2>=512*250||seed==0){//jsz add 修补核心已转储问题
			printf("break while\n");
			flag=0;
			break;
		}
	}
	ClearFatCluster( seed );
	/*清除目录表项*/
	c=0xe5;
	if(lseek(fd,ret-0x20,SEEK_SET)<0)
		perror("lseek deletefile failed1");
	if(write(fd,&c,1)<0)
		perror("write failed");  


	if(lseek(fd,ret-0x20+FAT_TWO_OFFSET-FAT_ONE_OFFSET,SEEK_SET)<0)
		perror("lseek deletefile failed2");
	if(write(fd,&c,1)<0)
		perror("write failed");
	if(WriteFat()<0)
		exit(1);
}
//jsz add 初始化删除列表
void initdeletelist()
{
	int i=0;
	for(i=0;i<512*250;i++)
		deletelist[i]=0;
	deletecount=0;
}
//jsz add 删除一个目录中的文件和目录信息
void deletedir(int ret,struct Entry *pentry)
{
	int offset,cluster_addr;
	unsigned char c;
	unsigned short seed,next;
	struct Entry *mempentry;
	mempentry=pentry;//记录下原目录的指针
	//printf("^^^^^^^^^^delete %s^^^^^^^^^^^\n",pentry->short_name);
	//开始删除子目录和子文件
	int newret;
	cluster_addr = DATA_OFFSET + (pentry->FirstCluster-2) * CLUSTER_SIZE ;
	if((newret = lseek(fd,cluster_addr,SEEK_SET))<0)
		perror("lseek cluster_addr failed");
	offset = cluster_addr;

	/*只读一簇的内容*/
	while(offset<cluster_addr +CLUSTER_SIZE)
	{
		pentry=(struct Entry*)malloc(sizeof(struct Entry));
		if((newret = lseek(fd,offset,SEEK_SET))<0)//重新定位文件指针！
			perror("lseek offset failed");
		newret = GetEntry(pentry);//jsz note newret为目录项的偏移量
		offset += abs(newret);
		
		if(newret > 0)
		{
			if(!(pentry->subdir)){//如果是文件则直接删除
				deletelist[deletecount++]=offset;//加入到删除列表中
			}
			else if(pentry->FirstCluster!=mempentry->FirstCluster){//如果是目录且不同于mempentry则递归调用deletedir
				deletedir(offset,pentry);
			}
		}		
	}
	if(pentry!=NULL){
		free(pentry);
		pentry=NULL;
	}

	deletelist[deletecount++]=ret;//加入到删除列表中
	
}
/*jsz add
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能;删除当前目录下的文件或目录
*/
int fd_rm(char *filename)
{
	struct Entry *pentry;
	int ret;
	unsigned char c;
	unsigned short seed,next;

	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,0);
	if(ret>=0){
		//找到了文件
		delbyret(ret);//删除当前目录下的文件
		printf("file %s deleted\n",filename);
		if(pentry!=NULL){
			free(pentry);
			pentry=NULL;
		}
		return 1;
	}
	/*扫描当前目录查找目录*/
	ret = ScanEntry(filename,pentry,1);
	if(ret>=0){
		//找到了目录
		int offset,cluster_addr,newret;
		unsigned char c;
		cluster_addr = DATA_OFFSET + (pentry->FirstCluster-2) * CLUSTER_SIZE ;
		if((newret = lseek(fd,cluster_addr,SEEK_SET))<0)
			perror("lseek cluster_addr failed");
		offset = cluster_addr;

		/*只读一簇的内容*/
		if(offset<cluster_addr +CLUSTER_SIZE)
		{
			if((newret = lseek(fd,offset,SEEK_SET))<0)//重新定位文件指针！
				perror("lseek offset failed");
			newret = GetEntry(pentry);//jsz note newret为目录项的偏移量
			offset += abs(newret);
			
			if(newret > 0){
				printf("#rm warning# dir %s is not empty!if you want to delete all file,please press y\n",filename);
				char cmd[10];
				scanf("%s",cmd);
				if(strcmp(cmd,"y")==0||strcmp(cmd,"Y")==0){
					//调用rmall删除全部文件
					fd_rmall(filename);
					printf("dir %s deleted\n",filename);
					if(pentry!=NULL){
						free(pentry);
						pentry=NULL;
					}
		return 1;
				}
				else{
					printf("I didn't delete any file.\n");
					if(pentry!=NULL){
						free(pentry);
						pentry=NULL;
					}
					return -1;
				}
			}
		}
		printf("dir %s deleted\n",filename);
		delbyret(ret);//删除目录项
		if(pentry!=NULL){
			free(pentry);
			pentry=NULL;
		}
		return 1;
	}
	printf("#rm warning# no such file or dir\n");
	if(pentry!=NULL){
		free(pentry);
		pentry=NULL;
	}
	return -1;
}
/*jsz add
*参数：filename，类型：char
*返回值：1，成功；-1，失败
*功能;删除当前目录下的子目录和子文件
*/
int fd_rmall(char* filename)
{
	
	struct Entry *pentry;
	int ret,offset,cluster_addr;
	unsigned char c;
	unsigned short seed,next;
	
	pentry = (struct Entry*)malloc(sizeof(struct Entry));

	/*扫描当前目录查找目录*/
	ret = ScanEntry(filename,pentry,1);
	if(ret>=0){
		//找到了目录
		
		//清空deletelist
		initdeletelist();
		deletedir(ret,pentry);
		int i;
		//将列表中的项全部清空
		printf("deleted %d files or dirs\n",deletecount);
		for(i=0;i<deletecount;i++)
			delbyret(deletelist[i]);
		
		if(pentry!=NULL){
			free(pentry);
			pentry=NULL;
		}
		return 1;
	}
	if(pentry!=NULL){
		free(pentry);
		pentry=NULL;
	}
	printf("#rm -r warning#could not find this dir or %s is a file\n",filename);
	return -1;
}
int fd_ret(char *filename)//jsz add 查询ret
{
	struct Entry *pentry;
	int result=0;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	result=ScanEntry (filename,pentry,0);
	if(result<=0)
		result=ScanEntry (filename,pentry,1);
	return result;
}
int GetEntryByRet(int ret,struct Entry *pentry)//jsz add 根据ret号读取pentry
{
	int i;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];
	if(lseek(fd,ret-DIR_ENTRY_SIZE,SEEK_SET)<0)//重新定位文件指针！
		perror("lseek offset failed");
	read(fd,buf,DIR_ENTRY_SIZE);//读取内容,存入buf
	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1;
	else
	{
		/*长文件名，忽略掉*/
		while (buf[11]== 0x0f)
		{
			if(read(fd,buf,DIR_ENTRY_SIZE)<0)
				perror("read root dir failed");
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

		return 1;
	}
}
int delbyret(int ret)//jsz add 根据ret精确删除
{
	struct Entry *pentry;
	unsigned char c;
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
	int result=-1;
	result=GetEntryByRet(ret,pentry);
	if(result>0)
		deletefile(ret,pentry);
	free(pentry);
}
void do_usage()
{
	printf("please input a command, including followings:\n\tls [-a/-all]\t\tlist all files(-a/all all the disk)\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\trm <file or dir>\tdelete a file or dir\n\trm -r <dir>\t\tdelete all contents of a dir\n\tstrcf <name> <string>\tuse an input_str initialize a file\n\trf <name>\t\tread a file\n\texit\t\t\texit this system\n");
}


int main()
{
	char input[1000];
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
		else if(strcmp(input,"ls")==0)
        {
            char c=getchar();
            if(c!='\n')
            {
		        scanf("%s",name);
		        if(strcmp(name,"-a")==0||strcmp(name,"-all")==0)
		             fd_ls(1);
		        else
		             fd_ls(0);
            }
            else
            {
            	fd_ls(0);
            }
        }
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
			fd_cf(name,size);
		}
		else if(strcmp(input,"strcf")==0)
		{
			scanf("%s",name);
			scanf("%s",input);
			ud_cf(name,input);
		}
		else if(strcmp(input,"mkdir")==0)//jsz add 创建目录函数
		{
			scanf("%s",name);
			fd_mkdir(name);
		}
		else if(strcmp(input,"rf")==0)
		{
			scanf("%s",name);
			ud_rf(name);
		}
		else if(strcmp(input,"rm")==0)
		{
			scanf("%s",name);
			if(strcmp(name,"-r")==0){
				scanf("%s",name);
				fd_rmall(name);//删除目录下的所有信息
			}
			else{
				fd_rm(name);//删除文件或目录
			}
		}
		else if(strcmp(input,"ret")==0)//jsz add 获取目录项位置
		{
			scanf("%s",name);
			printf("#command#ret:%d\n",fd_ret(name));
		}
		else if(strcmp(input,"del")==0)//jsz add 精确删除
		{
			int ret;
			scanf("%d",&ret);
			printf("#command#delete %d return %d\n",ret,delbyret(ret));
		}
		else
			do_usage();
	}	

	return 0;
}


