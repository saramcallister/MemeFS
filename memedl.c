#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getinmemory.c"
#include "urlextract.c"
#include "url2file.c"
#include "string_queue.c"

#define RSSFEED "https://www.reddit.com/r/dankmemes/.rss?sort=new"

#define FOLDRNAME "memes/" /*TODO make this work */

#define MEMEDL_TESTS 0

/* initializes required stuff for get_meme */
int memedl_init();

/* will download a BRAND NEW* meme and return you the filename
* *brand new relies on some assumptions about how frequently you ask
* for new memes, and currently isn't working well TODO improve turnover rate
*/
char *get_meme();

/* cleans up for nice exit
 * if you call destroy, but get_meme will cease to work until next inti call
 */
int memedl_destroy();



static string_queue url_queue;

/* pass me an n and a buffer of size (buffer[n][MAX_MATCH]) */
static int nmemeurls(int n, char* buffer)
{
  char *ret;
  int r;
  ret = getinmemory(RSSFEED);
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
  char buffer[20][MAX_MATCH];
  char *new;
  int r;
  int i;
  string_queue urls = new_string_queue();
  r = nmemeurls(20,(char*)&buffer);
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
  char *url;
  char *filename;

  url = string_queue_pop(q);

  filename = strdup(filenameget(url));
  /* TODO directory support? */
  url2file(url, filename);
  return filename;
}

char *get_meme()
{
  if (string_queue_size(&url_queue) == 0)
  {
    url_queue = get_url_queue();
  }
  return url_queue_pop(&url_queue);
}

int memedl_init()
{
  url_queue = get_url_queue();
}

int memedl_destroy()
{
  string_queue_destroy(&url_queue);
}


#if MEMEDL_TESTS

int main(int argc, char const *argv[])
{
  char *new;
  int i;
  memedl_init();
  for (i = 0; i < 10; i++)
  {
    new = get_meme();
    printf("GOT %s\n", new);
    free(new);
  }

  memedl_destroy();
  return 0;
}

#endif
