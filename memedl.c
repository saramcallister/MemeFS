#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "getinmemory.c"
#include "urlextract.c"
#include "url2file.c"
#include "string_queue.c"

#define MEMEDL_TESTS 0

#define DLFOLDRNAME ".dl/"

#if MEMEDL_TESTS
#define RSSFEED "https://www.reddit.com/r/me_irl.rss?sort=new&limit=50"


#define GETSIZE 50

#define MAX_MATCH 360

#endif

/* initializes required stuff for get_meme */
int memedl_init(char *path);

/* will download a BRAND NEW* meme and return you the filename
* *brand new relies on some assumptions about how frequently you ask
*/
char *get_meme();

/* cleans up for nice exit
 * if you call destroy, but get_meme will cease to work until next inti call
 */
int memedl_destroy();



static string_queue url_queue;

static char *dlpath;

/* pass me an n and a buffer of size (buffer[n][MAX_MATCH]) */
static int nmemeurls(int n, char* buffer)
{
  char *ret;
  int r;
  ret = getinmemory(RSSFEED);
  if (ret == NULL)
  {
    printf("Getting RSS Failed... are you online?\n");
    return -1;
  }
  r = getnjpg(n, ret,buffer);
  free(ret);
  return r;
}

static char *filenameget(const char* url)
{
  return strrchr(url, '/') + 1;
}

static string_queue get_url_queue()
{
  char buffer[GETSIZE][MAX_MATCH];
  char *new;
  int r;
  int i;
  string_queue urls = new_string_queue();
  r = nmemeurls(GETSIZE,(char*)&buffer);
  if (r < 1)
    return new_string_queue();
  for (i = 0; i < r; i++)
  {
    new = strdup(buffer[i]);
    string_queue_push(new, &urls);
  }
  return urls;
}

static char *url_queue_pop(string_queue *q)
{
  char dest[MAX_MATCH];
  char *url;
  char *ptr;

  strcpy((char*)&dest, dlpath);
  url = string_queue_pop(q);

  ptr = filenameget(url);
  strcat((char*)&dest,ptr);
  url2file(url, (char*)&dest);
  free(url);
  ptr = strdup((char*)&dest);
  return ptr;
}

char *get_meme()
{
  if (string_queue_size(&url_queue) == 0)
  {
    url_queue = get_url_queue();
    if (string_queue_size(&url_queue) == 0)
    {
      printf("Tried to get more memes and failed.\n");
      return NULL;
    }
  }
  return url_queue_pop(&url_queue);
}

int memedl_init(char *path)
{
  char buff[PATH_MAX];
  strcpy((char*)&buff, path);
  strcat((char*)&buff, DLFOLDRNAME);
  dlpath = strdup((char*)&buff);
  int err = mkdir(dlpath, 0755);
  if (err < 0 && errno != EEXIST)
  {
    printf("Error creating downloads directory: %s\n", strerror(errno));
  }
  url_queue = get_url_queue();
  if (string_queue_size(&url_queue) == 0)
  {
    printf("I Think your internet is not working\n");
    return -1;
  }
  return 0;
}

int memedl_destroy()
{
  string_queue_destroy(&url_queue);
  return 0;
}


#if MEMEDL_TESTS

int main(int argc, char const *argv[])
{
  char *new;
  int i;
  memedl_init("./");
  for (i = 0; i < GETSIZE; i++)
  {
    new = get_meme();
    printf("GOT %s\n", new);
    free(new);
  }
  new = NULL;
  memedl_destroy();
  return 0;
}

#endif
