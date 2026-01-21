#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int sleep_ticks;
    if(argc != 2){
        printf("[USER][SLEEP] should only be one argument!\n");
        exit(1);
    }
    sleep_ticks = atoi(argv[1]);
    pause(sleep_ticks);
    exit(0);
}