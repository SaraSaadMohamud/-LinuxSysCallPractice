#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int cp_main(int argc, char **argv) 
{
    if(argc == 1)
    {
        printf("cp: missing file operand\n");
        return(1);
    }
    else if (argc == 2)
    {
        printf("cp: missing destination file operand after '%s'\n",argv[1]);
        return(2);
    }
    else
    {
        int num_read = 0;
        char buf1[1024];
        int fd1 = open(argv[1],O_RDONLY);
        if (fd1 < 0)
        {
             return(5);
        }
        
        int fd2 = open(argv[2],O_CREAT |O_WRONLY | O_TRUNC, 0644);
        if (fd2 < 0)
        {
             close(fd1);
             return(4);
        }
        
        while( (num_read = read(fd1,buf1,sizeof(buf1))) > 0)
        {
            if(0 > write(fd2,buf1,num_read))
            {
                printf("Invalid to write this file !\n");
                return(3);
            }
        }
        
        close(fd1);
        close(fd2);
        
        return(0);
    }
}