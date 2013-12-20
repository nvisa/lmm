/*
 * Copyright (C) 2010 Yusuf Caglar Akyuz
 *
 * Her hakki Bilkon Ltd. Sti.'ye  aittir.
 */

#include "hardwareoperations.h"
#include "debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/errno.h>
#if defined(__arm__)
#include <sys/mman.h>
#endif
#include <sys/ioctl.h>
#include <errno.h>

/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>*/

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
	__LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

#if defined(__arm__)
static void * memmap(off_t target) {
	int fd;
	void *map_base;

	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;

	//printf("/dev/mem opened.\n");

	/* Map one page */
	map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~MAP_MASK);

	if(map_base == (void *) -1) FATAL;

	//printf("Memory mapped at address %p.\n", map_base);

	close(fd);
	return (unsigned char *)map_base + (target & MAP_MASK);
}
#endif
bool HardwareOperations::writeRegister(unsigned int addr, unsigned int value)
{
#if defined(__arm__)
	unsigned int *reg = (unsigned int *)memmap(addr);
	if (!reg)
		return false;

	*reg = value;
	munmap(reg, MAP_SIZE);
	return true;
#else
	(void)addr;
	(void)value;
	return false;
#endif
}

HardwareOperations::HardwareOperations(QObject *parent) :
	QObject(parent)
{
}

int HardwareOperations::map(unsigned int addr)
{
	void *map_base;

	int fd;
	if((fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;

	/* Map one page */
	map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr & ~MAP_MASK);

	if(map_base == (void *) -1) {
		::close(fd);
		return -EINVAL;
	}

	::close(fd);
	mmapBase = (unsigned int *)map_base + (addr & MAP_MASK);
	return 0;
}

int HardwareOperations::unmap()
{
	return munmap(mmapBase, MAP_SIZE);
}

int HardwareOperations::write(unsigned int off, unsigned int value)
{
	mmapBase[off] = value;
	return 0;
}

uint HardwareOperations::read(unsigned int off)
{
	return mmapBase[off];
}

bool HardwareOperations::SetOsdTransparency(unsigned char trans)
{
	return SetOsdTransparency(trans, 0, 0, 800, 480);
}

bool HardwareOperations::fillFramebuffer(const char *fbfile, unsigned int value)
{
#if defined(__arm__)
	struct fb_var_screeninfo vScreenInfo;
	struct fb_fix_screeninfo fScreenInfo;
	unsigned int          *attrDisplay;
	int                      attrSize;
	int fd = open(fbfile, O_RDWR);
	if(fd < 0)
		return false;

	if (ioctl(fd, FBIOGET_FSCREENINFO, &fScreenInfo) == -1) {
		::close(fd);
		return false;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vScreenInfo) == -1) {
		::close(fd);
		return false;
	}
	attrSize = fScreenInfo.line_length * vScreenInfo.yres;
	/* Map the attribute window to this user space process */
	attrDisplay = (unsigned int *) mmap(NULL, attrSize,
										PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (attrDisplay == MAP_FAILED) {
		::close(fd);
		return false;
	}
	/* Fill the window with the new attribute value */
	for(int i = 0; i < attrSize / 4; i++) {
		attrDisplay[i] = value;
	}

	munmap(attrDisplay, attrSize);
	::close(fd);
	return true;
#else
	(void)fbfile;
	(void)value;
	return false;
#endif
}

bool HardwareOperations::SetOsdTransparency(unsigned char trans, unsigned int x,
											unsigned int y, unsigned int width, unsigned int height)
{
#if defined(__arm__)
	struct fb_var_screeninfo vScreenInfo;
	struct fb_fix_screeninfo fScreenInfo;
	unsigned short          *attrDisplay;
	int                      attrSize;
	/*
		 * Check parameters
		 */
	if(((x & 31) != 0) || ((width & 31) != 0))
		return false;
	int fd = open("/dev/fb2", O_RDWR);
	if(fd < 0)
		return false;// err;

	//err = ERR_IOCTL;
	if (ioctl(fd, FBIOGET_FSCREENINFO, &fScreenInfo) == -1) {
		::close(fd);
		return false;// err;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &vScreenInfo) == -1) {
		::close(fd);
		return false;// err;
	}
	attrSize = fScreenInfo.line_length * vScreenInfo.yres;
	/* Map the attribute window to this user space process */
	attrDisplay = (unsigned short *) mmap(NULL, attrSize,
										  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (attrDisplay == MAP_FAILED) {
		::close(fd);
		return false;
	}

	/* Fill the window with the new attribute value */
	if(height < vScreenInfo.yres) {
		for(unsigned int i = y; i < y + height; i++) {
			int start = fScreenInfo.line_length * i + x / 2;
			int size = (width + x) / 2 < fScreenInfo.line_length ?
						width / 2 : fScreenInfo.line_length - x / 2;
			memset(attrDisplay + start / 2, trans, size);
		}
	} else
		memset(attrDisplay, trans, attrSize);

	munmap(attrDisplay, attrSize);
	::close(fd);

	//mDebug("%s: x:%d y:%d width:%d height:%d trans:%d\n", __PRETTY_FUNCTION__, x, y, width, height, trans);
	return true;
#else
	(void)trans;
	(void)x;
	(void)y;
	(void)width;
	(void)height;
	return false;
#endif
}

struct vpbe_bitmap_blend_params {
	unsigned int colorkey;	/* color key to be blended */
	unsigned int enable_colorkeying;	/* enable color keying */
	unsigned int bf;	/* valid range from 0 to 7 only. */
};
#define FBIO_SET_BITMAP_BLEND_FACTOR	\
	_IOW('F', 0x31, struct vpbe_bitmap_blend_params)

bool HardwareOperations::blendOSD(bool blend, unsigned short colorKey)
{
	struct vpbe_bitmap_blend_params params;
	params.colorkey = colorKey;
	if (blend) {
		params.enable_colorkeying = 1;
		params.bf = 0;
	} else {
		params.enable_colorkeying = 0;
		params.bf = 7;
	}

	int fd = open("/dev/fb0", O_RDWR);
	if(fd < 0)
		return false;

	//err = ERR_IOCTL;
	if (ioctl(fd, FBIO_SET_BITMAP_BLEND_FACTOR, &params) == -1) {
		::close(fd);
		return false;// err;
	}

	::close(fd);
	return true;
}
