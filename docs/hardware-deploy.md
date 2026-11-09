# Running on real hardware

Besides testing in QEMU, you can deploy your projects on a real Raspberry Pi. There are multiple ways to achieve this

## Materials required

* Raspberry Pi 3B or 3B+
* Micro-USB cable and power supply OR a [PoE hat](https://www.raspberrypi.com/products/poe-hat/) and a [power sourcing network equipment](https://en.wikipedia.org/wiki/Power_over_Ethernet#Power_sourcing_equipment)
* UART-to-serial adapter. An adapter based on the CH340 chip is recommended
* Some female-female jumper cables
* MicroSD card and adapter (for writting from the development computer)
* [Raspberry firmware files](https://github.com/raspberrypi/firmware/tree/master/boot)

## Method 1: PXE (recommended)

### Extra materials

* Ethernet cable
* A computer running Linux, with Docker preinstalled
* Network equipment with an ethernet port (or the computer itself)

The remote deployment server only works in Linux for now, due to issues with UDP in WSL. It's not been tested in MacOS. If you already use rpimetal on Linux you can  run the deployment server on the same computer

The docker compose definition found in `remote-server/` contains various services required for remotely deploying applications over PXE

* DHCP server for initial PXE signaling
* TFTP server to serve the firmware and application files
* rsync server to remotely send application files from RPiMetal
* A service that configures the USB-UART adapter and makes it available over TCP

### Procedure

1. Connect the ethernet cable to the RPi. If connecting to some other network equipment, it must be configured to forward all traffic from that port to the deployment server

1. Connect the UART adapter to the correct pins in the RPi header: 
![GPIO pin numbering](https://www.raspberrypi.com/documentation/computers/images/GPIO-Pinout-Diagram-2.png?hash=df7d7847c57a1ca6d5b2617695de6d46)
	* Ground to adapter GND
	* GPIO 14 to adapter RX
	* GPIO 15 to adapter TX

1. Download the firmware and copy these files to the output/ directory in RPiMetal
	* bootcode.bin
	* fixup.dat
	* start.elf
	* bcm2710-rpi-3-b.dtb
	* overlays/disable-bt.dtbo

1. Using the microSD adapter
	1. Format and create a FAT32 partition on the SD card
	1. Copy only the bootcode.bin file to the root of the partition
	1. Insert it in the RPi

1. Copy the remote-server directory to the remote server computer. Run the docker compose, modifing the values if needed

	```bash
	cd remote-server
	docker compose up -d
	```

	> If using the ethernet port on the computer, configure its network manager to stop managing the interface and assign an static IP (`192.168.97.2` by default). The included script [dhcp.sh](../remote-server/dhcp.sh) shows an example for NetworkManager

1. In RPiMetal, configure the `sync` task as the default built task and run it (Ctrl+Shift+B), or run `make all sync` in a new terminal. This will build the project and send all the firmware files and the compiled program to the remote server

1. Connect the RPiMetal terminal to the RPi UART, by running this command in a new command terminal inside RPiMetal:

	`socat TCP-CONNECT:[remote server IP]:4000 TCP-CONNECT:localhost:4444`

1. Connect the RPi to a power supply

Repeat the last 3 steps to iterate during development

---

If all steps are performed correctly, the RPi will:

1. Boot in a [special mode](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#special-bootcode-bin-only-boot-mode)
1. Obtain an IP address for itself from the DHCP server, and the address for the TFTP server
1. Get the firmware files and the application (kernel8.img) and load them

## Method 2: Manual copy

The firmware and application can be copied directly to the microSD card

1. Using the microSD adapter
	1. Format and create a FAT32 partition on the SD card
	1. Copy the firmware file and the built application to the root of the partition
		* bootcode.bin
		* fixup.dat
		* start.elf
		* bcm2710-rpi-3-b.dtb
		* overlays/disable-bt.dtbo
		* kernel8.img
		* config.txt
	1. Insert it in the RPi
1. Configure and connect the UART adapter to the RPiMetal terminal

	`socat TCP-CONNECT:localhost:4444 /dev/ttyUSB0,b115200,raw,onlcr=0,echo=0,echoctl=0`
1. Connect the RPi to a power supply
