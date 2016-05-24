#include"filesys.h"
#include <time.h>

#define RevByte(low,hight) ((hight)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<<24|(higher)<<16 |(lower)<<8|lowest)

void ScanBootSector(){
    unsigned char buf[SECTOR_SIZE];
    int ret,i;

    if((ret=read(ud,buf,SECTOR_SIZE))<0)
        perror("read boot sector failed");

    for(i=0;i<=10;i++)
        bdptor.Oem_name[i]=buf[i+0x03];
    bdptor.Oem_name[i]='\0';

    bdptor.BytesPerSector    =RevByte(buf[0x0b],buf[0x0c]);
    bdptor.SectorsPerCluster =buf[0x0d];
    bdptor.ReservedSectors    =RevByte(buf[0x0e],buf[0x0f]);
    bdptor.FATs              =buf[0x10];
    bdptor.RootDirEntries    =RevByte(buf[0x11],buf[0x12]);
    bdptor.LogicSectors      =RevByte(buf[0x13],buf[0x14]);
    bdptor.MediaType         =buf[0x15];
    bdptor.SectorsPerFAT     =RevByte(buf[0x16],buf[0x17]);
    bdptor.SectorsPerTrack   =RevByte(buf[0x18],buf[0x19]);
    bdptor.Heads             =RevByte(buf[0x1a],buf[0x1b]);
    bdptor.HiddenSectors     =RevByte(buf[0x1c],buf[0x1d]);


    printf("Oem_name \t\t%s\n"
    "BytesPerSector \t\t%d\n"
    "SectorsPerCluster \t%d\n"
    "ReservedSectors \t%d\n"
    "FATs \t\t\t%d\n"
    "RootDirEntries \t\t%d\n"
    "LogicSectors \t\t%d\n"
    "MediaType \t\t%x\n"
    "SectorsPerFAT \t\t%d\n"
    "SectorsPerTrack \t%d\n"
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

void findDate(unsigned short *year,unsigned short *month,
              unsigned short *day,unsigned char info[2]){
	time_t timer;
    struct tm *tblock;
    timer=time(NULL);
    tblock=localtime(&timer);     
	/*int data;
    data =RevByte(info[0],info[1]);*/
    *year=tblock->tm_year+1900/*((data & MASK_YEAR)>>9)+1980*/;
    *month=tblock->tm_mon+1/*(data & MASK_MONTH)>>5*/;
    *day=tblock->tm_mday/*(data &MASK_DAY)*/;

}

void findTime(unsigned short *hour,unsigned short *min,
              unsigned short *sec,unsigned char info[2]){
    time_t timer;
    struct tm *tblock;
    timer=time(NULL);
    tblock=localtime(&timer); 
    /*time=RevByte(info[0],info[1]);*/
    *hour=tblock->tm_hour/*(time & MASK_HOUR)>>118*/;
    *min=tblock->tm_min/*(time & MASK_MIN)>>5*/;
    *sec=tblock->tm_sec/*(time & MASK_SEC)*2*/;
}

void FileNameFormat(unsigned char *name){
    unsigned char *p=name;
    while(*p!='\0')
        p++;
    p--;
    //p=name+strlen(name)-1;/* stripp the traling space */
    while(isspace(*p))
        p--;
    p++;
    *p='\0';
}

int GetEntry(struct Entry *pentry){
    int ret,i;
    int count=0;
    unsigned char buf[DIR_ENTRY_SIZE],info[2];

    if((ret=read(ud,buf,DIR_ENTRY_SIZE))<0)
      perror("read entry failed");
    count+=ret;

    if(buf[0]==0xe5 || buf[0]==0x00)
     return -1*count;
    else{
		while(buf[11]==0x0f) {
		    if((ret=read(ud,buf,DIR_ENTRY_SIZE))<0)
		        perror("read root dir failed");
		    count+=ret;
		}
		for(i=0;i<=10;i++)
		    pentry->short_name[i]=buf[i];
		pentry->short_name[i]='\0';


		FileNameFormat(pentry->short_name);

		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->min),&(pentry->sec),info);

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		pentry->FirstCluster = RevByte (buf[26],buf[27]);
		pentry->size = RevWord (buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly = (buf[11] &ATTR_READONLY) ? 1:0;
		pentry->hidden   = (buf[11] &ATTR_HIDDEN)   ? 1:0;
		pentry->system   = (buf[11] &ATTR_SYSTEM)   ? 1:0;
		pentry->vlabel   = (buf[11] &ATTR_VLABEL)   ? 1:0;
		pentry->subdir   = (buf[11] &ATTR_SUBDIR)   ? 1:0;
		pentry->archive  = (buf[11] &ATTR_ARCHIVE)  ? 1:0;

		return count;
	}
}


void ScanRootEntry(){
    struct Entry entry;
    int ret,offset;

    if((ret=lseek(ud,ROOTDIR_OFFSET,SEEK_SET)) <0)
        perror("lseek ROOTDIR_OFFSET failed");

    offset=ROOTDIR_OFFSET;

    printf("Root dir\n");
    printf("\nname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

    while(offset < DATA_OFFSET)
	{
        ret=GetEntry(&entry);
        offset+=abs(ret);
        if(ret>0){
            printf("%s"
            "\t%d:%d:%d"
            "\t%d:%d:%d"
            "\t\t%d"
            "\t%d"
            "\t\t%s\n",
           entry.short_name,
			entry.year,entry.month,entry.day,
			entry.hour,entry.min,entry.sec,
			entry.FirstCluster,
			entry.size,
			(entry.subdir)?"dir":"file");
        }
    }
}

int ud_ls()
{
    int ret,offset,cluster_addr;
    struct Entry entry;
	unsigned short index = 0xffff;

    unsigned char buf[DIR_ENTRY_SIZE];
    if((ret=read(ud,buf,DIR_ENTRY_SIZE))<0)
       perror("read entry failed\n");

    if (curdir == NULL)
    {
        ScanRootEntry(ud);
    }
    else
	{
        printf("%s_dir\n",curdir->short_name);
        printf("\nname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");
		index = curdir->FirstCluster;

		do{

	    	cluster_addr = DATA_OFFSET+(index - 2) * CLUSTER_SIZE;
		
			if ((ret = lseek(ud,cluster_addr,SEEK_SET)) < 0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;

			while (offset < cluster_addr +CLUSTER_SIZE){
				ret = GetEntry(&entry);
				offset += abs(ret);
				if (ret > 0){
				    printf( "%s\t"
				    "%d:%d:%d\t"
				    "%d:%d:%d\t\t"
				    "%d\t"
				    "%d\t\t"
				    "%s\n",
				    entry.short_name,
				    entry.year,entry.month,entry.day,
				    entry.hour,entry.min,entry.sec,
				    entry.FirstCluster,
					entry.size,
				    (entry.subdir)?"dir":"file");
				}
			}
			
			index = GetFatCluster(index);
		}while( index != 0xffff);
    }
    return 0;
}

unsigned short GetFatCluster(unsigned short prev){
    unsigned short next;
    int index;

    index = prev *2;
    next = RevByte (fatbuf[index],fatbuf[index+1]);


    return next;
}

void ClearFatCluster(unsigned short cluster){
    int index;

    index = cluster * 2;


    fatbuf[index] = 0x00;
    fatbuf[index+1] = 0x00;

}

int ScanEntry(char *entryname,struct Entry *pentry,int mode){
    int ret,offset,i;
    int cluster_addr;
    char uppername[80];
	unsigned short index;

    for (i = 0;i<strlen(entryname);i++)
        uppername[i] = toupper(entryname[i]);

    uppername[i] = '\0';

    if (curdir == NULL)
	{
        if ((ret == lseek(ud,ROOTDIR_OFFSET,SEEK_SET)) < 0)
            perror ("lseek ROOTDIR_OFFSET failed");
        offset = ROOTDIR_OFFSET;
        while (offset < DATA_OFFSET){
            ret = GetEntry(pentry);
            offset += abs(ret);
            if (pentry->subdir == mode && !strcmp(pentry->short_name,uppername))
                return offset;
        }
        return -1;
    }
	else
	{
		index = curdir->FirstCluster;
		do{
		    cluster_addr = DATA_OFFSET + (index - 2) * CLUSTER_SIZE;
		    if ((ret == lseek(ud,cluster_addr,SEEK_SET)) < 0)
		        perror ("lseek cluster_addr failed");
		    offset = cluster_addr;

		    while(offset < cluster_addr + CLUSTER_SIZE){
		        ret = GetEntry(pentry);
		        offset += abs (ret);
		        if (pentry->subdir == mode && !strcmp(pentry->short_name,uppername))
		        return offset;
		    }
			index = GetFatCluster(index);
		}while(index != 0xffff);
        return -1;
    }
}


void anlDir(char *dir, char x[][12])
{
	int i = 0;
	int j = 0;	
	int k = 0;
	if(dir[i] == '/') i++;
	for(j = 0; dir[i] != '\0'; i++,j++)
	{
		k = 0;
		while(dir[i] != '\0' && dir[i] != '/')
		{
			x[j][k] = dir[i];
			i++;
			k++;
		}
		x[j][k] = '\0';
		#ifdef DEBUG
		printf("%d:%s\n",j,x[j]);
		#endif
		if(dir[i] == '\0') i--;
	}
	x[j][0] = '\0';

}


int ud_cd(char *dir){
    struct Entry *pentry;
	struct Entry *tempstack[10];
	struct Entry *temp;
	char dirst[20][12];
	int i,j;
    int ret;


    if (!strcmp(dir,"."))
        return 1;

    if (!strcmp(dir,"..") && curdir == NULL)
        return 1;
     if (!strcmp(dir,"..") && curdir != NULL)
     {
         curdir=fatherdir[dirno];
         dirno--;
         return 1;
     }
	if (!strcmp(dir,"/"))
	{
		while(dirno > 0)
		{
			free(fatherdir[dirno]);
			dirno--;
		}
		free(curdir);
		curdir = NULL;
		return 1;
	}
	temp = curdir;
    //解析目录	
	if(dir[0] == '/') curdir = NULL;
	anlDir(dir,dirst);
	for(i = 0; dirst[i][0] != '\0'; i++){
		pentry = (struct Entry*)malloc(sizeof(struct Entry));
		tempstack[i] = pentry;
		ret = ScanEntry(dirst[i],pentry,1);
		if (ret < 0){
		    printf ("no such dir:%s\n",dirst[i]);
			while(i >= 0)
			{
		    	free(tempstack[i]);
				i--;
			}
			curdir = temp;
		    return -1;
		}
		curdir = pentry;
	}
	curdir = temp;
	if(dir[0] == '/') { 
		while(dirno > 0)
		{
			free(fatherdir[dirno]);
			dirno--;
		}
		curdir = NULL;
	}
	dirno++;
    fatherdir[dirno]=curdir;
	for( j = 0; j < i - 1; j++){
		dirno++;
		fatherdir[dirno]=tempstack[i];
	}

	//free (curdir);
    curdir = tempstack[j];

    return 1;
}

int WriteFat(){
    if(lseek(ud,FAT_ONE_OFFSET,SEEK_SET) < 0){
        perror("lseek failed");
        return -1;
    }
    if(write(ud,fatbuf,512*239) < 0){
        perror("read failed");
        return -1;
    }
    if(lseek(ud,FAT_TWO_OFFSET,SEEK_SET) < 0){
        perror("lseek failed");
        return -1;
    }
    if(write(ud,fatbuf,512*239) < 0){
        perror("read failed");
        return -1;
    }
    return 1;
}


int ReadFat()
{
    if(lseek(ud,FAT_ONE_OFFSET,SEEK_SET)<0){
        perror("lseek failed");
        return -1;
    }
    if(read(ud,fatbuf,512*239)<0){
        perror("read failed");
        return -1;
    }

	#ifdef DEBUG
	printf("fat info :\n");
	print_fat();
	#endif
}

int print_fat()
{
	int i = 0;
	int next;

	printf("fat info : \n");

	for(i = 0; i < 512*239; i += 2)
	{
		if( fatbuf[i+1] != 0 || fatbuf[i] != 0)
		{
			next = RevByte(fatbuf[i], fatbuf[i+1]);
			printf("%x:\t%x\n", i/2, next);
		}
	}
}

int CheckDir(struct Entry *pentry)
{
	int ret,offset,cluster_addr;
    struct Entry entry;
	unsigned short index = 0xffff;

    unsigned char buf[DIR_ENTRY_SIZE];
	index = pentry->FirstCluster;

	do{

		cluster_addr = DATA_OFFSET+(index - 2) * CLUSTER_SIZE;

		if ((ret = lseek(ud,cluster_addr,SEEK_SET)) < 0)
			perror("lseek cluster_addr failed");
		offset = cluster_addr;

		while (offset < cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry);
			offset += abs(ret);
			if (ret > 0){
				return 1;
			}
		}
	
		index = GetFatCluster(index);
	}while( index != 0xffff);
	return 0;
}

void ClearDir(struct Entry *pentry)
{
	int ret,offset,cluster_addr;
    struct Entry entry;
	unsigned char c;
	unsigned short seed,next;
	unsigned short index = 0xffff;

    unsigned char buf[DIR_ENTRY_SIZE];
	index = pentry->FirstCluster;

	do{

		cluster_addr = DATA_OFFSET+(index - 2) * CLUSTER_SIZE;

		if ((ret = lseek(ud,cluster_addr,SEEK_SET)) < 0)
			perror("lseek cluster_addr failed");
		offset = cluster_addr;

		while (offset < cluster_addr +CLUSTER_SIZE){
			ret = GetEntry(&entry);
			offset += abs(ret);
			if (ret > 0){
				//return ;
				if(entry.subdir == 1 && CheckDir(&entry) == 1)
				{
   					ClearDir(&entry);
				}
		
				seed = entry.FirstCluster;
				while((next = GetFatCluster(seed)) != 0xffff)
				{
					ClearFatCluster(seed);
					seed = next;
				}
				ClearFatCluster(seed);
				 c = 0xe5;
				//if (lseek(ud,ret-0x40,SEEK_SET) < 0)
					//perror("lseek fd_rm failed");
				//if (write(ud,&c,1) < 0)
					//perror("write failed");
				if(lseek(ud,offset-0x20,SEEK_SET) < 0)
					perror("lseek uf_df failed");
				if(write(ud,&c,1) <  0)
					perror("write failed");

				if (WriteFat()<0)
			   		 exit(1);

				if(lseek(ud,offset,SEEK_SET) < 0)
					perror("lseek uf_df failed");
			}
		}
	
		index = GetFatCluster(index);
	}while( index != 0xffff);
}
int ud_rrm(char *dirname)
{
	struct Entry *pentry;
    int ret;
    unsigned char c;
    unsigned short seed,next;
	char judge[12];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    ret = ScanEntry(dirname,pentry,1);
    if(ret < 0){
		printf("no such dir\n");
		free(pentry);
		return -1;
    }
	ClearDir(pentry);
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed)) != 0xffff)
	{
		ClearFatCluster(seed);
		seed = next;
	}
	ClearFatCluster(seed);
	 c = 0xe5;
	//if (lseek(ud,ret-0x40,SEEK_SET) < 0)
		//perror("lseek fd_rm failed");
	//if (write(ud,&c,1) < 0)
		//perror("write failed");
	if(lseek(ud,ret-0x20,SEEK_SET) < 0)
		perror("lseek uf_rm failed");
	if(write(ud,&c,1) <  0)
		perror("write failed");

	if (WriteFat()<0)
   		 exit(1);
	free(pentry);

	printf("dir delete OK!\n");
    	return 1;
	
}
int ud_rm(char *name)
{
	struct Entry *pentry;
    int ret1,ret2;
    unsigned char c;
    unsigned short seed,next;
	char judge[12];
	pentry = (struct Entry*)malloc(sizeof(struct Entry));
    ret1 = ScanEntry(name,pentry,1);
    ret2 = ScanEntry(name,pentry,0);
    if(ret1 < 0&& ret2<0){
		printf("no such target\n");
		free(pentry);
		return -1;
    }
	if(ret1>=0){
	if(CheckDir(pentry) == 0)//检测到为空目录
	{
		seed = pentry->FirstCluster;
		while((next = GetFatCluster(seed)) != 0xffff)
		{
			ClearFatCluster(seed);
			seed = next;
		}
		ClearFatCluster(seed);
		 c = 0xe5;
		//if (lseek(ud,ret1-0x40,SEEK_SET) < 0)
			//perror("lseek fd_rm failed");
		//if (write(ud,&c,1) < 0)
			//perror("write failed");
		if(lseek(ud,ret1-0x20,SEEK_SET) < 0)
			perror("lseek uf_df failed");
		if(write(ud,&c,1) <  0)
			perror("write failed");

		if (WriteFat()<0)
	   		 exit(1);
		printf("dir delete OK!\n");
		return 1;
	}

	printf("warn: this dir is not empty.Do you want to continue?(Y/N)\n");
	scanf("%s",judge);
	if(!(strcmp(judge,"y")==0||strcmp(judge,"Y")==0)) 
	return 1;
	ClearDir(pentry);
	seed = pentry->FirstCluster;
	while((next = GetFatCluster(seed)) != 0xffff)
	{
		ClearFatCluster(seed);
		seed = next;
	}
	ClearFatCluster(seed);
	 c = 0xe5;
	//if (lseek(ud,ret1-0x40,SEEK_SET) < 0)
		//perror("lseek fd_rm failed");
	//if (write(ud,&c,1) < 0)
		//perror("write failed");
	if(lseek(ud,ret1-0x20,SEEK_SET) < 0)
		perror("lseek uf_df failed");
	if(write(ud,&c,1) <  0)
		perror("write failed");

	if (WriteFat()<0)
   		 exit(1);
	free(pentry);

	printf("dir delete OK!\n");
    	return 1;
	}
	if(ret2>=0){
	seed = pentry->FirstCluster;
    	while ((next = GetFatCluster(seed)) != 0xffff){
		ClearFatCluster(seed);		
		seed = next;
    	}
    	ClearFatCluster(seed);
    	c = 0xe5;
    	//if (lseek(ud,ret2-0x40,SEEK_SET) < 0)
    	//perror("lseek fd_rm failed");
    	//if (write(ud,&c,1) < 0)
    	//perror("write failed");
    	if(lseek(ud,ret2-0x20,SEEK_SET) < 0)
    	perror("lseek uf_df failed");
    	if(write(ud,&c,1) <  0)
    		perror("write failed");

    	free(pentry);
    	if (WriteFat()<0)
   		 exit(1);
    	printf("file delete OK!\n");
    	return 1;}
}

int ud_df(char *filename)
{
    struct Entry *pentry;
    int ret;
    unsigned char c;
    unsigned short seed,next;

    //printf("\nA:\\rm %s\n",filename);

    pentry = (struct Entry*)malloc(sizeof(struct Entry));
    ret = ScanEntry(filename,pentry,0);
    if(ret < 0){
		printf("no such file\n");
		free(pentry);
		return -1;
    }

    seed = pentry->FirstCluster;
    while ((next = GetFatCluster(seed)) != 0xffff){
		ClearFatCluster(seed);		
		seed = next;
    }
    ClearFatCluster(seed);
    c = 0xe5;
    //if (lseek(ud,ret-0x40,SEEK_SET) < 0)
    	//perror("lseek fd_rm failed");
    //if (write(ud,&c,1) < 0)
    	//perror("write failed");
    if(lseek(ud,ret-0x20,SEEK_SET) < 0)
    	perror("lseek uf_df failed");
    if(write(ud,&c,1) <  0)
    	perror("write failed");

    free(pentry);
    if (WriteFat()<0)
   		 exit(1);
    printf("file delete OK!\n");
    return 1;
}

int InitCluster(int index)
{
	int cluster_addr = DATA_OFFSET + (index - 2) * CLUSTER_SIZE;
	unsigned char buf[DIR_ENTRY_SIZE];
	int ret, offset = 0;
	memset(buf, 0, sizeof(char) * DIR_ENTRY_SIZE);

	if((ret = lseek(ud, cluster_addr, SEEK_SET)) < 0)
	{
		perror("lseek clusert_addr faliled");
		return -1;
	}

	offset = cluster_addr;
	while(offset < cluster_addr + CLUSTER_SIZE)
	{
		if(write(ud, buf, DIR_ENTRY_SIZE) < 0)
		{
			perror("write failed");
			return -1;
		}
		offset += DIR_ENTRY_SIZE;
	}

	return 1;
}

int ud_cf(char *filename, int size)
{
	struct Entry *pentry;
	int ret, i=0, cluster_addr, offset;
	unsigned short cluster, clusterno[100];
	unsigned char c[DIR_ENTRY_SIZE];
	int index,found,clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];
	
	pentry = (struct Entry*)malloc (sizeof(struct Entry));
	
	clustersize = (size / (CLUSTER_SIZE));
	if(size % (CLUSTER_SIZE) != 0)
	{
		clustersize ++;
	}
	
	ret = ScanEntry(filename,pentry,1);
	if(ret < 0)
	{
		for(cluster = 2; cluster <1000; cluster++)
		{
			index = cluster*2;
			if(fatbuf[index] == 0x00&&fatbuf[index + 1] == 0x00)
			{
				clusterno[i] = cluster;
				i++;
				if(i == clustersize)
					break;				
			}
		}
		
		for(i = 0; i < clustersize - 1; i++)
		{
			index = clusterno[i]*2;
			fatbuf[index] = (clusterno[i+1] & 0x00ff);
			fatbuf[index + 1] = ((clusterno[i+1] & 0xff00)>>8);
		}
		index = clusterno[i]*2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;
		
		if(curdir == NULL)
		{
			if((ret = lseek(ud, ROOTDIR_OFFSET, SEEK_SET))<0)
			{
					perror("lseek ROOTDIR_OFFSET failed");
					
			}
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(ud, buf, DIR_ENTRY_SIZE))<0)
					perror("read entry failed");
				offset += abs(ret);
				if(buf[0] != 0xe5 && buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(ud,buf,DIR_ENTRY_SIZE))<0)
								perror("read root dir failed");
						offset += abs(ret);
					}
				}
				else
				{
					offset = offset-abs(ret);
					for(i = 0; i<=strlen(filename); i++)
					{
						c[i] = toupper(filename[i]);
					}
					for(; i <= 10; i++)
					{
						c[i] = ' ';
					}
					c[11] = 0x01;
					c[26] = (clusterno[0] & 0x00ff);
					c[27] = ((clusterno[0] & 0xff00) >> 8);
						
					c[28] = (size & 0x000000ff);
					c[29] = ((size & 0x0000ff00) >> 8);
					c[30] = ((size & 0x00ff0000) >> 16);
					c[31] = ((size & 0xff000000) >> 24);
					if(lseek(ud,offset, SEEK_SET) < 0 )
						perror("lseek ud_cf failed");
					if(write(ud, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					free(pentry);
					if(WriteFat() < 0)
						exit(1);
					return 1;
						
				}
			}
		}
		else
		{
			index = curdir->FirstCluster;
			//found = FALSE;
			do{
				cluster_addr = (index - 2)*CLUSTER_SIZE + DATA_OFFSET;
				if((ret = lseek(ud, cluster_addr, SEEK_SET)) < 0)
					perror("lseek cluster_addr failed");
				offset = cluster_addr;
				while(offset < cluster_addr + CLUSTER_SIZE)
				{
					if((ret = read(ud, buf,DIR_ENTRY_SIZE)) < 0)
						perror("read entry failed");
					offset += abs(ret);
					if(buf[0] != 0xe5 && buf[0]!= 0x00)
					{
						while(buf[11] == 0x0f)
						{
							if((ret = read(ud, buf, DIR_ENTRY_SIZE)) < 0)
								perror("read root dir failed");
							offset += abs(ret);
						}	
					}
					else
					{
						//found = TRUE;
						offset = offset - abs(ret);
						for(i = 0; i <= strlen(filename); i++)
						{
							c[i] = toupper(filename[i]);
						}
						for(; i <=10; i++)
							c[i] = ' ';
						c[11] = 0x01;
						c[26] = (clusterno[0] & 0x00ff);
						c[27] = ((clusterno[0] & 0xff00) >> 8);
					
						c[28] = (size & 0x000000ff);
						c[29] = ((size & 0x0000ff00) >> 8);
						c[30] = ((size & 0x00ff0000) >> 16);
						c[31] = ((size & 0xff000000) >> 24);
						if(lseek(ud,offset, SEEK_SET) < 0 )
							perror("lseek ud_cf failed");
						if(write(ud, &c,DIR_ENTRY_SIZE)<0)
							perror("write failed");
						free(pentry);
						if(WriteFat() < 0)
							exit(1);
						return 1;
					}	
				}
				index = GetFatCluster(index);
			}while(index != 0xffff);

			//查找fat表，找到空白簇
			for(cluster = 2; cluster <1000; cluster++)
			{
				if(fatbuf[cluster*2] == 0x00&&fatbuf[cluster*2+ 1] == 0x00)
				{
					index = index *2;
					fatbuf[index] = (cluster & 0x00ff);
					fatbuf[index + 1] = ((cluster & 0xff00) >> 8);
					index = cluster *2;
					fatbuf[index] = 0xff;
					fatbuf[index + 1] = 0xff;
					index = cluster;
					InitCluster(index);
					break;	
				}
			}
			cluster_addr = (index - 2)*CLUSTER_SIZE + DATA_OFFSET;
			if((ret = lseek(ud, cluster_addr, SEEK_SET)) < 0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;
			while(offset < cluster_addr + CLUSTER_SIZE)
			{
				if((ret = read(ud, buf,DIR_ENTRY_SIZE)) < 0)
					perror("read entry failed");
				offset += abs(ret);
				if(buf[0] != 0xe5 && buf[0]!= 0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(ud, buf, DIR_ENTRY_SIZE)) < 0)
							perror("read root dir failed");
						offset += abs(ret);
					}	
				}
				else
				{
					//found = TRUE;
					offset = offset - abs(ret);
					for(i = 0; i <= strlen(filename); i++)
					{
						c[i] = toupper(filename[i]);
					}
					for(; i <=10; i++)
						c[i] = ' ';
					c[11] = 0x01;
					c[26] = (clusterno[0] & 0x00ff);
					c[27] = ((clusterno[0] & 0xff00) >> 8);
				
					c[28] = (size & 0x000000ff);
					c[29] = ((size & 0x0000ff00) >> 8);
					c[30] = ((size & 0x00ff0000) >> 16);
					c[31] = ((size & 0xff000000) >> 24);
					if(lseek(ud,offset, SEEK_SET) < 0 )
						perror("lseek ud_cf failed");
					if(write(ud, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					free(pentry);
					if(WriteFat() < 0)
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
int ud_mkdir(char *filename)
{
	struct Entry *pentry;
	int ret, i=0, cluster_addr, offset;
	unsigned short cluster, clusterno = -1;
	unsigned char c[DIR_ENTRY_SIZE];
	int index, clustersize;
	unsigned char buf[DIR_ENTRY_SIZE];

	int size = 0;//CLUSTER_SIZE;

	memset(c, 0, sizeof(char) * DIR_ENTRY_SIZE);
	pentry = (struct Entry*)malloc (sizeof(struct Entry));
	ret = ScanEntry(filename,pentry,0);
	if(ret < 0)
	{
		for(cluster = 2; cluster <1000; cluster++)
		{
			index = cluster*2;
			if(fatbuf[index] == 0x00&&fatbuf[index + 1] == 0x00)
			{
				clusterno = cluster;
				break;				
			}
		}
		
		if(clusterno < 0)
		{
			perror("find fat error");
			return -1;
		}
		
		index = clusterno *2;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;
		InitCluster(index);
		
		if(curdir == NULL)
		{
			if((ret = lseek(ud, ROOTDIR_OFFSET, SEEK_SET))<0)
			{
					perror("lseek ROOTDIR_OFFSET failed");
					
			}
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(ud, buf, DIR_ENTRY_SIZE))<0)
					perror("read entry failed");
				offset += abs(ret);
				if(buf[0] != 0xe5 && buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(ud,buf,DIR_ENTRY_SIZE))<0)
								perror("read root dir failed");
						offset += abs(ret);
					}
				}
				else
				{
					offset = offset-abs(ret);
					for(i = 0; i<=strlen(filename); i++)
					{
						c[i] = toupper(filename[i]);
					}
					for(; i <= 10; i++)
					{
						c[i] = ' ';
					}
					c[11] = 0x11;
					c[26] = (clusterno & 0x00ff);
					c[27] = ((clusterno & 0xff00) >> 8);
						
					c[28] = (size & 0x000000ff);
					c[29] = ((size & 0x0000ff00) >> 8);
					c[30] = ((size & 0x00ff0000) >> 16);
					c[31] = ((size & 0xff000000) >> 24);
					if(lseek(ud,offset, SEEK_SET) < 0 )
						perror("lseek ud_cf failed");
					if(write(ud, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					free(pentry);
					if(WriteFat() < 0)
						exit(1);
					return 1;
						
				}
			}
		}
		else
		{
			cluster_addr = (curdir->FirstCluster - 2)*CLUSTER_SIZE + DATA_OFFSET;
			if((ret = lseek(ud, cluster_addr, SEEK_SET)) < 0)
				perror("lseek cluster_addr failed");
			offset = cluster_addr;
			while(offset < cluster_addr + CLUSTER_SIZE)
			{
				if((ret = read(ud, buf,DIR_ENTRY_SIZE)) < 0)
					perror("read entry failed");
				offset += abs(ret);
				if(buf[0] != 0xe5 && buf[0]!= 0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(ud, buf, DIR_ENTRY_SIZE)) < 0)
							perror("read root dir failed");
						offset += abs(ret);
					}	
				}
				else
				{
					offset = offset - abs(ret);
					for(i = 0; i <= strlen(filename); i++)
					{
						c[i] = toupper(filename[i]);
					}
					for(; i <=10; i++)
						c[i] = ' ';
					c[11] = 0x11;
					c[26] = (clusterno & 0x00ff);
					c[27] = ((clusterno & 0xff00) >> 8);
					
					c[28] = (size & 0x000000ff);
					c[29] = ((size & 0x0000ff00) >> 8);
					c[30] = ((size & 0x00ff0000) >> 16);
					c[31] = ((size & 0xff000000) >> 24);
					if(lseek(ud,offset, SEEK_SET) < 0 )
						perror("lseek ud_cf failed");
					if(write(ud, &c,DIR_ENTRY_SIZE)<0)
						perror("write failed");
					free(pentry);
					if(WriteFat() < 0)
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
	printf("please input a command, including followings:\n\tls\t\t\tlist all files\n\t"
			"cd <dir>\t\tchange direcotry\n\tcf <filename> <size>\tcreate a file\n\tmkdir <dirname>\t\tcreate a dir\n\t"
			"rm <dir>\t\tdelete a dir\n\tdf <file>\t\t"
			"delete a file \n\tpf\t\t\tprint fattable\n\thelp\t\t\tprint help info\n\texit\t\t\texit this system\n");
}

int main()
{
	char input[10];
	int size = 0;
	char name[120];
	char rname[120];
	curdir = NULL;
	
	if((ud = open(DEVNAME,O_RDWR)) < 0)
	{
		perror("open failed");
		
	}

	ScanBootSector();

	if(ReadFat() < 0)
	{
		exit(EXIT_SUCCESS);
	}

	do_usage();

	while(1)
	{
		printf(">");
		scanf("%s", input);

		if(strcmp(input, "exit") == 0)
			break;
		else if(strcmp(input, "ls") == 0)
		{
			ud_ls();
		}
		else if(strcmp(input, "cd") == 0)
		{
			scanf("%s", name);
			ud_cd(name);
		}
		else if(strcmp(input, "rm") == 0)
		{
			scanf("%s", name);
			if(strcmp(name,"-r")==0)
			{
				scanf("%s", rname);
				ud_rrm(rname);
			}
			else{
			ud_rm(name);}
		}
		else if(strcmp(input, "df") == 0)
		{
			scanf("%s", name);
			ud_df(name);
		}
		else if(strcmp(input, "mkdir") == 0)
		{
			scanf("%s", name);
			ud_mkdir(name);
		}
		else if(strcmp(input, "cf") == 0)
		{
			scanf("%s", name);
			scanf("%s", input);
			size = atoi(input);

			ud_cf(name,size);
			#ifdef DEBUG
			print_fat();
			#endif
		}
		//#ifdef DEBUG
		else if(strcmp(input, "pf") == 0)
		{
			print_fat();
		}
		//#endif
		else if(strcmp(input, "help") == 0)
		{
			do_usage();
		}
	}
}

