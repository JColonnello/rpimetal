#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
Copyright (c) 2014 Anders Kalør

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

typedef struct ring_buffer ring_buffer;

struct ring_buffer
{
	char *start;
	char *end;
	char *head;
	char *tail;
	bool empty;
};

#define CLIP(buffer, var) var -= var >= buffer->end ? buffer->end - buffer->start : 0

/**
 * @brief Initializes the ring buffer pointed to by <em>buffer</em>.
 * @param buffer The ring buffer to initialize.
 * @param buf The buffer allocated for the ringbuffer.
 * @param buf_size The size of the allocated ringbuffer.
 */
static inline void ring_buffer_init(ring_buffer *buffer, char *buf, size_t buf_size)
{
	buffer->start = buf;
	buffer->end = buf + buf_size;
	buffer->head = buf;
	buffer->tail = buf;
	buffer->empty = true;
}

/**
 * Returns the size of the ring buffer.
 * @param buffer The buffer whose size should be returned.
 * @return The size of the ring buffer.
 */
static inline size_t ring_buffer_size(ring_buffer *buffer)
{
	return buffer->end - buffer->start;
}

/**
 * Returns whether a ring buffer is empty.
 * @param buffer The buffer for which it should be returned whether it is empty.
 * @return true if empty; false otherwise.
 */
static inline bool ring_buffer_is_empty(ring_buffer *buffer)
{
	return buffer->empty;
}

/**
 * Returns whether a ring buffer is full.
 * @param buffer The buffer for which it should be returned whether it is full.
 * @return true if full; false otherwise.
 */
static inline bool ring_buffer_is_full(ring_buffer *buffer)
{
	return buffer->head == buffer->tail && !buffer->empty;
}

/**
 * Adds a byte to a ring buffer without checking if it is full.
 * This function should only be used if the caller is sure that the buffer is not full.
 * @param buffer The buffer in which the data should be placed.
 * @param data The byte to place.
 */
static inline void ring_buffer_queue_nc(ring_buffer *buffer, char data)
{
	*(buffer->tail++) = data;
	CLIP(buffer, buffer->tail);
	buffer->empty = false;
}

/**
 * Adds a byte to a ring buffer.
 * @param buffer The buffer in which the data should be placed.
 * @param data The byte to place.
 * @return true if the byte was added; false if the buffer is full.
 */
static inline bool ring_buffer_queue(ring_buffer *buffer, char data)
{
	if (ring_buffer_is_full(buffer))
		return false;

	*(buffer->tail++) = data;
	CLIP(buffer, buffer->tail);
	buffer->empty = false;
	return true;
}

/**
 * Returns the number of items in a ring buffer.
 * @param buffer The buffer for which the number of items should be returned.
 * @return The number of items in the ring buffer.
 */
static inline size_t ring_buffer_num_items(ring_buffer *buffer)
{
	if (buffer->empty)
		return 0;
	if (buffer->head < buffer->tail)
		return buffer->tail - buffer->head;
	return buffer->end - buffer->head + buffer->tail - buffer->start;
}

/**
 * Returns the remaining capacity of the ring buffer.
 * @param buffer The buffer whose capacity should be returned.
 * @return The capacity of the ring buffer.
 */
static inline size_t ring_buffer_capacity(ring_buffer *buffer)
{
	if (buffer->empty)
		return buffer->end - buffer->start;
	if (buffer->head > buffer->tail)
		return buffer->head - buffer->tail;
	return buffer->end - buffer->tail + buffer->head - buffer->start;
}

/**
 * Adds an array of bytes to a ring buffer.
 * @param buffer The buffer in which the data should be placed.
 * @param data A pointer to the array of bytes to place in the queue.
 * @param size The size of the array.
 */
static inline void ring_buffer_queue_arr(ring_buffer *buffer, const char *data, size_t size)
{
	size_t capacity = ring_buffer_capacity(buffer);

	// Check if the size to add is larger than the capacity of the buffer
	// If so, as the buffer can be overwritten, we just copy the last part of the data
	if (size > capacity)
	{
		data += size - capacity;
		size = capacity;
	}

	size_t freeSpace = buffer->end - buffer->tail;
	if (size > freeSpace)
	{
		// Copy until the end of the buffer
		memcpy(buffer->tail, data, freeSpace);
		// Copy the rest to the beginning of the buffer
		memcpy(buffer->start, data + freeSpace, size - freeSpace);
		buffer->tail = buffer->start + (size - freeSpace);
	}
	else
	{
		// Copy the data to the tail
		memcpy(buffer->tail, data, size);
		buffer->tail += size;
	}

	CLIP(buffer, buffer->tail);
	buffer->empty = false;
}

/**
 * Returns the oldest byte in a ring buffer without checking if it is empty.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the location at which the data should be placed.
 */
static inline char ring_buffer_dequeue_nc(ring_buffer *buffer)
{
	register char c = *(buffer->head++);
	CLIP(buffer, buffer->head);
	if (buffer->head == buffer->tail)
		buffer->empty = true; // Buffer is now empty
	return c;
}

/**
 * Returns the oldest byte in a ring buffer.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the location at which the data should be placed.
 * @return true if data was returned; false otherwise.
 */
static inline bool ring_buffer_dequeue(ring_buffer *buffer, char *data)
{
	if (buffer->empty)
		return false; // Buffer is empty

	*data = *(buffer->head++);
	CLIP(buffer, buffer->head);

	if (buffer->head == buffer->tail)
		buffer->empty = true; // Buffer is now empty
	return true;
}

/**
 * Returns the <em>len</em> oldest bytes in a ring buffer.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the array at which the data should be placed.
 * @param len The maximum number of bytes to return.
 * @return The number of bytes returned.
 */
static inline size_t ring_buffer_dequeue_arr(ring_buffer *buffer, char *data, size_t len)
{
	if (buffer->empty)
		return 0; // Buffer is empty

	size_t numItems = ring_buffer_num_items(buffer);
	size_t size = numItems < len ? numItems : len;
	size_t untilEnd = buffer->end - buffer->head;

	if (size > untilEnd)
	{
		// Copy until the end of the buffer
		memcpy(data, buffer->head, untilEnd);
		// Copy the rest from the beginning of the buffer
		memcpy(data + untilEnd, buffer->start, size - untilEnd);
		buffer->head = buffer->start + (size - untilEnd);
	}
	else
	{
		// Copy the data to the array
		memcpy(data, buffer->head, size);
		buffer->head += size;
	}

	CLIP(buffer, buffer->head);
	return size;
}

/**
 * Peeks a ring buffer, i.e. returns an element without removing it.
 * @param buffer The buffer from which the data should be returned.
 * @param data A pointer to the location at which the data should be placed.
 * @param index The index to peek.
 * @return true if data was returned; false otherwise.
 */
static inline bool ring_buffer_peek(ring_buffer *buffer, char *data, size_t index)
{
	if (index >= ring_buffer_num_items(buffer))
		return false; // Index out of bounds

	char *pos = buffer->head + index;
	CLIP(buffer, pos);
	*data = *pos;
	return true;
}

#undef CLIP