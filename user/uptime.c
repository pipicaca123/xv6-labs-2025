#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int uptime_ticks;
    if(argc != 1){
        printf("[USER][UPTIME] should not have argument!\n");
        exit(1);
    }
    uptime_ticks = uptime();
    printf("%d\n", uptime_ticks);
    exit(0);
}