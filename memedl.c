#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "getinmemory.c"
#include "urlextract.c"

#define RSSFEED "https://www.reddit.com/r/dankmemes/.rss?sort=new"



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


int main(int argc, char const *argv[])
{
  char buffer[20][MAX_MATCH];
  int i;
  int r;
  r = nmemeurls(20,(char*)&buffer);
  for (i = 0; i < r; i++)
  {
    printf("got: %s\n", buffer[i]);
  }
  return 0;
}
