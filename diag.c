/*
 * diag.c
 *
 * Copyright (C) 2017, Viosoft Corp.
 */

#include "util.h"
#include <stdio.h>

#define HTIF_DEV_SHIFT      (56)
#define HTIF_CMD_SHIFT      (48)

extern void printstr(const char *);

#define HTIF_CMD_READ       (0x00UL)
#define HTIF_CMD_WRITE      (0x01UL)
#define HTIF_CMD_IDENTITY   (0xFFUL)

#define HTIF_NR_DEV         (256UL)

#define ETH_MAX_DATA_SIZE	(1472)
//#define TEST_DISK
#define TEST_ETH

#define mb()	__asm__ __volatile__ ("fence" : : : "memory")

struct request_t
{
  uint64_t addr;	// Address on the emulator
  uint64_t offset;	// Position on the disk
  uint64_t size;	// Length of the data will be read/written
  uint64_t tag;		// For verifying
};

static inline void htif_tohost(unsigned long dev,
			       unsigned long cmd, unsigned long data)
{
	unsigned long packet;
	packet = (dev << HTIF_DEV_SHIFT) | (cmd << HTIF_CMD_SHIFT) | data;
	while (swap_csr(tohost, packet) != 0);
}

static inline unsigned long htif_fromhost(void)
{
	unsigned long data;
	while ((data = swap_csr(fromhost, 0)) == 0);
	return data;
}

static void test_htif_console(void)
{
	htif_tohost(1, HTIF_CMD_WRITE, 'h');
	htif_tohost(1, HTIF_CMD_WRITE, 'e');
	htif_tohost(1, HTIF_CMD_WRITE, 'l');
	htif_tohost(1, HTIF_CMD_WRITE, 'l');
	htif_tohost(1, HTIF_CMD_WRITE, 'o');
	htif_tohost(1, HTIF_CMD_WRITE, '\n');
}

static void htif_bus_enumerate(void)
{
	char buf[64] __attribute__((aligned(64)));
	char str[128];
	unsigned int minor, code;

	printstr("enumerate:\n");
	for (minor = 0; minor < HTIF_NR_DEV; minor++) {
		htif_tohost(minor, HTIF_CMD_IDENTITY,
			    ((unsigned long) buf << 8) | 0xff);
		htif_fromhost();


		if (buf[0] != '\0') {
			sprintf(str, "dev%d = %s\n", minor, buf);
			printstr(str);
		}
	}
}

static char get_device_id(char *dev_name)
{
	char buf[64] __attribute__((aligned(64)));
	unsigned int id;

	for (id = 0; id < HTIF_NR_DEV; id++) {
		mb();
		// left shift 8 bits for request's type which includes: HTIF_CMD_IDENTITY to call device's identity and HTIF_CMD_* for getting command name
		htif_tohost(id, HTIF_CMD_IDENTITY,
	    		((unsigned long) buf << 8) | HTIF_CMD_IDENTITY);
		htif_fromhost();
		mb();

		if (buf[0] != '\0' && strstr(buf, dev_name)) {
			return id;
		}
	}
	return -1;
}

static inline unsigned long htif_disk_transfer(char dev, char cmd, unsigned long src_addr,
							unsigned long dst_addr, unsigned long size)
{
	unsigned long tag = 0;
	struct request_t req __attribute__((aligned(64)));

	// Initialize a request
	if (cmd == HTIF_CMD_WRITE)
	{
		req.addr = src_addr;
		req.offset = dst_addr;
	}
	else if (cmd == HTIF_CMD_READ)
	{
		req.addr = dst_addr;
		req.offset = src_addr;
	}
	req.size = size;
	req.tag = 1;

	// Send the request to the host side
	mb();
	htif_tohost(dev, cmd, &req);
	tag = htif_fromhost();
	mb();

	return tag;
}

int main(int argc, char **argv)
{
	char str[128] __attribute__((aligned(64)));
	char dev_id;
	char packet[ETH_MAX_DATA_SIZE] __attribute__((aligned(64))) =
		"08002779848fa0651895c8e208004520003706ab40003606f61417636e40c0a80196c355975ba0fb66e2c921d60780180039b53600000101080a665e1b3100060ac6574c94";
	char src_wdata[64] __attribute__((aligned(64))) = "Test disk devices: OK";
	char rdata[ETH_MAX_DATA_SIZE] __attribute__((aligned(64))) = "Before read";
	int packet_len;
	unsigned long dst_wdata = 0x1000;	// The position (on the disk) where content of the src_wdata will be written into
	unsigned long tag;

	printstr("htifdiag\n");
	test_htif_console();
	htif_bus_enumerate();

	#ifdef TEST_DISK

	printstr("\n***TESTING DISK DEVICE***\n");
	dev_id = get_device_id("disk");
	sprintf(str, "Device ID of the \"disk\" is %d\n", dev_id);
	printstr(str);

	sprintf(str, "Writing \"%s\" into 0x%lx of the disk ...\n", src_wdata, dst_wdata);
	printstr(str);
	tag = htif_disk_transfer (dev_id, HTIF_CMD_WRITE, src_wdata, dst_wdata, strlen(src_wdata));

	sprintf(str, "Getting data from 0x%lx of the disk ...\n", dst_wdata);
	printstr(str);
	sprintf(str, "Before reading: \"%s\"\n", rdata);
	printstr(str);
	tag = htif_disk_transfer (dev_id, HTIF_CMD_READ, dst_wdata, rdata, strlen(src_wdata));
	sprintf(str, "After reading: \"%s\"\n\n", rdata);
	printstr(str);

	#endif // TEST_DISK

	#ifdef TEST_ETH

	printstr("\n***TESTING VIOSOFT_ETH DEVICE***\n");
	dev_id = get_device_id("viosoft_eth");
	sprintf(str, "Device ID of the \"viosoft_eth\" is %d\n", dev_id);
	printstr(str);

	// Send a packet to the host
	tag = htif_disk_transfer (dev_id, HTIF_CMD_WRITE, packet, 0, sizeof(packet));

	// Receive a packet
	sprintf(str, "Before reading: \"%s\"\n", rdata);
	printstr(str);
	tag = htif_disk_transfer (dev_id, HTIF_CMD_READ, 0, rdata, 0);
	// Get size of the packet
	packet_len = ((int *)rdata)[0];
	// Show the received packet
	memset(str, 0, strlen(str));
	printstr("After reading: ");
	for (int k =0; k < packet_len; k++)
	{
		sprintf(str, "%s%02x", str, rdata[k+sizeof(packet_len)]);
	}
	sprintf(str, "%s\n", str);
	printstr(str);

	#endif // TEST_ETH

	return 0;
}
