/* AUTHOR: M Jake Palanker */

#include "blocklayer.h"
#include <stdio.h>

#define VERSION 0
      /*  - 0 just use a big file
          - 1 use text files
          - 2 actually use images */

#define TEST 0

int read_block(int blockNum, char *buf)
{
  /* TODO replace stub */
  return -1;
}

int write_block(int blockNum, const char *buf)
{
  /* TODO replace stub */
  return -1;
}

int allocate_block()
{
  /* TODO replace stub */
  return -1;
}

int free_block(int blockNum)
{
  /* TODO replace stub */
  return -1;
}

#if TEST /* include main for running some tests */

int main(int argc, const char* argv[])
{
  printf("RUNNING TESTS NOW\n");
  return 0;
}



#endif /* TEST */
