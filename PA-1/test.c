#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <linux/kernel.h>

int main()
{
    long int hello = syscall(326);
    printf("Called sys_HelloWorld with a return value of %ld \n", hello);

    
   
    int result;
    long int add = syscall(327, 100, 200, &result);
    
    
    printf("Result of system add: %d \n", result);

}

