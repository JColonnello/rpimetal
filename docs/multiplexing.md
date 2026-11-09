# Serial Multiplexing Protocol

RPiMetal includes a serial multiplexing protocol that allows multiple channels of communication over a single UART connection. This enables separate streams for stdout, stderr, debug output, file transfer, and more — all through a single serial port.

## Overview

In **plain mode**, the UART transmits raw ASCII text directly. This is simple but limits you to a single bidirectional stream.

In **muxed mode**, data is encapsulated in packets with channel identifiers, allowing multiple logical channels to share the same physical connection.

```mermaid
---
config:
 flowchart:
  curve: linear
---
graph TB
	subgraph Raspberry Pi
	c0["Channel 0
	stdout"]
	c1["Channel 1
	stderr"]
	c2["Channel 2
	debug"]
	c0 & c1 & c2 --> m
	m[MUX]-->u
	end
	subgraph host [Host]
	u[UART]-->d
	d[DEMUX]
	d --> n0 & n1 & n2
	n0["nc :4440
	stdout"]
	n1["nc :4441
	stderr"]
	n2["nc :4442
	debug"]
	end
```

## Protocol Specification

### Packet Format

Each packet consists of a 6-byte header followed by the message payload. All header fields are little-endian:

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Channel | 2 bytes | `int16_t` (signed) | Channel identifier. Negative values reserved for control. |
| Length | 2 bytes | `uint16_t` (unsigned) | Payload length in bytes (0-246) |
| Checksum | 2 bytes | `uint16_t` (unsigned) | Checksum of the message payload |
| Payload | 0-246 bytes | raw data | The message content |
| Padding | 0-11 bytes | unspecified | Padding to align total packet size to 12-byte boundary. Receivers ignore its contents. |

Example for a 13 byte payload:
```mermaid
packet
0-1: "Channel"
2-3: "Length"
4-5: "Checksum"
6-18: "Payload"
19-23: "Padding"
```

### Checksum

The checksum is the sum of all payload bytes, each taken as an unsigned 8-bit value, truncated to 16 bits. The header and padding are not included.

```c
uint16_t checksum = 0;
for (size_t i = 0; i < length; i++)
    checksum += (uint8_t)payload[i];
```

On a mismatch, both the kernel and the host multiplexer print a warning to stderr, but the packet is still delivered.

### Alignment Rule

The total packet size (header + payload + padding) must be a multiple of `LENGTH_MULT` (12 bytes):

```
total_size = HEADER_SIZE + length + padding
total_size % LENGTH_MULT == 0
```

Both the kernel mux layer and the host multiplexer add the padding automatically, so application code can send payloads of any length.

Examples:
- Payload of 4 bytes: `6 + 4 = 10` → 2 bytes padding → 12 bytes total
- Payload of 10 bytes: `6 + 10 = 16` → 8 bytes padding → 24 bytes total
- Payload of 246 bytes: `6 + 246 = 252` → no padding needed

### Channel Conventions

| Channel | Purpose |
|---------|---------|
| 0 | Standard output (stdout) |
| 1 | Standard error (stderr) |
| 2+ | User-defined channels |
| Negative | Reserved for control/metadata |

`INT16_MIN` (-32768) is used to exit muxed mode: `uart_plain_mode()` sends it with length 0, and the host multiplexer exits when it receives it.

## Kernel-Side API

The multiplexing module is located in `modules/sys/mux/` and provides the following API (from `include/sys/mux.h`):

### Channel Management

```c
// Register a new channel with specified buffer size
bool mux_channel_add(int16_t channel, size_t buffer_size, bool complete);

// Unregister a channel
bool mux_channel_remove(int16_t channel);
```

The `complete` parameter controls flushing behavior:
- `true`: Only flush when a complete message is received
- `false`: Flush data as it becomes available

### Sending and Receiving

```c
// Send data on a channel (returns bytes actually sent)
size_t mux_send(int16_t channel, const char *data, size_t size);

// Receive data from a channel (returns bytes received)
size_t mux_recv(int16_t channel, char *buffer, size_t size);

// Check available data
size_t mux_rx_available(int16_t channel);
size_t mux_tx_available(int16_t channel);
```

### Callbacks

```c
// Called when data is available to read
bool mux_set_rx_callback(int16_t channel, void (*handler)(size_t available));

// Called when space is available to write
bool mux_set_tx_callback(int16_t channel, void (*handler)(size_t available));
```

### Example Usage

```c
#include <sys/mux.h>

void my_debug_handler(size_t available) {
    char buf[256];
    size_t n = mux_recv(2, buf, sizeof(buf));
    // Process debug commands...
}

int kernel_start() {
    // Channel 0 and 1 are auto-registered for stdio
    
    // Add a custom debug channel
    mux_channel_add(2, 512, false);
    mux_set_rx_callback(2, my_debug_handler);
    
    // Send data
    mux_send(0, "Hello stdout!\n", 14);
    mux_send(1, "Hello stderr!\n", 14);
    mux_send(2, "Debug message\n", 14);
    
    return 0;
}
```

## Host-Side Demultiplexer

The `multiplex` tool (`toolchain/multiplex.c`) runs on the host and demultiplexes the serial stream into separate channels.

### Building

The tool is built automatically by `make mux-tcp`, or explicitly with:
```bash
make multiplex
```

Or manually:
```bash
gcc -g -o multiplex toolchain/multiplex.c
```

Either way the binary ends up at `./multiplex` in the repo root.

### Configuration File

The multiplexer reads a configuration file that maps channels to commands. Each line has the format:

```
<channel>,<command>
```

Example `config-mult.txt`:
```
0,nc localhost 4440
1,nc localhost 4441
```

This maps:
- Channel 0 → netcat connecting to port 4440
- Channel 1 → netcat connecting to port 4441

### Running

Start the multiplexer with socat to bridge serial/TCP:

```bash
make mux-tcp
```

This runs:
```bash
socat TCP-LISTEN:4444,reuseaddr,fork SYSTEM:'(./multiplex config-mult.txt; nc -N localhost 4440; echo Terminated >&2)',nofork
```

When `multiplex` exits (for example after the kernel switches back to plain mode), the serial stream falls back to a plain `nc` on port 4440, so output keeps reaching the stdout terminal.

The commands in `config-mult.txt` connect out to ports 4440 and 4441, so each channel needs a listener. Start them before running QEMU, since `multiplex` spawns the commands as soon as QEMU connects:
```bash
# Terminal 1 - stdout
nc -lkvp 4440

# Terminal 2 - stderr
nc -lkvp 4441
```

### How It Works

1. QEMU connects its serial port to TCP port 4444
2. `socat` bridges port 4444 to the `multiplex` process
3. `multiplex` spawns the command for every channel in the config file at startup
4. It reads packets from stdin (the serial stream), and for each one:
   - Parses the channel, length and checksum from the header
   - Looks up the command for that channel (unknown channels are skipped with a warning)
   - Verifies the checksum and forwards the payload to that command's stdin
5. Output from commands is read, packaged into mux packets, and sent to stdout

## VS Code Terminal Integration

The dev container comes with preconfigured terminal layouts:

### Plain Mode Terminals
- **stdout**: Direct connection to QEMU serial port

### Muxed Mode Terminals
- **Muxer**: Runs the multiplexer
- **Channel 0**: stdout via mux
- **Channel 1**: stderr via mux

Switch between modes using the **Terminal Keeper** extension in the VS Code sidebar.

## Implementing Mux Support in Your Program

### Minimal Example (Plain Mode)

For simple programs, skip muxing entirely:

```c
#include <drivers/simple-uart.h>

int kernel_start() {
    uart_send_string("Hello, plain mode!\n");
    return 0;
}
```

Use with `bootloaders/linked` and the plain terminal.

### Muxed Example

For programs using multiplexing:

```c
#include <sys/mux.h>
#include <sys/printf.h>

static void putc_mux(void *p, char c) {
    mux_send(0, &c, 1);
}

int kernel_start() {
    // printf will output to channel 0
    init_printf(0, putc_mux);
    
    printf("This goes to stdout (channel 0)\n");
    mux_send(1, "This goes to stderr (channel 1)\n", 32);
    
    return 0;
}
```

Ensure your kernel includes the `sys/mux` module and use with the muxed terminal configuration.

## Troubleshooting

### No Output in Muxed Mode

1. Verify the multiplexer is running: `make mux-tcp`
2. Check `config-mult.txt` has correct channel mappings
3. Ensure your kernel includes `sys/mux` module
4. Verify UART module is initialized before mux

### Garbled Output

1. Check baud rate matches (default: 921600)
2. Restart the host-side multiplexer

### Channel Not Found

The multiplexer prints warnings for unknown channels:
```
Unknown channel 5, skipping packet
```

Add the channel to `config-mult.txt` or use an existing channel number.
