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

int push(int data, queue *q)
{
  queue_entry *new;
  new = malloc(sizeof(struct queue_entry));
  new->data = data;
  new->next = NULL;
  if (q->size == 0)
  {
    q->head = new;
    q->tail = new;
  }
  else
  {
    q->tail->next = new;
    q->tail = new;
  }
  q->size += 1;
  return 0;

}

int pop(queue *q)
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
    retval = q->head->data;
    free(q->head);
    q->head == NULL;
    q->tail == NULL;
  }
  else
  {
    retval = q->head->data;
    temp = q->head->next;
    free(q->head);
    q->head = temp;
  }
  return retval;
}

int size(queue *q)
{
  return q->size;
}

queue new_queue()
{
  queue new;
  new.size = 0;
  new.head = NULL;
  new.tail = NULL;
  return new;
}
