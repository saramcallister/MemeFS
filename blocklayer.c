/* AUTHOR: M Jake Palanker */

#include "blocklayer.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#define TESTPATH "/home/meow/Documents/MemeFS/"

#define VERSION 0
      /*  - 0 just use a big file
          - 1 use text files
          - 2 actually use images */

#define QUETYPE 0
      /*  - 0 unordered
          - 1 ordered descending */

#define TEST 0


#if QUETYPE == 1
#include "queue.c"
#else
#include "queue_desc.c"
#endif


#define FILEMODE 0000666

#define UNUSED(x) (void)(x)

#if VERSION == 0
#define BIGFILENAME "temp_blockdev" /* only used in version 0 */
#define BIGFILEMODE 0000666

/* bigfile version globals */
int bigfile;
queue freed_blocks;
int next_alloc;
char *path;
static int init()
{
  char buffer[PATH_MAX] = {0};
  strcpy((char *)&buffer, path);
  strcat((char *)&buffer, BIGFILENAME);
  bigfile = open((char *)&buffer, O_RDWR|O_CREAT, BIGFILEMODE);
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
  close(bigfile);
  while (queue_size(&freed_blocks) > 0)
  {
    queue_pop(&freed_blocks);
  }
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
int block_dev_init(char *cwd)
{
  path = strdup(cwd);
  return init();
}
int block_dev_destroy()
{
  return destroy();
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

  if (queue_size(&freed_blocks) > 0)
  {
    return queue_pop(&freed_blocks);
  }
  else
  {
    newblock = next_alloc;
    next_alloc += 1;
    return newblock;
  }
}
static int free_cleanup()
{
  int i;
  i = next_alloc -1;
  while (queue_remove(i, &freed_blocks) != -1)
  {
    next_alloc -= 1;
    i = next_alloc -1;
  }
  return 0;
}
int free_block(int blockNum)
{
  int retval;
  if (blockNum == (next_alloc - 1))
  {
    next_alloc -= 1;
    free_cleanup();
    return 0;
  }
  else
  {
    retval = queue_push(blockNum, &freed_blocks);
    free_cleanup();
    return retval;
  }
}
static int basic_test()
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
static int better_test()
{
  int first;
  int second;
  int third;
  first = allocate_block();
  second = allocate_block();
  third = allocate_block();

  if(queue_size(&freed_blocks) != 0)
  {
    printf("\tERROR freelist not empty at start\n");
    return -1;
  }
  if (free_block(first) == -1)
  {
    printf("\tERROR freeing first block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 1)
  {
    printf("\tERROR freelist not equal to one\n");
    return -1;
  }
  if (free_block(second) == -1)
  {
    printf("\tERROR freeing second block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 2)
  {
    printf("\tERROR freelist not equal to two\n");
    return -1;
  }
  if (free_block(third) == -1)
  {
    printf("\tERROR freeing third block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 0)
  {
    printf("\tERROR freelist cleanup failed\n");
    return -1;
  }

  return 0;
}
static int far_test()
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
static int run_tests()
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
/* many files version */
#define EXTENTION ".txt"
/* globals */
queue freed_blocks;
int next_alloc;
char *path;

int block_dev_init(char *cwd)
{
  path = strdup(cwd);
  freed_blocks = new_queue();
  next_alloc = 0;
  return 0;
}
int block_dev_destroy()
{
  while (queue_size(&freed_blocks) > 0)
  {
    queue_pop(&freed_blocks);
  }
  return 0;
}
static int int_to_name(int val, char *name)
{
  char buffer[NAME_MAX] = {0};
  snprintf((char *)&buffer, NAME_MAX, "%d%s", val, EXTENTION);
  strcpy(name, path);
  strcat(name, (char*)&buffer);
  return 0;
}
int read_block(int blockNum, char *buf)
{
  int filedesc;
  char filename[NAME_MAX];
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
  int_to_name(blockNum, (char *) &filename);
  filedesc = open(filename, O_RDONLY, FILEMODE);
  read(filedesc, buf, BLOCKSIZE);
  close(filedesc);
  return 0;
}
int write_block(int blockNum, const char *buf)
{
  int filedesc;
  char filename[NAME_MAX];
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
  int_to_name(blockNum, (char *) &filename);
  filedesc = open(filename, O_WRONLY, FILEMODE);
  write(filedesc, buf, BLOCKSIZE);
  close(filedesc);
  return 0;
}
static int create_file(int blockNum)
{
  int filedesc;
  char filename[NAME_MAX];
  int_to_name(blockNum, (char *) &filename);
  filedesc = open(filename, O_CREAT|O_WRONLY, FILEMODE);
  close(filedesc);
  return 0;
}
static int remove_file(int blockNum)
{
  char filename[NAME_MAX];
  int_to_name(blockNum, (char *) &filename);
  return remove(filename);
}
int allocate_block()
{
  int newblock;

  if (queue_size(&freed_blocks) > 0)
  {
    newblock = queue_pop(&freed_blocks);
  }
  else
  {
    newblock = next_alloc;
    next_alloc += 1;
  }
  create_file(newblock);
  return newblock;
}
static int free_cleanup()
{
  int i;
  i = next_alloc -1;
  while (queue_remove(i, &freed_blocks) != -1)
  {
    remove_file(i);
    next_alloc -= 1;
    i = next_alloc -1;
  }
  return 0;
}
int free_block(int blockNum)
{
  int retval;
  if (remove_file(blockNum) == -1)
  {
    printf("ERROR removing\n");
  }
  if (blockNum == (next_alloc - 1))
  {
    next_alloc -= 1;
    free_cleanup();
    return 0;
  }
  else
  {
    retval = queue_push(blockNum, &freed_blocks);
    free_cleanup();
    return retval;
  }
}
static int basic_test()
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
static int better_test()
{
  int first;
  int second;
  int third;
  first = allocate_block();
  second = allocate_block();
  third = allocate_block();

  if(queue_size(&freed_blocks) != 0)
  {
    printf("\tERROR freelist not empty at start\n");
    return -1;
  }
  if (free_block(first) == -1)
  {
    printf("\tERROR freeing first block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 1)
  {
    printf("\tERROR freelist not equal to one\n");
    return -1;
  }
  if (free_block(second) == -1)
  {
    printf("\tERROR freeing second block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 2)
  {
    printf("\tERROR freelist not equal to two\n");
    return -1;
  }
  if (free_block(third) == -1)
  {
    printf("\tERROR freeing third block\n");
    return -1;
  }

  if(queue_size(&freed_blocks) != 0)
  {
    printf("\tERROR freelist cleanup failed\n");
    return -1;
  }

  return 0;
}
static int far_test()
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
static int queue_test()
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
  for (i = 0; i < 30; i++)
  {
    free_block(blocks[i]);
  }
  return 0;
}
static int run_tests()
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
  if (queue_test() == -1)
  {
    printf("error in queue test\n");
    return -1;
  }
  return 0;
}
#else
/* images version */
#endif

#if TEST /* include main for running some tests */


int main(int argc, const char* argv[])
{
  UNUSED(argc);
  UNUSED(argv);
  printf("Initializing w/ Mode: %d\n",VERSION);
  if(block_dev_init(TESTPATH) == -1)
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
