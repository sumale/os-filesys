

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

int fd;
struct BootDescriptor_t bdptor;
struct Entry *curdir=NULL;
int dirno=0;
struct Entry* fatherdir[50];

unsigned char *fatbuf;


int WriteContent(int startCluster, char* content, int length)
{
	// Requires: length>=0, startCluster>=2
	int start = DATA_OFFSET+(startCluster-2)*CLUSTER_SIZE;
	short cur;
	cur = startCluster;
	int temp = length;
	if(length<0||startCluster<2)
	{
		return -1;
	}

	while(temp>0)
	{
		if(lseek(fd,start,SEEK_SET)<0)
			return -1;
		if(temp < CLUSTER_SIZE)
		{
			write(fd, content, temp);
			content += temp;
		}
		else
		{
			write(fd, content, CLUSTER_SIZE);
			content += CLUSTER_SIZE;
			cur = GetFatCluster(cur);
			if(cur==0xffff) break;
			start = DATA_OFFSET+(cur-2)*CLUSTER_SIZE;
		}
		temp -= CLUSTER_SIZE;
	}
	return length;
}

char* get_cur_dir(){
	char *s;
	int le = 0, shift = 0, l;
	int i;
	if (curdir) le = strlen(curdir->short_name);

	for(i=2;i<=dirno;i++){
		if (fatherdir[i]){
			le += strlen(fatherdir[i]->short_name);
			le++;
		}

	}
	s = (char*)malloc(sizeof(char)*(le+2));
	shift=1;
	s[0]='/';
	for(i=2;i<=dirno;i++)
	if (fatherdir[i]){
		l = strlen(fatherdir[i]->short_name);
		memcpy(s+shift,fatherdir[i]->short_name,l);
		shift+=l;
		s[shift++] = '/';
	}
	if (curdir){
		l = strlen(curdir->short_name);
		memcpy(s+shift,curdir->short_name,l);
		shift+=l;
	}
	s[shift]=0;
	return s;
}

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
	if(!bdptor.LogicSectors) bdptor.LogicSectors = RevWord(buf[0x20],buf[0x21],buf[0x22],buf[0x23]);//RevByte(buf[0x13],buf[0x14]);
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);

	fatbuf=(unsigned char*)malloc(sizeof(unsigned char)*FAT_SIZE);
	//printf("ROOTDIR_OFFSET: %x\n",FAT_ONE_OFFSET);

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

	*year = ((date & MASK_YEAR)>> 9 )+1900;
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
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
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
		int current_cluster;
		for(current_cluster=curdir->FirstCluster;current_cluster!=0xffff;current_cluster=RevByte(fatbuf[current_cluster<<1],fatbuf[current_cluster<<1|1])) {
			cluster_addr = DATA_OFFSET + (current_cluster/*curdir->FirstCluster*/-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
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
int magic;
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

			if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername)) {
				magic=ret;

				return offset;
			}

		}
		return -1;
	}

	/*扫描子目录*/
	else
	{
		int current_cluster;
		for(current_cluster=curdir->FirstCluster;current_cluster!=0xffff;current_cluster=RevByte(fatbuf[current_cluster<<1],fatbuf[current_cluster<<1|1])) {
			cluster_addr = DATA_OFFSET + (current_cluster/*curdir->FirstCluster*/-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");
			offset= cluster_addr;

			while(offset<cluster_addr + CLUSTER_SIZE)
			{
				ret= GetEntry(pentry);
				offset += abs(ret);
				if(pentry->subdir == mode &&!strcmp((char*)pentry->short_name,uppername)) {
					magic=ret;
					return offset;
				}



			}
		}
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
	struct Entry *pentry,*tmp;
	int ret;
	char *pi,flag=0;

	if(*dir=='/') { // absolute path
		struct Entry* tmp=curdir;
		int pre_dirno=dirno;
		if(dir[1]=='/') {
			puts("illegal input");
			return -1;
		}
		curdir=NULL; dirno=0;
		ret=fd_cd(dir+1);
		if(ret<0) { //recover on failure
			curdir=tmp;
			dirno=pre_dirno;
			return ret;
		}
		return 1;
	}

	for(pi=dir;*pi;++pi)
		if(*pi=='/') {
			if(pi[1]=='/') {
				puts("illegal input");
				return -1;
			}
			*pi=0; flag=1;
			break;
		}

	if(!strcmp(dir,"."))
	{
		if(flag) return fd_cd(pi+1); // think carefully why this is correct
		return 1;
	}
	if(!strcmp(dir,"..") && curdir==NULL) {
		if(flag) return fd_cd(pi+1);
		return 1;
	}

	/*返回上一级目录*/
	if(!strcmp(dir,"..") && curdir!=NULL)
	{
		struct Entry *tmp=curdir;
		curdir = fatherdir[dirno];
		dirno--;
		if(flag) ret=fd_cd(pi+1);
		if(ret<0) { // recover changes on failure.
			curdir=tmp;
			++dirno;
			return ret;
		}
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
	tmp=curdir;
	curdir = pentry;

	if(flag) {
		ret=fd_cd(pi+1);
		if(ret<0) { // recover changes on failure.
			curdir=fatherdir[dirno--];
			//curdir=tmp;
			free(pentry);
			return ret;
		}
		//free(pentry);
		//free(tmp);
		return 1;
	}

	return 1;
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
	if(write(fd,fatbuf,FAT_SIZE)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,FAT_SIZE))<0)
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
	if(read(fd,fatbuf,FAT_SIZE)<0)
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
	struct Entry *pentry,*tmp;
	int ret,i,j,k;
	char *pi,*splitter;
	unsigned char c;
	unsigned short seed,next;
	int start_cluster=-1,start_offs=-1,end_addr=-1;

	/*tmp=curdir;
	for(pi=filename,splitter=0;*pi;++pi)
		if(*pi=='/') splitter=pi;
	if(splitter) {
		*splitter=0;
		changeDirTmp(filename);
	}*/
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	/*扫描当前目录查找文件*/
	ret = ScanEntry(filename,pentry,0);
	if(ret<0)
	{
		printf("no such file\n");
		free(pentry);
		return -1;
	}

	if(pentry->subdir) {
		struct Entry *tentry=(struct Entry*)malloc(sizeof(struct Entry));
		fd_cd(filename);
		for(i=curdir->FirstCluster;i!=0xffff;i=GetFatCluster(i)) {
			for(j=0;j<CLUSTER_SIZE;j+=abs(ret)) {
				lseek(fd,DATA_OFFSET+(i-2)*CLUSTER_SIZE+j,SEEK_SET);
				ret=GetEntry(tentry);
				if(ret>0 && strcmp(tentry->short_name,".") && strcmp(tentry->short_name,"..")) fd_df(tentry->short_name);
			}
		}
		fd_cd("..");
		free(tentry);
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

	for(lseek(fd,ret,SEEK_SET);magic>0;magic-=0x20) {
	if(lseek(fd,-0x20,SEEK_CUR)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");
	}

/*
	if(lseek(fd,ret-0x40,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if(write(fd,&c,1)<0)
		perror("write failed");*/

	free(pentry);
	if(WriteFat()<0)
		exit(1);
	return 1;
}
int mogic=0;

/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf_old(char *filename,int size)
{

	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset;
	unsigned short cluster,*clusterno;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	clustersize = (size / (CLUSTER_SIZE));
	clusterno=(unsigned short*)malloc(sizeof(short)*clustersize);

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,0);
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<(FAT_SIZE>>1);cluster++)
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
					if(mogic) {
						mogic=0;
						c[11]=ATTR_SUBDIR;
					}

					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

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
			int currentCluster=curdir->FirstCluster;
			for(;currentCluster!=0xffff;currentCluster=GetFatCluster(currentCluster)) {
			cluster_addr = (currentCluster/*curdir->FirstCluster*/ -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
			if(GetFatCluster(currentCluster)==0xffff) {
				int i;
				for(i=2*2;i<FAT_SIZE;i+=2)
					if(fatbuf[i]==0&&fatbuf[i+1]==0) {
						fatbuf[i]=fatbuf[i+1]=0xff;
						break;
					}
				if(i!=FAT_SIZE) {
					i<<=2;
					fatbuf[currentCluster<<1]=i&0xff;
					fatbuf[currentCluster<<1|1]=i>>8;
				}
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
	return -1;

}
int fd_cf(char *filename,char *data)
{
	//puts(data);
	struct Entry *pentry;
	int ret,i=0,cluster_addr,offset,size  = strlen(data);
	unsigned short cluster,*clusterno;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));


	clustersize = (size / (CLUSTER_SIZE));
	clusterno=(unsigned short*)malloc(sizeof(short)*clustersize);

	if(size % (CLUSTER_SIZE) != 0)
		clustersize ++;

	//扫描根目录，是否已存在该文件名
	ret = ScanEntry(filename,pentry,mogic);
	if (ret<0)
	{
		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<(FAT_SIZE>>1);cluster++)
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
					if(mogic) {
						c[11]=ATTR_SUBDIR;
					}
					FillTime(c+22);
					/*写第一簇的值*/
					c[26] = (clusterno[0] &  0x00ff);
					c[27] = ((clusterno[0] & 0xff00)>>8);

					/*写文件的大小*/
					c[28] = (size &  0x000000ff);
					c[29] = ((size & 0x0000ff00)>>8);
					c[30] = ((size& 0x00ff0000)>>16);
					c[31] = ((size& 0xff000000)>>24);

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,&c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					if(mogic==0)
						WriteContent(clusterno[0],data,size);
					else
					{
						int i;
						char content[640];
						FillNewDir(content,clusterno[0],0);
						FillNewDir(content,0,32);

						WriteContent(clusterno[0],content,64);
						mogic=0;
					}



					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
		}
		else
		{
			int currentCluster=curdir->FirstCluster;
			for(;currentCluster!=0xffff;currentCluster=GetFatCluster(currentCluster)) {
			cluster_addr = (currentCluster/*curdir->FirstCluster*/ -2 )*CLUSTER_SIZE + DATA_OFFSET;
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
					if(mogic) {
						c[11]=ATTR_SUBDIR;
					}
					FillTime(c+22);
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

					if(mogic==0)
						WriteContent(clusterno[0],data,size);
					else
					{
						char content[640];
						FillNewDir(content,clusterno[0],0);
						FillNewDir(content,curdir->FirstCluster,32);
						/*
						for(i = 0;i<64;i++)
						{
							printf("%c ",content[i]);
						}*/
						WriteContent(currentCluster,content,64);
						mogic=0;
					}
					free(pentry);
					if(WriteFat()<0)
						exit(1);

					return 1;
				}

			}
			if(GetFatCluster(currentCluster)==0xffff) {
				int i;
				for(i=2*2;i<FAT_SIZE;i+=2)
					if(fatbuf[i]==0&&fatbuf[i+1]==0) {
						fatbuf[i]=fatbuf[i+1]=0xff;
						break;
					}
				if(i!=FAT_SIZE) {
					i<<=2;
					fatbuf[currentCluster<<1]=i&0xff;
					fatbuf[currentCluster<<1|1]=i>>8;
				}
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
	return -1;

}

void do_usage()
{
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\tcd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tdf <file>\t\tdelete a file\n\texit\t\t\texit this system\n");
}

int fd_mkdir(char* name) {
	mogic = 1;
	char content[120] = "sdjfs";
	fd_cf(name,content);

}

void FillTime(char *content){
	time_t nowtime;
	struct tm *timeinfo;
	int time1,date1,year,month,day,second,hour,minute;
	time( &nowtime );
	timeinfo = localtime(&nowtime);
	year = timeinfo->tm_year;//+1900 for the real year
	month = 1+timeinfo->tm_mon;
	day = timeinfo->tm_mday;
	hour = timeinfo->tm_hour;
	second = timeinfo->tm_sec/2;
	minute = timeinfo->tm_min;

	time1=date1=0;
	time1 = (hour << 11) | (minute << 5) | (second);
	date1 = (year << 9) | ( month << 5) | day;

	content[1] = (unsigned char)(time1 >> 8) & 255;
	content[0] = (unsigned char)time1 & 255;
	content[3] = (unsigned char)(date1 >> 8) & 255;
	content[2] = (unsigned char)date1 & 255;
}
void FillNewDir(char *content,int currentStartCluster,int startIndex)
{
	int i = 0;

	if(startIndex==0)
	{
		content[i+startIndex] = '.';
		i++;
	}
	if(startIndex!=0)
	{
		content[startIndex] = content[startIndex+1] = '.';
		i = 2;
	}
	for(;i<11;i++)
	{
	content[startIndex+i] = (char)0x20;
	}
	if(magic==1)
		content[startIndex+11] = (char)0x04;
	else
		content[startIndex+11] = (char)0x05;
	for(i = 12;i<22;i++)
	content[startIndex+i] = (char)0x00;

	FillTime(content+22);
	/*
	content[startIndex+22] = (char)(((hour&31)<<3)+((minute>>3)&7));
	content[startIndex+23] = (char)(((minute&7)<<5)+(second&31));
	content[startIndex+24] = (char)(((year&127)<<1)+((month>>3)&1));

	content[startIndex+25] = (char)(((month&7)<<5)+(day&31));
	*/

	content[startIndex+27] = (char)((currentStartCluster>>8)&255);

	content[startIndex+26] = (char)(currentStartCluster&255);

	for(i = 28;i<32;i++)
	{
		content[startIndex+i] = (char)0x00;
	}
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
		else if(strcmp(input,"mkdir")==0) {
			scanf("%s",name);
			fd_mkdir(name);
		}
		else
			do_usage();
	}

	return 0;
}
