
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */

#ifndef BTNX_H_
#define BTNX_H_

#include <sys/time.h>
#include "../config.h"

#define MAX_MODS		3
#define MAX_RAWCODES	10
#define HEXDUMP_SIZE	8
#define NULL_FD			-1

/* A string prepended to output from this program. Makes messages and errors
 * clearer. */
#define OUT_PRE			" btnx: "

/* Exit errors */
enum
{
	BTNX_EXIT_NORMAL=0,
	BTNX_ERROR_FATAL,
	BTNX_ERROR_NO_BIN_RESERVED,
	BTNX_ERROR_NO_CONFIG=150,
	BTNX_ERROR_BAD_CONFIG,
	BTNX_ERROR_OPEN_HANDLER,
	BTNX_ERROR_OPEN_UINPUT,
	BTNX_ERROR_CREATE_PID_FILE,
	BTNX_ERROR_INIT_SIGNALS,
};

/* btnx specific events, not specified by the input interface */
enum
{
	BTNX_EXTRA_EVENTS=0xFFF0,
	REL_WHEELFORWARD,
	REL_WHEELBACK,
	COMMAND_EXECUTE,
	CONFIG_SWITCH
};

/* Configuration switch types */
enum
{
	CONFIG_SWITCH_NONE,
	CONFIG_SWITCH_NEXT,
	CONFIG_SWITCH_PREV,
	CONFIG_SWITCH_TO
};

/* Button types */
enum
{
	BUTTON_NORMAL=0,	/* Send pressed and release signals separately when received */
	BUTTON_IMMEDIATE,	/* Send both pressed and released signals immediately */
	BUTTON_RELEASE		/* Same as immediate, but release event is ignored */
};

/* Contains all necessary information to handle a button event */
typedef struct btnx_event
{
	int		rawcode;		/* 32-bit button rawcode, sniffed from event handler */
	int 	type;			/* Button type */
	int 	delay;			/* Minimum time before event can be resent */
	struct timeval last;	/* Last time this event occurred */
	int 	keycode;		/* Keyboard or mouse button keycode */
	int 	mod[MAX_MODS];	/* Modifier key for the keycode */
	int 	pressed;		/* 0: release event, 1: press event */
	int		enabled;		/* Only send event if enabled */
	char	*command;		/* The absolute path of the executable to execute */
	char	**args;			/* Arguments for the executable */
	int		uid;			/* UID to run the command as */
	int		switch_type;	/* Configuration switch type */
	char	*switch_name;	/* Name of confiugration to switch to */
} btnx_event;

/* Most important data from hexdumping an event handler */
typedef struct hexdump_s
{
	int rawcode;	/* 32-bit button rawcode */
	int pressed;	/* Specifies whether button was pressed or released */
} hexdump_t;

int open_handler(char *name, int flags);

#endif /*BTNX_H_*/
