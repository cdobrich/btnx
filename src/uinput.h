
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */

#ifndef UINPUT_H_
#define UINPUT_H_

#include "btnx.h"

#define UMOUSE_NAME		"btnx mouse"
#define UKBD_NAME		"btnx keyboard"
#define UINPUT_LOCATION	"/dev/input/uinput"


int uinput_init(void);
void uinput_close(void);
void uinput_key_press(btnx_event *bev);

#endif /*UINPUT_H_*/
