#include <string.h>
#include <stdio.h>
#include <regex.h>
#include <sys/types.h>
#include <stdlib.h>

int getnjpg(size_t n, const char *src, char *dest);


static int ngetmatches(size_t n, char *reg, const char *src, char *dest)
{
  regex_t regex;
  regmatch_t pmatch[2];
  size_t i;
  size_t count;
  const char *cursor;
  const char *old;
  size_t start;
  size_t end;
  size_t size;
  char *write;

  count = 0;
  write = dest;

  if (regcomp(&regex, reg, 0))
  {
    printf("Error compiling regex\n");
    return -1;
  }
  cursor = src;
  old = cursor;
  for (i = 0; i < n; i ++)
  {
    /*printf("cursor location %s\n",cursor);*/
    if (regexec(&regex, cursor, 2, (regmatch_t *)&pmatch, 0) != 0)
    {
      /*printf("No more matches: stopped at %ld\n", i);*/
      break;
    }
    else{
      start = pmatch[0].rm_so;
      end = pmatch[0].rm_eo;
      size = end - start;
      /*printf("Match data: %ld, %ld\n", start, end);*/
      cursor = old + start;
      strncpy(write, cursor, size);
      *(write + size) = '\0';
      /*printf("Found match: %s\n", write);*/
      write += MAX_MATCH;
      cursor = old + end;
      old = cursor;
      /*printf("end value: %ld\n", end);
      printf("cursor value: %ld\n", (size_t)cursor);*/
      count += 1;
    }
  }
  regfree(&regex);
  /*printf("Found %ld matches\n", count);*/
  return count;
}

int getnjpg(size_t n, const char *src, char *dest)
{
  /* TODO match more with nicer regex */
  return ngetmatches(n, "https://i[^ ]*.jpg", src, dest);
}
