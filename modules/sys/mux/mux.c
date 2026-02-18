#include "arm/irq.h"
#include "drivers/uart.h"
#include <attrib.h>
#include <ringbuffer.h>
#include <sglib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mux.h>

typedef struct channel_tree
{
	int16_t channel;
	ring_buffer rx, tx;
	bool complete;
	void (*rx_callback)(size_t available);
	void (*tx_callback)(size_t available);

	char color;
	struct channel_tree *left, *right;
} channel_tree;

#define TREE_COMPARE(a, b) ((a)->channel - (b)->channel)
SGLIB_DEFINE_RBTREE_PROTOTYPES(channel_tree, left, right, color, TREE_COMPARE)
SGLIB_DEFINE_RBTREE_FUNCTIONS(channel_tree, left, right, color, TREE_COMPARE)

#define MAX_MESSAGE_SIZE 246
#define HEADER_SIZE 6
#define LENGTH_MULT 12
_Static_assert(
	(HEADER_SIZE + MAX_MESSAGE_SIZE) % LENGTH_MULT == 0,
	"Header size + message size must be multiple of length alignment"
);

static channel_tree *channels = NULL;

static char rx_mesg_buf[MAX_MESSAGE_SIZE + HEADER_SIZE];
static unsigned rx_mesg_bytes;

enum uart_mode uart_mode = UART_MODE_MUXED;
weak size_t mux_stdio_buffer_size = 1024;

constructor void callbacks()
{
	mux_channel_add(0, mux_stdio_buffer_size, false); // stdin/stdout
	mux_channel_add(1, mux_stdio_buffer_size, false); // stderr
	uart_set_rx_callback(mux_process_input);
	uart_set_tx_callback(mux_process_output);
}

static channel_tree *search_channel(int16_t channel)
{
	channel_tree *search = &(channel_tree){.channel = channel};
	return sglib_channel_tree_find_member(channels, search);
}

static uint16_t calculate_checksum(const char *data, size_t length)
{
	uint16_t checksum = 0;
	for (size_t i = 0; i < length; i++)
	{
		checksum += (uint8_t)data[i];
	}
	return checksum;
}

void mux_process_input(size_t available)
{
	if (available == 0)
		available = uart_rx_available();
	for (;;)
	{
		int remaining = HEADER_SIZE - rx_mesg_bytes;

		if (remaining > 0)
		{
			if (available >= remaining)
			{
				size_t read = uart_recv_buffer(&rx_mesg_buf[rx_mesg_bytes], remaining);
				rx_mesg_bytes += read;
				available -= read;
			}
			else
				return;
		}
		int16_t curr_mesg_channel = *(int16_t *)rx_mesg_buf;
		uint16_t curr_mesg_len = *(uint16_t *)(rx_mesg_buf + 2);
		uint16_t curr_mesg_checksum = *(uint16_t *)(rx_mesg_buf + 4);
		if (curr_mesg_len > MAX_MESSAGE_SIZE)
		{
			fprintf(stderr, "Received message length %d exceeds maximum %d\n", curr_mesg_len, MAX_MESSAGE_SIZE);
			exit(1);
		}

		remaining = curr_mesg_len + HEADER_SIZE + LENGTH_MULT - 1;
		remaining = remaining / LENGTH_MULT * LENGTH_MULT; // Round up to multiple of LENGTH_MULT
		remaining -= rx_mesg_bytes;
		if (remaining > 0)
		{
			if (available >= remaining)
			{
				size_t read = uart_recv_buffer(&rx_mesg_buf[rx_mesg_bytes], remaining);
				rx_mesg_bytes += read;
				available -= read;
			}
			else
				return;
		}

		channel_tree *channel = search_channel(curr_mesg_channel);
		uint16_t calculated_checksum = calculate_checksum(&rx_mesg_buf[HEADER_SIZE], curr_mesg_len);
		if (calculated_checksum != curr_mesg_checksum)
		{
			fprintf(
				stderr,
				"Checksum mismatch for channel %d: calculated 0x%04x, expected 0x%04x\n",
				curr_mesg_channel,
				calculated_checksum,
				curr_mesg_checksum
			);
		}

		if (channel == NULL)
		{
			rx_mesg_bytes = 0;
			continue;
		}
		if (ring_buffer_capacity(&channel->rx) >= curr_mesg_len)
		{
			ring_buffer_queue_arr(&channel->rx, &rx_mesg_buf[HEADER_SIZE], curr_mesg_len);
			rx_mesg_bytes = 0;
			if (channel->rx_callback)
				channel->rx_callback(ring_buffer_num_items(&channel->rx));
			continue;
		}
		else
			return;
	}
}

static struct sglib_channel_tree_iterator tx_it;
void mux_process_output(size_t available)
{
	channel_tree *tx_next;
	if (available == 0)
		available = uart_tx_available();

	for (tx_next = sglib_channel_tree_it_current(&tx_it); tx_next; tx_next = sglib_channel_tree_it_next(&tx_it))
	{
		if (ring_buffer_is_empty(&tx_next->tx))
			continue;
		static char tx_buf[MAX_MESSAGE_SIZE + HEADER_SIZE];
		int16_t curr_mesg_channel = tx_next->channel;
		uint16_t curr_mesg_len = ring_buffer_num_items(&tx_next->tx);
		if (curr_mesg_len > MAX_MESSAGE_SIZE)
			curr_mesg_len = MAX_MESSAGE_SIZE;

		if (available < curr_mesg_len + HEADER_SIZE)
			return;
		ring_buffer_dequeue_arr(&tx_next->tx, tx_buf + HEADER_SIZE, curr_mesg_len);
		*(int16_t *)tx_buf = curr_mesg_channel;
		*(uint16_t *)(tx_buf + 2) = curr_mesg_len;
		*(uint16_t *)(tx_buf + 4) = calculate_checksum(tx_buf + HEADER_SIZE, curr_mesg_len);
		if (tx_next->tx_callback)
			tx_next->tx_callback(ring_buffer_capacity(&tx_next->tx));
		unsigned to_write = curr_mesg_len + HEADER_SIZE + LENGTH_MULT - 1;
		to_write = to_write / LENGTH_MULT * LENGTH_MULT;
		available -= uart_send_buffer(tx_buf, to_write);
	}
	tx_next = sglib_channel_tree_it_init_inorder(&tx_it, channels);
}

bool mux_channel_add(int16_t channel, size_t buffer_size, bool complete)
{
	if (buffer_size < MAX_MESSAGE_SIZE)
		return false; // Buffer size too small
	channel_tree *new_channel = search_channel(channel);
	if (new_channel)
		return false; // Channel already exists

	new_channel = malloc(sizeof(channel_tree));
	*new_channel = (channel_tree){
		.channel = channel,
		.complete = complete,
	};
	ring_buffer_init(&new_channel->rx, malloc(buffer_size), buffer_size);
	ring_buffer_init(&new_channel->tx, malloc(buffer_size), buffer_size);

	sglib_channel_tree_add(&channels, new_channel);
	sglib_channel_tree_it_init_inorder(&tx_it, channels);

	return true;
}

bool mux_channel_remove(int16_t channel)
{
	channel_tree *channel_to_remove = &(channel_tree){.channel = channel};
	if (!sglib_channel_tree_delete_if_member(&channels, channel_to_remove, &channel_to_remove))
		return false; // Channel does not exist
	sglib_channel_tree_it_init_inorder(&tx_it, channels);

	free(ring_buffer_get_raw_buffer(&channel_to_remove->rx));
	free(ring_buffer_get_raw_buffer(&channel_to_remove->tx));
	free(channel_to_remove);

	return true;
}

size_t mux_send(int16_t channel, const char *data, size_t size)
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return 0; // Channel does not exist

	if (channel_node->complete && ring_buffer_capacity(&channel_node->tx) < size)
		return 0; // Not enough space in the transmit buffer

	size_t written = ring_buffer_queue_arr(&channel_node->tx, data, size);
	if (written)
		uart_send_buffer(NULL, 0); // Flush output
	return written;
}

size_t mux_recv(int16_t channel, char *buffer, size_t size)
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return 0; // Channel does not exist

	size_t read = ring_buffer_dequeue_arr(&channel_node->rx, buffer, size);
	if (read)
	{
		irq_disable();
		mux_process_input(0);
		irq_enable();
	}
	return read;
}

bool mux_set_rx_callback(int16_t channel, void (*handler)(size_t available))
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return false; // Channel does not exist

	channel_node->rx_callback = handler;
	return true;
}

bool mux_set_tx_callback(int16_t channel, void (*handler)(size_t available))
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return false; // Channel does not exist

	channel_node->tx_callback = handler;
	return true;
}

size_t mux_rx_available(int16_t channel)
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return 0; // Channel does not exist

	return ring_buffer_num_items(&channel_node->rx);
}

size_t mux_tx_available(int16_t channel)
{
	channel_tree *channel_node = search_channel(channel);
	if (!channel_node)
		return 0; // Channel does not exist

	return ring_buffer_capacity(&channel_node->tx);
}
