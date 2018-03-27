#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getinmemory.c"
#include "urlextract.c"
#include "url2file.c"

#define RSSFEED "https://www.reddit.com/r/dankmemes/.rss?sort=new"

#define FOLDRNAME "memes/"


/* pass me an n and a buffer of size (buffer[n][MAX_MATCH]) */
int nmemeurls(int n, char* buffer)
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

int main(int argc, char const *argv[])
{
  const char folder[] = FOLDRNAME;
  char dest[MAX_MATCH];
  char buffer[20][MAX_MATCH];
  int i;
  int r;
  strcpy((char*)&dest,(char*)&folder);
  r = nmemeurls(20,(char*)&buffer);
  for (i = 0; i < r; i++)
  {
    printf("got: %s\n", buffer[i]);
    strcat((char*)&dest, filenameget(buffer[i]));
    printf("Saving to: %s\n", (char*)&dest);
    url2file(buffer[i],(char*)&dest);
    strcpy((char*)&dest,(char*)&folder);
  }
  return 0;
}
