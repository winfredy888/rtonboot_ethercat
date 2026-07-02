#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

#define PREACT_MAGIC "RTOnBoot PreACT Image Header"

char buffer[1024];
 
int main(int argc , char *argv[])
{
	int fd_preact;
	int fd_rtonboot;
	uint32_t pos;
	uint32_t len;
	FILE * fp; 	
	
	fp = popen("./rtonboot.sh", "r");
    if (fp == NULL) 
    {
        printf("popen failed");
        
        return -1;
    }
    
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("Output: %s", buffer);
    }
    
    pclose(fp);
    
    fd_preact = open("./preact.img", O_RDWR);	
	if (fd_preact < 0) 
	{
		printf("cannot open pre act img file for rw\n");
		
		return -1; 
	}
		
	pos = 0;
	
	lseek(fd_preact, pos, SEEK_SET);
	
	len = strlen(PREACT_MAGIC);
	
	read(fd_preact, &buffer[0], len + 1);
	
	if(strcmp(&buffer[0], PREACT_MAGIC))
	{
		printf("wrong magic for img file\n");
		
		close(fd_preact);
		
		return -1;
	}	
	
	pos = 152;
	
	lseek(fd_preact, pos, SEEK_SET);
	
	read(fd_preact, &buffer[0], 32);
	
	close(fd_preact);
	
	fd_rtonboot = open("./rtonboot.img", O_RDWR);	
	if (fd_rtonboot < 0) 
	{
		printf("cannot open rtonboot img file for rw\n");
		
		return -1; 
	}
	
	pos = 0x9e0;
	
	lseek(fd_rtonboot, pos, SEEK_SET);
	
	write(fd_rtonboot, &buffer[0], 32);
	
	close(fd_rtonboot);
	
	return 0;
}
