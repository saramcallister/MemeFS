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

#define TEST 1

/* GLOBALS and INIT section */

#if VERSION == 0
/* bigfile version globals */
int bigfile;
queue freed_blocks;
size_t next_alloc;

static int init()
{
  bigfile = open(BIGFILENAME, O_RDWR|O_CREAT, S_IROTH|S_IWOTH);
  if (bigfile < 0)
  {
    printf("Couldn't open file");
    return -1;
  }
  freed_blocks = new_queue();
  next_alloc = 0;
  return 0;
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
    return pop(&freed_blocks);
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

#elif VERSION == 1
/* text files version */
#else
/* images version */
#endif

#if TEST /* include main for running some tests */

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



int main(int argc, const char* argv[])
{
  printf("Initializing w/ Mode: %d\n",VERSION);
  if(block_dev_init() == -1)
  {
    printf("init failed\n");
    return -1;
  }
  printf("RUNNING TESTS NOW\n");

  if (basic_test() == -1)
  {
    printf("error in basic_test\n");
    return -1;
  }

  printf("SURVIVED TESTS!\n");
  return 0;
}



#endif /* TEST */
