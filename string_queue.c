/* AUTHOR: M Jake Palanker */
/* Terrible basic queue */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


typedef struct string_queue_entry string_queue_entry;


typedef struct
{
  size_t size;
  string_queue_entry *head;
  string_queue_entry *tail;
} string_queue;


struct string_queue_entry
{
  char *data;
  string_queue_entry *next;
};

int string_queue_size(const string_queue *q)
{
  return q->size;
}

int string_queue_push(char* data, string_queue *q)
{
  string_queue_entry *new;
  new = malloc(sizeof(struct string_queue_entry));
  new->data = data;
  new->next = NULL;
  if (string_queue_size(q) == 0)
  {
    q->head = new;
    q->tail = new;
  }
  else
  {
    q->tail->next = new;
    q->tail = new;
  }
  q->size = q->size + 1;
  return 0;

}

char *string_queue_pop(string_queue *q)
{
  string_queue_entry *temp;
  char *retval;
  if (q->size == 0)
  {
    printf("THIS string_queue IS EMPTY, WHY DID YOU POP?\n");
    return NULL;
  }
  else if (q->size == 1)
  {
    q->size = 0;
    retval = q->head->data;
    free(q->head);
    q->head = NULL;
    q->tail = NULL;
  }
  else
  {
    q->size = q->size -1;
    retval = q->head->data;
    temp = q->head->next;
    free(q->head);
    q->head = temp;
  }
  return retval;
}


string_queue new_string_queue()
{
  string_queue new;
  new.size = 0;
  new.head = NULL;
  new.tail = NULL;
  return new;
}

int is_in(char* data, const string_queue *q)
{
  struct string_queue_entry *ptr;
  ptr = q->head;
  while(ptr != NULL)
  {
    if (strcmp(data, ptr->data) == 0)
      return 1;
    ptr = ptr->next;
  }
  return 0;
}
/*
int string_queue_remove(char *data, string_queue *q)
{
  *//* DONT CALL IS BROKEN *//*
  struct string_queue_entry *last;
  struct string_queue_entry *next;
  if (string_queue_size(q) == 0)
  {
    return -1;
  }

  next = q->head;
  if (next->data == data)
  {
    string_queue_pop(q);
    return 0;
  }
  while (next != NULL)
  {
    if (next->data == data)
    {
      last->next = next->next;
      free(next);
      q->size -= 1;
      return 0;
    }
    last = next;
    next = next->next;
  }

  return -1;
}*/
int string_queue_destroy(string_queue *q)
{
  while (q->size > 0)
  {
    free(string_queue_pop(q));
  }
  return 0;
}

string_queue string_queue_copy(string_queue *q)
{
  char *copy;
  string_queue_entry *ptr;
  string_queue new = new_string_queue();

  ptr = q->head;

  while (ptr != NULL)
  {
    copy = strdup(ptr->data);
    string_queue_push(copy, &new);
    ptr = ptr->next;
  }
  return new;
}
