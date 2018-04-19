/* AUTHOR: M Jake Palanker */
/* Terrible basic queue */
#include <stdlib.h>
#include <stdio.h>


typedef struct queue_entry queue_entry;


typedef struct
{
  size_t size;
  queue_entry *head;
  queue_entry *tail;
} queue;


struct queue_entry
{
  int data;
  queue_entry *next;
};

int queue_size(const queue *q)
{
  return q->size;
}

int queue_push(int data, queue *q)
{
  queue_entry *new;
  queue_entry *last;
  queue_entry *next;
  new = malloc(sizeof(struct queue_entry));
  new->data = data;
  new->next = NULL;
  if (queue_size(q) == 0)
  {
    q->head = new;
    q->tail = new;
  }
  else if (q->head->data < data)
  {
    new->next = q->head;
    q->head = new;
  }
  else
  {
    next = q->head;
    while ((next->data > data) && (next != NULL))
    {
      last = next;
      next = next->next;
    }
    last->next = new;
    new->next = next;
  }
  q->size = q->size + 1;
  return 0;

}

int queue_pop(queue *q)
{
  queue_entry *temp;
  int retval;
  if (q->size == 0)
  {
    printf("THIS QUEUE IS EMPTY, WHY DID YOU POP?\n");
    return -1;
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


queue new_queue()
{
  queue new;
  new.size = 0;
  new.head = NULL;
  new.tail = NULL;
  return new;
}

int queue_destroy(queue *q)
{
  while (queue_size(q) > 0)
  {
    queue_pop(q);
  }
  q->head = NULL;
  q->tail = NULL;
  return 0;
}


int is_in(int data, const queue *q)
{
  struct queue_entry *ptr;
  ptr = q->head;
  while(ptr != NULL)
  {
    if (data == ptr->data)
      return 1;
    ptr = ptr->next;
  }
  return 0;
}

int queue_remove(int data, queue *q)
{
  struct queue_entry *last;
  struct queue_entry *next;
  if (queue_size(q) == 0)
  {
    return -1;
  }

  next = q->head;
  if (next->data == data)
  {
    queue_pop(q);
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
}

int queue_save(int *buffer, queue *q)
{
  int size;
  int i;
  queue_entry *ptr;
  size = (q->size);
  ptr = q->head;
  buffer[0] = size;
  for (i = 0; i < size; i++)
  {
    buffer[i+1] = ptr->data;
    ptr = ptr->next;

  }
  return size;

}

int queue_load(int *buffer, queue *q)
{

  int size;
  int i;

  size = buffer[0];
  for (i = 0; i < size; i ++)
  {
    queue_push(buffer[i+1], q);
  }

  return size;
}
