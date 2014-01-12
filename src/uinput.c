
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libdaemon/dlog.h>

#include "uinput.h"
#include "btnx.h"

#define BTNX_VENDOR			0xB216
#define BTNX_PRODUCT_MOUSE	0x0001
#define BTNX_PRODUCT_KBD	0x0002

/* Static variables */
static int uinput_mouse_fd = -1;
static int uinput_kbd_fd = -1;

/*
 * uinput_init() function partially derived from Micah Dowty's uinput_mouse.c
 * <http://svn.navi.cx/misc/trunk/vision/lib/uinput_mouse.c>
 */
/* Open and init uinput file descriptors */
int uinput_init(void) 
{
  struct uinput_user_dev dev_mouse, dev_kbd;
  int i;

  uinput_mouse_fd = open_handler("uinput", O_WRONLY | O_NDELAY);
  if (uinput_mouse_fd < 0) 
  {
    perror(	OUT_PRE "Error opening the uinput device.\n"
    		OUT_PRE "Make sure you have loaded the uinput module (modprobe uinput)");
    exit(BTNX_ERROR_OPEN_UINPUT);
  }
  uinput_kbd_fd = open_handler("uinput", O_WRONLY | O_NDELAY);
  if (uinput_kbd_fd < 0) 
  {
    perror(OUT_PRE "Error opening the uinput device");
    exit(BTNX_ERROR_OPEN_UINPUT);
  }

  memset(&dev_mouse, 0, sizeof(dev_mouse));
  dev_mouse.id.bustype = 0;
  dev_mouse.id.vendor = BTNX_VENDOR;
  dev_mouse.id.product = BTNX_PRODUCT_MOUSE;
  dev_mouse.id.version = 0;
  strcpy(dev_mouse.name, UMOUSE_NAME);
  write(uinput_mouse_fd, &dev_mouse, sizeof(dev_mouse));
  
  memset(&dev_kbd, 0, sizeof(dev_kbd));
  dev_kbd.id.bustype = 0;
  dev_kbd.id.vendor = BTNX_VENDOR;
  dev_kbd.id.product = BTNX_PRODUCT_KBD;
  dev_kbd.id.version = 0;
  strcpy(dev_kbd.name, UKBD_NAME);
  write(uinput_kbd_fd, &dev_kbd, sizeof(dev_kbd));

  ioctl(uinput_mouse_fd, UI_SET_EVBIT, EV_REL);
  ioctl(uinput_mouse_fd, UI_SET_RELBIT, REL_X);
  ioctl(uinput_mouse_fd, UI_SET_RELBIT, REL_Y);
  ioctl(uinput_mouse_fd, UI_SET_RELBIT, REL_WHEEL);
  ioctl(uinput_mouse_fd, UI_SET_EVBIT, EV_KEY);
  
  for (i=BTN_MISC; i<KEY_OK; i++)
  {
  	ioctl(uinput_mouse_fd, UI_SET_KEYBIT, i);
  }
   
  ioctl(uinput_kbd_fd, UI_SET_EVBIT, EV_KEY);
  ioctl(uinput_kbd_fd, UI_SET_EVBIT, EV_REP);
  for (i=0; i<BTN_MISC; i++)
  {
  	ioctl(uinput_kbd_fd, UI_SET_KEYBIT, i);
  }
  for (i=KEY_OK; i<KEY_MAX; i++)
  {
  	ioctl(uinput_kbd_fd, UI_SET_KEYBIT, i);
  }
  
  ioctl(uinput_kbd_fd, UI_DEV_CREATE, 0);
  ioctl(uinput_mouse_fd, UI_DEV_CREATE, 0);

  return 0;
}

void uinput_close(void) {
	if (uinput_mouse_fd > -1)
		close(uinput_mouse_fd);
	if (uinput_kbd_fd > -1)
		close(uinput_kbd_fd);
}

/* Send any necessary modifier keys */
static void uinput_send_mods(struct btnx_event *bev, struct input_event event) {
	int i;
	int mod_pressed=0;
	
	for (i=0; i<MAX_MODS; i++)
	{
		if (bev->mod[i] == 0)
			continue;
			
		event.type = EV_KEY;
		event.code = bev->mod[i];
		event.value = bev->pressed;
		write(uinput_kbd_fd, &event, sizeof(event));
		
		event.type = EV_SYN;
  		event.code = SYN_REPORT;
  		event.value = 0;
  		write(uinput_kbd_fd, &event, sizeof(event));
  		
  		mod_pressed = 1;
	}
	
	if (mod_pressed)
	    usleep(200);    /* Needs a little delay for mouse + modifier combo */
}

/* Send the main key or button press/release */
static void uinput_send_key(struct btnx_event *bev, struct input_event event, int fd) {
	if (bev->keycode > BTNX_EXTRA_EVENTS)
	{
		event.type = EV_REL;
		
		if (bev->keycode == REL_WHEELFORWARD || bev->keycode == REL_WHEELBACK)
			event.code = REL_WHEEL;
		
		if (bev->keycode == REL_WHEELFORWARD)
			event.value = 1;
		else if (bev->keycode == REL_WHEELBACK)
			event.value = -1;
		write(fd, &event, sizeof(event));
	}
	else
	{
		event.type = EV_KEY;
		event.code = bev->keycode;
		event.value = bev->pressed;
		write(fd, &event, sizeof(event));
	}
	
	event.type = EV_SYN;
  	event.code = SYN_REPORT;
  	event.value = 0;
  	write(fd, &event, sizeof(event));
}

/* Send a key combo event, either press or release */
void uinput_key_press(struct btnx_event *bev)
{
	struct input_event event;
	int fd;

	if (uinput_mouse_fd < 0 || uinput_kbd_fd < 0)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Warning: uinput_fd not valid");
		return;
	}
	
	if ((bev->keycode <= KEY_UNKNOWN || bev->keycode >= KEY_OK) && bev->keycode < BTNX_EXTRA_EVENTS)
		fd = uinput_kbd_fd;
	else
		fd = uinput_mouse_fd;
		
	gettimeofday(&event.time, NULL);
	
	/* If button is pressed, send modifiers first and then the main key.
	 * If button is released, release main key first and then the modifiers. */
	if (bev->pressed) {
		uinput_send_mods(bev, event);
		uinput_send_key(bev, event, fd);
	}
	else {
		uinput_send_key(bev, event, fd);
		uinput_send_mods(bev, event);
	}
}


