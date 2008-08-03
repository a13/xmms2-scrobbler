/*
 * Copyright (c) 2008 Tilman Sauerbeck (tilman at xmms org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>

#include "queue.h"

void
queue_init (Queue *q)
{
	q->head = q->tail = NULL;
}

void
queue_push (Queue *q, void *data)
{
	QueueItem *item;

	item = malloc (sizeof (QueueItem));

	item->next = NULL;
	item->data = data;

	if (q->tail)
		q->tail->next = item;

	q->tail = item;

	if (!q->head)
		q->head = item;
}

void *
queue_pop (Queue *q)
{
	QueueItem *item;
	void *data;

	if (!q->head)
		return NULL;

	item = q->head;
	data = item->data;

	q->head = q->head->next;

	/* if we just removed the last item in the queue,
	 * we also need to reset the tail pointer.
	 */
	if (!q->head)
		q->tail = NULL;

	free (item);

	return data;
}

void *
queue_peek (Queue *q)
{
	return q->head ? q->head->data : NULL;
}
