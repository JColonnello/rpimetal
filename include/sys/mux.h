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
size_t mux_read(int16_t channel, char *buffer, size_t size);

void mux_process_input(size_t available);
void mux_process_output(size_t available);
