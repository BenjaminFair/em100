/*
 * Copyright 2012-2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "em100.h"

/* SPI flash related operations */

int get_spi_flash_id(struct em100 *em100)
{
	unsigned char cmd[16];
	unsigned char data[512];
	memset(cmd, 0, 16);
	cmd[0] = 0x30; /* Get SPI flash ID */
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	int len = get_response(em100->dev, data, 512);
	if (len == 3) {
		int id = (data[0] << 16) | (data[1] << 8) | data[2];
		return id;
	}
	return 0;
}

int erase_spi_flash(struct em100 *em100)
{
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x31; /* Erase SPI flash */
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	/* Specification says to wait 5s before
	 * issuing another USB command
	 */
	sleep(5);
	return 1;
}

int poll_spi_flash_status(struct em100 *em100)
{
	unsigned char cmd[16];
	unsigned char data[1];
	memset(cmd, 0, 16);
	cmd[0] = 0x32; /* Poll SPI flash status */
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	int len = get_response(em100->dev, data, 1);
	if ((len == 1) && (data[0] == 1)) {
		/* ready */
		return 1;
	}
	/* busy (or read unsuccessful) */
	return 0;
}

/**
 * read_spi_flash_page: fetch SPI flash page
 * @param em100: initialized em100 device structure
 *
 * out(16 bytes): 0x33 addr addr addr .. 0
 * in(len + 255 bytes): 0xff ?? serno_lo serno_hi ?? ?? .. ??
 */
int read_spi_flash_page(struct em100 *em100, int addr, unsigned char *blk)
{
	unsigned char cmd[16];
	unsigned char data[256];
	memset(cmd, 0, 16);
	cmd[0] = 0x33; /* read configuration block */
	cmd[1] = (addr >> 16) & 0xff;
	cmd[2] = (addr >> 8)  & 0xff;
	cmd[3] = addr & 0xff;
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	int len = get_response(em100->dev, data, 256);

	if (len == 256) {
		memcpy(blk, data, 256);
		return 1;
	}
	return 0;
}

int write_spi_flash_page(struct em100 *em100, unsigned char *data, int address)
{
	int length = 256;
	int actual;
	int bytes_sent=0;
	int bytes_left;
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x34; /* host-to-em100 eeprom data */
	cmd[2] = (address >> 16) & 0xff;
	cmd[3] = (address >> 8) & 0xff;
	cmd[4] = address & 0xff;

	if (!send_cmd(em100->dev, cmd)) {
		printf("error initiating host-to-em100 transfer.\n");
		return 0;
	}

	while ( bytes_sent < length) {
		actual = 0;

		bytes_left = length - bytes_sent;

		libusb_bulk_transfer(em100->dev, 1 | LIBUSB_ENDPOINT_OUT,
			data + bytes_sent, bytes_left, &actual, BULK_SEND_TIMEOUT);
		bytes_sent += actual;
		if (actual < bytes_left) {
			printf("Tried sending %d bytes, sent %d\n", bytes_left, actual);
			break;
		}

		printf("Sent %d bytes of %d\n", bytes_sent, length);
	}

	printf ("Transfer %s\n",bytes_sent == length ? "Succeeded" : "Failed");
	return (bytes_sent == length);
}

/* SPI HyperTerminal related operations */

/**
 * read_ht_register: Read HT registers
 * @param em100: initialized em100 device structure
 *
 * out(2 bytes): 0x50 RegAddr .. 0
 * in(len + 1 byte): 0x02 val
 */
int read_ht_register(struct em100 *em100, int reg, uint8_t *val)
{
	unsigned char cmd[16];
	unsigned char data[2];

	memset(cmd, 0, 16);
	cmd[0] = 0x50; /* read fpga register */
	cmd[1] = reg;
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	int len = get_response(em100->dev, data, 2);
	if ((len == 2) && (data[0] == 1)) {
		*val = data[1];
		return 1;
	}
	return 0;
}

/**
 * write_ht_register: Write HT registers
 * @param em100: initialized em100 device structure
 *
 * out(3 bytes): 0x51 RegAddr Val .. 0
 */
int write_ht_register(struct em100 *em100, int reg, uint8_t val)
{
	unsigned char cmd[16];
	memset(cmd, 0, 16);
	cmd[0] = 0x51; /* write fpga registers */
	cmd[1] = reg;
	cmd[2] = val;
	if (!send_cmd(em100->dev, cmd)) {
		return 0;
	}
	return 1;
}
