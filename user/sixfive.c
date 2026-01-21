#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"

void print_sixfive(int val){
    if(val % 5 == 0 || val % 6 == 0)
        printf("%d\n", val);
}
int is_num(char *buf, int buf_len){
    int i;
    for(i=0;i<buf_len;i++){
        if(buf[i] >= '0' && buf[i] <= '9'){
            continue;
        }else{
            break;
        }
    }
    return i == buf_len;
}
void number_extractor(char c){
    static char num_c[21]; // UINT64 has 20 digits
    static int num_c_digits = 0;
    if(c >= '0' && c <= '9'){
        num_c[num_c_digits++] = c;
    }
    else if(strchr(" -\r\t\n./,", c)){
        if(!is_num(num_c, num_c_digits)){
            num_c_digits = 0;
            memset(num_c, 0, sizeof(num_c));
        }
        int val = atoi(num_c);
        print_sixfive(val);

        num_c_digits = 0;
        memset(num_c, 0, sizeof(num_c));
    }
}

int main(int argc, char *argv[])
{
    int fd, rd_bytes;
    char buf[1];

    if(argc < 2){
        printf("[USER][SIXFIVE] should as least one argument(file name)!\n");
        exit(1);
    }
    for(int file_cnt=1; file_cnt<argc; file_cnt++){
        fd = open(argv[file_cnt], O_RDONLY);
        do{
            rd_bytes = read(fd, buf, 1);
            if(rd_bytes > 0)
                number_extractor(buf[0]);
        }while(rd_bytes);
        number_extractor('\n');

        close(fd);
        memset(buf, 0, sizeof(buf));
    }

    exit(0);
}