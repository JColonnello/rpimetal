#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * Registers a channel for sending and receiving messages
 * @param channel The channel number
 * @param buffer_size The size in bytes for the receive and transmit buffers
 * @param complete true if it should flush only when a message is complete, false otherwise
 * @return true if completed succesfully, false otherwise
 */
bool mux_channel_add(int16_t channel, size_t buffer_size, bool complete);
/**
 * Unregisters a channel for sending and receiving messages
 * @param channel The channel number
 * @return true if the channel exists and was deleted, false otherwise
 */
bool mux_channel_remove(int16_t channel);
/**
 * Sends a message through a channel
 * @param channel The channel number
 * @param data The pointer to the data
 * @param size The length of the data in bytes
 * @return The amount of bytes actually sent
 */
size_t mux_send(int16_t channel, const char *data, size_t size);
/**
 * Receive a message from a channel
 * @param channel The channel number
 * @param buffer The pointer to the buffer in which to save the data
 * @param size The amount of bytes to read at most
 * @return The amount of bytes received
 */
size_t mux_recv(int16_t channel, char *buffer, size_t size);
/**
 * Sets the callback for when data is available to read
 * @param channel The channel number
 * @param handler The callback function
 * @return true if the callback was set successfully, false otherwise
 */
bool mux_set_rx_callback(int16_t channel, void (*handler)(size_t available));
/**
 * Sets the callback for when data can be written
 * @param channel The channel number
 * @param handler The callback function
 * @return true if the callback was set successfully, false otherwise
 */
bool mux_set_tx_callback(int16_t channel, void (*handler)(size_t available));
/**
 * Gets the amount of data available to read
 * @param channel The channel number
 * @return The amount of data available to read
 */
size_t mux_rx_available(int16_t channel);
/**
 * Gets the amount of space available to write
 * @param channel The channel number
 * @return The amount of space available to write
 */
size_t mux_tx_available(int16_t channel);
/**
 * Buffer size for stdio channels (0 and 1)
 */
extern size_t mux_stdio_buffer_size;

void mux_process_input(size_t available);
void mux_process_output(size_t available);
