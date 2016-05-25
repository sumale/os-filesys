#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"
int main(){
	int i=1;
	while(i<=64){
		 char buf[100];
		 sprintf(buf,"/home/pi/os-filesys/source-code/u/t/text%d",i++);
		 FILE *fp=fopen(buf,"w");
		 printf("%d\n",i);
	}
	int fd;
	char buf1[100]={'\0'};
	char buf2[100]={'\0'};
	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	
	read(fd,buf1,10);
	printf("%d\n",lseek(fd,0,SEEK_CUR));
	read(fd,buf2,10);
	printf("%d\n",lseek(fd,0,SEEK_CUR));
	//printf("%s\n",buf1);	
	//printf("%s\n",buf2);
}
