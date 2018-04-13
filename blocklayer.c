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


/* the url of the RSS feed from which to download memes */
#define RSSFEED "https://www.reddit.com/r/me_irl.rss?sort=new&limit=50"

/* folder for memes to be stored in, currently a compile time arg */
#define FOLDRNAME "memes/"

/* Max number of memes to try and "preload" at a time
 * a pre-loaded meme isn't downloaded, just its url saved for future downloading
*/
#define GETSIZE 50

/* max length of urls to grab */
#define MAX_MATCH 360

/* TODO properly optimize for ordered queues */


#define TEST 0

#define WORKTEST 1

#define TESTLOAD 1

#if QUETYPE == 0
#include "queue.c"
#else
#include "queue_desc.c"
#endif

#define FILEMODE 0000666

#define UNUSED(x) (void)(x)

#if VERSION == 0
#define BIGFILENAME "temp_blockdev" /* only used in version 0 */
#define BIGFILEMODE 0000666
#define METABLOCKS 1

/* bigfile version globals */
int bigfile;
queue freed_blocks;
int next_alloc;
char *path;
static int real_goto_block(int blockNum)
{
  if(lseek(bigfile, (blockNum) * BLOCKSIZE, SEEK_SET) < 0)
  {
    printf("Error Seeking File\n");
    return -1;
  }
  return 0;
}
static int save_state()
{
  int size;
  int buffersize;
  int written;
  int *buff;
  printf("Here's a save_state!\n");
  size = freed_blocks.size;
  buffersize = (size + 2) * sizeof(int);
  if (buffersize > BLOCKSIZE)
  {
    printf("ERROR, DATA STRUCTURE LARGER THAN BLOCKSIZE\n");
    return -1;
  }
  buff = malloc(buffersize);
  buff[0] = next_alloc;
  queue_save(buff+1, &freed_blocks);
  real_goto_block(0);
  written = write(bigfile, buff, BLOCKSIZE);
  if (written < 0)
  {
    return -1;
  }
  printf("Wrote %d bytes\n", written);
  free(buff);
  return 0;
}
static int load_state()
{
  char rbuff[BLOCKSIZE];
  int *buff;
  int ret;
  size_t size;
  buff = (int*) &rbuff;
  printf("Here's a load_state\n");
  real_goto_block(0);
  ret = read(bigfile, buff, BLOCKSIZE);
  next_alloc = buff[0];
  size = (size_t)buff[1];
  if (ret < 0)
    return -1;
  freed_blocks = new_queue();
  queue_load(buff+1, &freed_blocks);
  printf("Expected %ld got %ld\n", size, freed_blocks.size);
  if (freed_blocks.size != size)
    return -1;
  return 0;
}
static int init()
{
  char buffer[PATH_MAX] = {0};
  strcpy((char *)&buffer, path);
  strcat((char *)&buffer, BIGFILENAME);
  bigfile = open((char *)&buffer, O_RDWR|O_CREAT|O_EXCL, BIGFILEMODE);
  if (bigfile < 0)
  {
    printf("File Exists! (probably)\n");
    bigfile = open((char *)&buffer, O_RDWR, BIGFILEMODE);
    if (bigfile < 0)
    {
      printf("Ok no just an error oppening file\n");
      return -1;
    }
    if (load_state() < 0)
    {
      printf("Error loading previous state\n");
      return -1;
    }
    return 0;
  }
  freed_blocks = new_queue();
  next_alloc = 0;
  return 0;
}
static int destroy()
{
  int ret = 0;
  if (save_state() < 0)
  {
    printf("Saving state failed\n");
    ret = -1;
  }
  close(bigfile);
  while (queue_size(&freed_blocks) > 0)
  {
    queue_pop(&freed_blocks);
  }
  return ret;
}
static int goto_block(int blockNum)
{
  return real_goto_block(blockNum + METABLOCKS);
}
int block_dev_init(char *cwd)
{
  path = strdup(cwd);
  return init();
}
int block_dev_destroy()
{
  free(path);
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
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
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
#if TESTLOAD
  int i;
  int tm;
  printf("PRINTING DIAGNOSTICS\n");
  printf("\tnext_alloc: %d\n\tfreed_blocks.size: %ld\n", next_alloc, freed_blocks.size);

  for (i = 0; i < 30; i++)
  {
    tm = allocate_block();
    printf("allocated block %d\n", tm);
    if (tm == -1)
    {
      printf("\tERROR getting block %d\n", i);
      return -1;
    }
  }
  for (i = 0; i < 29; i++)
  {
    tm = free_block(i);
    printf("freed_block block %d\n", i);
    if (tm == -1)
    {
      printf("\tERROR freeing block %d\n", i);
      return -1;
    }
  }
  printf("PRINTING DIAGNOSTICS\n");
  printf("\tnext_alloc: %d\n\tfreed_blocks.size: %ld\n", next_alloc, freed_blocks.size);

  return 0;
#else
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
#endif
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
  free(path);
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
/* TODO replace stubs */
/* TODO write error safe code */
/* TODO support directory stuffs */
#include "memedl.c"
#define EXTENTION ".jpg"

/* globals */
queue freed_blocks;
int next_alloc;
char *path;

static int new_meme(char *path)
{
  char* old;
  old = get_meme();
  rename(old, path);
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
int block_dev_init(char *cwd)
{
  memedl_init();
  freed_blocks = new_queue();
  next_alloc = 0;
  path = strdup(cwd);
  return 0;
}
int block_dev_destroy()
{
  memedl_destroy();
  queue_destroy(&freed_blocks);
  free(path);
  return 0;
}
static int image_read(const char *path, char *buf)
{
  /* stub temp for stenography */
  return 0;
}
static int image_write(const char *path, const char* buf)
{
  /* stub temp for stenography */
  return 0;
}
int read_block(int blockNum, char *buf)
{
  char filename[NAME_MAX];
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
  int_to_name(blockNum, (char *) &filename);
  image_read((char*)&filename,buf);
  return 0;
}
int write_block(int blockNum, const char *buf)
{
  char filename[NAME_MAX];
  if (blockNum >= next_alloc)
  {
    /* I'm being asked about a block you definetly havent alloced yet */
    return -1;
  }
  int_to_name(blockNum, (char *) &filename);
  remove((char*)&filename);
  new_meme((char*)&filename);
  image_write((char*)&filename, buf);
  return 0;
}
int allocate_block()
{
  int newblock;
  char buffer[BLOCKSIZE] = {0};
  char filename[NAME_MAX];


  if (queue_size(&freed_blocks) > 0)
  {
    newblock = queue_pop(&freed_blocks);
  }
  else
  {
    newblock = next_alloc;
    next_alloc += 1;
  }
  int_to_name(newblock, (char *) &filename);
  new_meme((char*)&filename);
  image_write((char*)&filename, buffer);
  return newblock;
}
static int free_cleanup()
{
  int i;
  char filename[NAME_MAX];

  i = next_alloc -1;
  while (queue_remove(i, &freed_blocks) != -1)
  {
    int_to_name(i, (char *) &filename);
    remove((char*)&filename);
    next_alloc -= 1;
    i = next_alloc -1;
  }
  return 0;
}

int free_block(int blockNum)
{
  int retval;
  char filename[NAME_MAX];

  int_to_name(blockNum, (char *) &filename);
  remove((char*)&filename);
  if (blockNum == (next_alloc - 1))
  {
    next_alloc -= 1;
    retval = 0;
  }
  else
  {
    retval = queue_push(blockNum, &freed_blocks);
  }
  free_cleanup();
  return retval;
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
#if WORKTEST
  if (memcmp(read_buffer, write_buffer, BLOCKSIZE) != 0)
  {
    printf("\tERROR reading produced different bytes than those writen\n");
    return -1;
  }
#endif
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
#if WORKTEST
  if (memcmp(read, buf, BLOCKSIZE) != 0)
  {
    printf("\tERROR read and write wrong\n");
    return -1;
  }
#endif
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
#if WORKTEST
  if (memcmp(read, buf, BLOCKSIZE) != 0)
  {
    printf("\tERROR read and write wrong\n");
    return -1;
  }
#endif
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
  block_dev_destroy();
  return 0;
}



#endif /* TEST */
