#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int pwd_main() 
{
    char cmd[1024];
    if(NULL != getcwd(cmd,sizeof(cmd)))
    {
        printf("%s\n",cmd);
        return(0);
    }
    else
    {
        perror("pwd: getcmd() failed!!\n");
        return(-1);
    }
}