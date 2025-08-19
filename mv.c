#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int mv_main(int argc, char **argv) 
{
    if(argc == 1)
    {
        printf("mv: missing file operand\n");
        return(1);
    }
    else if(argc == 2)
    {
     printf("mv: missing destination file operand after '%S'\n",argv[1]);
        return(2);
    }
    else
    {
    
        if (rename(argv[1], argv[2])== 0)
        {
            return(0);
        }
        else{
            perror("Invalid to mv this file");
            return(3);
        }
    }
}