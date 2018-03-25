/* AUTHOR: M Jake Palanker */

#include "blocklayer.h"
#include "queue.c"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define VERSION 0
      /*  - 0 just use a big file
          - 1 use text files
          - 2 actually use images */


#define BIGFILENAME "temp_blockdev" /* only used in version 0 */
#define BIGFILEMODE 0000666

#define TEST 0

/* GLOBALS and INIT section */

#if VERSION == 0
/* bigfile version globals */
int bigfile;
queue freed_blocks;
size_t next_alloc;
static int init()
{
  bigfile = open(BIGFILENAME, O_RDWR|O_CREAT, BIGFILEMODE);
  if (bigfile < 0)
  {
    printf("Couldn't open file");
    return -1;
  }
  freed_blocks = new_queue();
  next_alloc = 0;
  return 0;
}
static int destroy()
{
  while (size(&freed_blocks) > 0)
  {
    pop(&freed_blocks);
  }
}
static int goto_block(int blockNum)
{
  if(lseek(bigfile, blockNum * BLOCKSIZE, SEEK_SET) < 0)
  {
    printf("Error Seeking File\n");
    return -1;
  }
  return 0;
}
int block_dev_init()
{
  return init();
}
int read_block(int blockNum, char *buf)
{
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
  goto_block(blockNum);
  if (read(bigfile, buf, BLOCKSIZE) < 0)
  {
    return -1;
  }
  return 0;
}
int write_block(int blockNum, const char *buf)
{
  goto_block(blockNum);
  if (write(bigfile, buf, BLOCKSIZE) < 0)
  {
    return -1;
  }
  return 0;
}
int allocate_block()
{
  int newblock;

  if (size(&freed_blocks) > 0)
  {
    return pop(&freed_blocks);
  }
  else
  {
    newblock = next_alloc;
    next_alloc += 1;
    return newblock;
  }
}
int free_block(int blockNum)
{
  if (blockNum == (next_alloc - 1))
  {
    next_alloc -= 1;
    return 0;
  }
  else
  {
    return push(blockNum, &freed_blocks);
  }
}
int basic_test()
{
  char write_buffer[BLOCKSIZE];
  char read_buffer[BLOCKSIZE];
  int block_to_test;
  memset((char *)&write_buffer, 'a', BLOCKSIZE);
  block_to_test = allocate_block();
  if (block_to_test == -1)
  {
    printf("\tERROR allocating block\n");
    return -1;
  }
  if (write_block(block_to_test, (char *) &write_buffer) == -1)
  {
    printf("\tERROR writing block\n");
    return -1;
  }
  if (read_block(block_to_test, (char *) &read_buffer) == -1)
  {
    printf("\tERROR reading block\n");
    return -1;
  }
  if (memcmp(read_buffer, write_buffer, BLOCKSIZE) != 0)
  {
    printf("\tERROR reading produced different bytes than those writen\n");
    return -1;
  }

  if(free_block(block_to_test) == -1)
  {
    printf("\tERROR freeing block\n");
    return -1;
  }
  return 0;
}
int better_test()
{
  int first;
  int second;
  int third;
  first = allocate_block();
  second = allocate_block();
  third = allocate_block();

  if(size(&freed_blocks) != 0)
  {
    printf("\tERROR freelist not empty at start\n");
    return -1;
  }
  if (free_block(first) == -1)
  {
    printf("\tERROR freeing first block\n");
    return -1;
  }

  if(size(&freed_blocks) != 1)
  {
    printf("\tERROR freelist not equal to one\n");
    return -1;
  }
  if (free_block(second) == -1)
  {
    printf("\tERROR freeing second block\n");
    return -1;
  }

  if(size(&freed_blocks) != 2)
  {
    printf("\tERROR freelist not equal to two\n");
    return -1;
  }
  if (free_block(third) == -1)
  {
    printf("\tERROR freeing third block\n");
    return -1;
  }

  if(size(&freed_blocks) != 2)
  {
    printf("\tERROR freelist not still two\n");
    return -1;
  }

  return 0;
}
int far_test()
{
  int blocks[30];
  int i;
  char buf[BLOCKSIZE];
  char read[BLOCKSIZE];
  memset((char *)&buf, 'a', BLOCKSIZE);

  for (i = 0; i < 30; i++)
  {
    blocks[i] = allocate_block();
    if (blocks[i] == -1)
    {
      printf("\tERROR getting block %d\n", i);
      return -1;
    }
  }
  if(write_block(blocks[29], buf) == -1)
  {
    printf("\tERROR writing far block\n");
    return -1;
  }
  if(read_block(blocks[29],(char*)&read) == -1)
  {
    printf("\tERROR reading far block\n");
    return -1;
  }
  if (memcmp(read, buf, BLOCKSIZE) != 0)
  {
    printf("\tERROR read and write wrong\n");
    return -1;
  }
  for (i = 29; i >= 0; i--)
  {
    free_block(blocks[i]);
  }
  return 0;
}
int run_tests()
{
  if (basic_test() == -1)
  {
    printf("error in basic_test\n");
    return -1;
  }
  if (better_test() == -1)
  {
    printf("error in advanced tests\n");
    return -1;
  }
  if (far_test() == -1)
  {
    printf("error in far test\n");
    return -1;
  }
  return 0;
}
#elif VERSION == 1
/* text files version */
#else
/* images version */
#endif

#if TEST /* include main for running some tests */


int main(int argc, const char* argv[])
{
  printf("Initializing w/ Mode: %d\n",VERSION);
  if(block_dev_init() == -1)
  {
    printf("init failed\n");
    return -1;
  }
  printf("RUNNING TESTS NOW\n");
  if(run_tests() == -1)
    return -1;

  printf("SURVIVED TESTS!\n");
  return 0;
}



#endif /* TEST */
