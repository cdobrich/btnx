/*
 * Simple hack to control the wheel of Logitech's MX-Revolution mouse.
 *
 * Requires hiddev.
 *
 * Written November 2006 by E. Toernig's bonobo - no copyrights.
 *
 * Contact: Edgar Toernig <froese@gmx.de>
 * 
 * Modifications for integration into btnx by Olli Salonen, October 2007.
 *
 * Discovered commands:
 * (all numbers in hex, FS=free-spinning mode, CC=click-to-click mode):
 *   6 byte commands send with report ID 10:
 *   01 80 56 z1 00 00	immediate FS
 *   01 80 56 z2 00 00	immediate CC
 *   01 80 56 03 00 00	FS when wheel is moved
 *   01 80 56 04 00 00	CC when wheel is moved
 *   01 80 56 z5 xx yy	CC and switch to FS when wheel is rotated at given
 *			speed; xx = up-speed, yy = down-speed
 *			(speed in something like clicks per second, 1-50,
 *			 0 = previously set speed)
 *   01 80 56 06 00 00	?
 *   01 80 56 z7 xy 00	FS with button x, CC with button y.
 *   01 80 56 z8 0x 00	toggle FS/CC with button x; same result as 07 xx 00.
 *
 * If z=0 switch temporary, if z=8 set as default after powerup.
 *
 * Button numbers:
 *   0 previously set button
 *   1 left button	(can't be used for mode changes)
 *   2 right button	(can't be used for mode changes)
 *   3 middle (wheel) button
 *   4 rear thumb button
 *   5 front thumb button
 *   6 find button
 *   7 wheel left tilt
 *   8 wheel right tilt
 *   9 side wheel forward
 *  11 side wheel backward
 *  13 side wheel pressed
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "revoco.h"
#include "../config.h"
#include "btnx.h"

#define streq(a,b)		(strcmp((a), (b)) == 0)
#define strneq(a,b,c)	(strncmp((a), (b), (c)) == 0)

#define LOGITECH	0x046d
#define MX_REVOLUTION	0xc51a	// version RR41.01_B0025
#define MX_REVOLUTION2	0xc525	// version RQR02.00_B0020

/*** extracted from hiddev.h ***/

typedef signed short s16;
typedef signed int s32;
typedef unsigned int u32;

struct hiddev_devinfo {
	u32 bustype;
	u32 busnum;
	u32 devnum;
	u32 ifnum;
	s16 vendor;
	s16 product;
	s16 version;
	u32 num_applications;
};

struct hiddev_report_info {
	u32 report_type;
	u32 report_id;
	u32 num_fields;
};

#define HID_REPORT_TYPE_INPUT	1
#define HID_REPORT_TYPE_OUTPUT	2
#define HID_REPORT_TYPE_FEATURE	3

struct hiddev_usage_ref {
	u32 report_type;
	u32 report_id;
	u32 field_index;
	u32 usage_index;
	u32 usage_code;
	s32 value;
};

struct hiddev_usage_ref_multi {
	struct hiddev_usage_ref uref;
	u32 num_values;
	s32 values[1024];
};

#define HIDIOCGDEVINFO		_IOR('H', 0x03, struct hiddev_devinfo)
#define HIDIOCGREPORT		_IOW('H', 0x07, struct hiddev_report_info)
#define HIDIOCSREPORT		_IOW('H', 0x08, struct hiddev_report_info)
#define HIDIOCGUSAGES		_IOWR('H', 0x13, struct hiddev_usage_ref_multi)
#define HIDIOCSUSAGES		_IOW('H', 0x14, struct hiddev_usage_ref_multi)

/*** end hiddev.h ***/

/* Static variables */
static int revoco_mode=0;
static int revoco_btn=3;
static int revoco_up_scroll=0;
static int revoco_down_scroll=0;

/* Static function declarations */
static void fatal(const char *fmt, ...);
static int open_dev(char *path);
static void close_dev(int fd);
static void send_report(int fd, int id, int *buf, int n);
static void mx_cmd(int fd, int b1, int b2, int b3);
static void configure(int handle);
static void trouble_shooting(void);
static int revoco_check_values(void);


static void fatal(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    fprintf(stderr, "revoco: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static int open_dev(char *path)
{
    char buf[128];
    int i, fd;
    struct hiddev_devinfo dinfo;

    for (i = 0; i < 16; ++i)
    {
	sprintf(buf, path, i);
	fd = open(buf, O_RDWR);
	if (fd >= 0)
	{
	    if (ioctl(fd, HIDIOCGDEVINFO, &dinfo) == 0)
		if (dinfo.vendor == (short)LOGITECH)
		{
		    if (dinfo.product == (short)MX_REVOLUTION)
			return fd;
		    if (dinfo.product == (short)MX_REVOLUTION2)
			return fd;
		}
	    close(fd);
	}
    }
    return -1;
}

static void close_dev(int fd)
{
    close(fd);
}

static void send_report(int fd, int id, int *buf, int n)
{
    struct hiddev_usage_ref_multi uref;
    struct hiddev_report_info rinfo;
    int i;

    uref.uref.report_type = HID_REPORT_TYPE_OUTPUT;
    uref.uref.report_id = id;
    uref.uref.field_index = 0;
    uref.uref.usage_index = 0;
    uref.num_values = n;
    for (i = 0; i < n; ++i)
	uref.values[i] = buf[i];
    if (ioctl(fd, HIDIOCSUSAGES, &uref) == -1)
    {
		fatal("send report %02x/%d, HIDIOCSUSAGES: %s", id, n, strerror(errno));
		return;
    }

    rinfo.report_type = HID_REPORT_TYPE_OUTPUT;
    rinfo.report_id = id;
    rinfo.num_fields = 1;
    if (ioctl(fd, HIDIOCSREPORT, &rinfo) == -1)
	{
		fatal("send report %02x/%d, HIDIOCSREPORT: %s", id, n, strerror(errno));
		return;
	}
}

static void mx_cmd(int fd, int b1, int b2, int b3)
{
    int buf[6] = { 0x01, 0x80, 0x56, b1, b2, b3 };

    send_report(fd, 0x10, buf, 6);
}

static void configure(int handle)
{
#if REVOCO_TEMP==1
	int perm = 0x00;
#else
	int perm = 0x80;
#endif
	
	switch (revoco_mode)
	{
	case REVOCO_FREE_MODE:
	    mx_cmd(handle, perm + 1, 0, 0);
	    break;
	case REVOCO_CLICK_MODE:
	    mx_cmd(handle, perm + 2, 0, 0);
	    break;
	case REVOCO_MANUAL_MODE:
	    mx_cmd(handle, perm + 7, revoco_btn * 16 + revoco_btn, 0);
	    break;
	case REVOCO_AUTO_MODE:
	    mx_cmd(handle, perm + 5, revoco_up_scroll, revoco_down_scroll);
	    break;
	default:
	    fatal("unknown mode %d", revoco_mode);
	    break;
    }
}

static void trouble_shooting(void)
{
    char *path;
    int fd;

	fprintf(stderr, OUT_PRE "revoco launch failed. If the problem persists, disable"
					"revoco from btnx-config. revoco error message:\n");

    fd = open(path = "/dev/hiddev0", O_RDWR);
    if (fd == -1 && errno == ENOENT)
	fd = open(path = "/dev/usb/hiddev0", O_RDWR);

    if (fd != -1)
    {
		fatal(	"No Logitech MX-Revolution (%04x:%04x or %04x:%04x) found.",
				LOGITECH, MX_REVOLUTION,
				LOGITECH, MX_REVOLUTION2);
		return;
    }

    if (errno == EPERM || errno == EACCES)
    {
		fatal(	"No permission to access hiddev (%s-15)\n"
	      		"Try 'sudo revoco ...'", path);
    	return;
    }
    
    fatal("Hiddev kernel driver not found.  Check with 'dmesg | grep hiddev'\n"
  "whether it is present in the kernel.  If it is, make sure that the device\n"
  "nodes (either /dev/usb/hiddev0-15 or /dev/hiddev0-15) are present.  You\n"
  "can create them with\n"
  "\n"
  "\tmkdir /dev/usb\n"
  "\tmknod /dev/usb/hiddev0 c 180 96\n"
  "\tmknod /dev/usb/hiddev1 c 180 97\n\t...\n"
  "\n"
  "or better by adding a rule to the udev database in\n"
  "/etc/udev/rules.d/10-local.rules\n"
  "\n"
  "\tBUS=\"usb\", KERNEL=\"hiddev[0-9]*\", NAME=\"usb/%k\", MODE=\"660\"\n");
}

static int revoco_check_values(void)
{
	switch (revoco_mode)
	{
	case REVOCO_FREE_MODE:
	    break;
	case REVOCO_CLICK_MODE:
	    break;
	case REVOCO_MANUAL_MODE:
	    if ((revoco_btn < 3 && revoco_btn != 0) || 
	    	revoco_btn == 10 || 
	    	revoco_btn == 12 || 
	    	revoco_btn > 13) 
	    {
	    	return -1;
	    }
	    break;
	case REVOCO_AUTO_MODE:
	    if (revoco_up_scroll < 0 || revoco_up_scroll > 50 ||
	    	revoco_down_scroll < 0 || revoco_down_scroll > 50) 
	    {
	    	return -1;
	    }
	    	
	    break;
	default:
	    return -1;
	}
	return 0;
}

void revoco_set_mode(int mode)
{	
	revoco_mode = mode;	
}

void revoco_set_btn(int btn)
{
	revoco_btn = btn;	
}

void revoco_set_up_scroll(int value)
{
	revoco_up_scroll = value;	
}

void revoco_set_down_scroll(int value)
{
	revoco_down_scroll = value;
}

int revoco_launch(void)
{
    int handle;
    
    if (revoco_mode == REVOCO_DISABLED)
    {
    	fprintf(stderr, OUT_PRE "revoco not started. Disabled in configuration.\n");
    	return 0;
    }
    if (revoco_check_values() < 0)
    {
    	fprintf(stderr, OUT_PRE "Attempted to pass illegal values to revoco. revoco "
    					"will not be started.\n");
    	return -1;
    }

    handle = open_dev("/dev/usb/hiddev%d");
    if (handle == -1)
	handle = open_dev("/dev/hiddev%d");
    if (handle == -1)
    {
		trouble_shooting();
    	return -1;
    }

    configure(handle);

    close_dev(handle);
    return 0;
}
