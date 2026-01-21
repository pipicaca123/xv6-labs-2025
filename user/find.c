#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user/user.h"

#define MAX_FIND_FILE_CNT 32
#define MAX_FILENAME_LEN 128 // if too large, overflow occurs!

// get the string after last slash
char *fmtname(char *path){
    char *p;
    static char buf[DIRSIZ+1];

    for(p=path+strlen(path);*p!='/' && p>=path; p--)
        continue;
    p++; // first char after last slash

    if(strlen(p) >= DIRSIZ)
        printf("find: warning! dirname overflow!\n");
    memmove(buf, p, strlen(p));
    buf[strlen(p)] = '\0';
    return buf;
}

void execute(char *argv[], int argc, char *filenames[], int filecnt){
    int pid;
    if(argc <= 0){ // no argc, print all directly.
        for(int i=0;i<filecnt;i++)
            printf("%s\n", filenames[i]);
        return;
    }
    if(strcmp(argv[0], "-exec") != 0){
        printf("receiving unexpected arg, extension cmd should start from '-exec'\n");
        return;
    }

    for(int i=argc, j=0;i<filecnt+argc;i++, j++){
        argv[i] = filenames[j];
    }
    argv++; // no need arg: -exec any more.

    pid = fork();
    if(pid == 0){ // new proc
        exec(argv[0], argv);
        printf("find: exec CMD failed\n");
        exit(1);
    }else if(pid > 0){ // org proc
        pid = wait((int *) 0);
    }else{
        printf("find: fork failed\n");
        exit(1);
    }
}

void find(char *path, char *target, char *ext[], int ext_c, char *filenames[], int *p_filecnt){
    char buf[MAX_FILENAME_LEN], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // if(strcmp(path, ".") == 0 ||
    //     strcmp(path, "..") == 0)
    //     return;

    if((fd = open(path, O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }
    switch(st.type){
        case T_DIR:
            if(strlen(path)+1+DIRSIZ+1 > sizeof(buf)){
                // 1 for slash, 1 for '\0'
                printf("find: path too long!\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(path);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
                memmove(p, de.name, DIRSIZ);
                find(buf, target, ext, ext_c, filenames, p_filecnt);
            }
        break;
        case T_FILE:
        case T_DEVICE:
            if(strcmp(target, fmtname(path)) == 0){
                if(*p_filecnt > MAX_FIND_FILE_CNT){
                    printf("find: files too many, overflow find file buffer\r\n");
                    exit(1);
                }
                strcpy(filenames[(*p_filecnt)++], path);
            }
        break;
        default:
            fprintf(2, "find: not exist file stat(%d)!\n", st.type);
    }
    close(fd);
}
int
main(int argc, char *argv[])
{
    char target[MAX_FILENAME_LEN], rootpath[MAX_FILENAME_LEN];
    char *find_ext[MAX_FILENAME_LEN]; // assume extension command(exec) will not over MAX_FILENAME_LEN parameters
    char filenames[MAX_FIND_FILE_CNT][MAX_FILENAME_LEN]; // assume system at most 64 files.
    char *p_filenames[MAX_FIND_FILE_CNT];
    int find_ext_c = argc - 3;
    int file_cnt = 0;
    if(argc < 3){
        printf("[USER][FIND] should be more than three argument!\n");
        exit(1);
    }
    strcpy(rootpath, argv[1]);
    strcpy(target, argv[2]);
    for(int i=0;i<find_ext_c;i++){
        find_ext[i] = argv[i+3];
    }
    for(int i=0;i<MAX_FIND_FILE_CNT;i++){
        p_filenames[i] = filenames[i];
    }
    find(rootpath, target, find_ext, find_ext_c, p_filenames, &file_cnt);
    execute(find_ext, find_ext_c, p_filenames, file_cnt);
    exit(0);
}