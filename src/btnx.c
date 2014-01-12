
/*------------------------------------------------------------------------*
 * btnx (Button extension): A program for rerouting                       *
 * events from the mouse as keyboard and other mouse events (or both).    *
 * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>                 *
 *                                                                        *
 * This program is free software; you can redistribute it and/or          *
 * modify it under the terms of the GNU General Public License            *
 * as published by the Free Software Foundation; either version 2         *
 * of the License, or (at your option) any later version.                 *
 *                                                                        *
 * This program is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 * GNU General Public License for more details.                           *
 *                                                                        *
 * You should have received a copy of the GNU General Public License      *
 * along with this program; if not, write to the Free Software            *
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,                     *
 * Boston, MA  02110-1301, USA.                                           *
 *------------------------------------------------------------------------*/
 
/*------------------------------------------------------------------------*
 * File revoco.c has no copyrights (it was written by an animal), and is  *
 * not bound by the GNU GPL.                                              *
 * However, it is still distributed and modified by the blessing of its   *
 * original author. See revoco.c for contact info of the author.          *
 *------------------------------------------------------------------------*/
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <linux/input.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>

#include "uinput.h"
#include "btnx.h"
#include "config_parser.h"
#include "device.h"
#include "revoco.h"

#define PROGRAM_NAME			PACKAGE
#define PROGRAM_VERSION			VERSION

#define CHAR2INT(c, x) (((int)(c)) << ((x) * 8))
#define INPUT_BUFFER_SIZE		512
#define NUM_EVENT_HANDLERS		20
#define NUM_HANDLER_LOCATIONS	3
#define TYPE_MOUSE				0
#define TYPE_KBD				1

#define test_bit(bit, array) (array[bit/8] & (1<<(bit%8)))

/* Static variables */
static char *g_exec_path=NULL; 		/* Path of this executable */
static struct timeval exec_time; 	/* time when daemon was executed. */

/* Possible paths of event handlers */
const char handler_locations[][15] = {
	{"/dev"},
	{"/dev/input"},
	{"/dev/misc"}
};

/* Static function declarations */
static const char *get_handler_location(int index);
static int find_handler(struct device_fds_t *dev_fds, int flags, int vendor, int product);
static int btnx_event_get(btnx_event **bevs, int rawcode, int pressed);
static hexdump_t btnx_event_read(int fd, int *status);
static void command_execute(btnx_event *bev);
static void config_switch(btnx_event **bevs, int index);
static void send_extra_event(btnx_event **bevs, int index);
static int check_delay(btnx_event *bev);
static void main_args(int argc, char *argv[], int *bg, int *kill_all, char **config_file);

/* To simplify the open_handler loop. Can't think of another reason why I
 * coded this */
static const char *get_handler_location(int index) {
	if (index < 0 || index > NUM_HANDLER_LOCATIONS - 1)
		return NULL;
	
	return handler_locations[index];
}


/* Used to find a certain named event handler in several paths */
int open_handler(char *name, int flags) {
	const char *loc;
	int x=0, fd;
	char loc_buffer[128];
	
	while ((loc = get_handler_location(x++)) != NULL) {
		sprintf(loc_buffer, "%s/%s", loc, name);
		if ((fd = open(loc_buffer, flags)) >= 0) {
			//daemon_log(LOG_DEBUG, OUT_PRE "Opened handler: %s", loc_buffer);
			return fd;
		}
	}
	
	return NULL_FD;
}

/* Tries to find an input handler that has a certain vendor and product
 * ID associated with it. type determines whether it is a mouse or keyboard
 * input handler that is being searched. */
static int find_handler(struct device_fds_t *dev_fds, int flags, int vendor, int product)
{
	int i, fd;
	unsigned short id[6];
	char name[16];
	
	for (i=0; i<NUM_EVENT_HANDLERS; i++) {
		sprintf(name, "event%d", i);
		if ((fd = open_handler(name, flags)) < 0)
			continue;
		ioctl(fd, EVIOCGID, id); /* Extract IDs */
		if (vendor == id[ID_VENDOR] && product == id[ID_PRODUCT]) {
			device_fds_add_fd(dev_fds, fd);
		}
		else
			close(fd);
	}
	device_fds_set_max_fd(dev_fds);
	return dev_fds->count; /* No such handler found */
}

/* Find the btnx_event structure that is associated with a captured rawcode
 * and return its index. */
static int btnx_event_get(btnx_event **bevs, int rawcode, int pressed)
{
	int i=0;
	
	while (bevs[i] != 0) {
		if (bevs[i]->rawcode == rawcode) {
			if (bevs[i]->enabled == 0)
				return -1; /* associated rawcode found, but event is disabled */
			bevs[i]->pressed = pressed;
			return i; /* rawcode found and event is enabled */
		}
		i++;
	}
	/* no such rawcode in configuration */
	return -1; 
}

/* Extract the rawcode(s) of an input event. */
static hexdump_t btnx_event_read(int fd, int *status) {
	hexdump_t hexdump = {.rawcode = 0, .pressed = 0};
	struct input_event ev;

	if ((*status = read(fd, &ev, sizeof(ev))) > 0) {
		if (((ev.code == 0x0000 || ev.code == 0x0001) && ev.type == 0x0002) || (ev.code == 0x0000 && ev.type == 0x0000)) {
			hexdump.rawcode = 0;
			hexdump.pressed = 0;
		}
		hexdump.rawcode = ev.code & 0xFFFF;
		if (ev.type == 0x0002)
			hexdump.rawcode += (ev.value & 0xFF) << 16;
		hexdump.rawcode += (ev.type & 0xFF) << 24;
		hexdump.pressed = ev.value;
	}
	
	return hexdump;
}

/* Execute a shell script or binary file */
static void command_execute(btnx_event *bev) {
	int pid;
	
	if (!(pid = fork())) {
		/* Change this to use su to set environments correctly for certain
		 * programs */
		setuid(bev->uid);
		execv(bev->args[0], bev->args);
	}
	else if (pid < 0) {
		daemon_log(LOG_WARNING, OUT_PRE "Error: could not fork: %s", strerror(errno));
		return;
	}
	return;
}

/* Perform a configuration switch */
static void config_switch(btnx_event **bev, int index) {
	const char *name=NULL;
	struct timeval now;
	
	/* Block in case last config switch button is the same as a current one.
	 * This helps prevent a situation where configurations switch multiple
	 * times if the button is held down while the switch occurs. */
	gettimeofday(&now, NULL);
	if (((int)(((unsigned int)now.tv_sec - (unsigned int)exec_time.tv_sec) * 1000) +
		(int)(((int)now.tv_usec - (int)exec_time.tv_usec) / 1000))
		< (int)500)
			return;
	/* ------------------------------------------------------------------- */
	
	switch (bev[index]->switch_type) {
	case CONFIG_SWITCH_NEXT:
		name = config_get_next();
		break;
	case CONFIG_SWITCH_PREV:
		name = config_get_prev();
		break;
	case CONFIG_SWITCH_TO:
		name = bev[index]->switch_name;
	}
	
	if (name == NULL) {
		daemon_log(LOG_WARNING, OUT_PRE "Warning: config switch failed. "
				"Invalid configuration name.");
		return;
	}
	
	daemon_log(LOG_DEBUG, OUT_PRE "switching to config: %s", name);
	uinput_close();
	daemon_signal_done();
	daemon_pid_file_remove();
	execl(g_exec_path, g_exec_path, "-c", name, (char *) NULL);
}

/* Special events, like wheel scrolls and command executions need to be
 * handled differently. They use this function. */
static void send_extra_event(btnx_event **bevs, int index)
{
	int tmp_kc = bevs[index]->keycode;
	
	if (tmp_kc == COMMAND_EXECUTE) {
		command_execute(bevs[index]);
		return;
	}
	if (tmp_kc == CONFIG_SWITCH) {
		config_switch(bevs, index);
		return;
	}
	
	/* Perform a "button down" and "button up" event for relative events
	 * such as wheel scrolls. */
	bevs[index]->pressed = 1;
	uinput_key_press(bevs[index]);
	bevs[index]->pressed = 0;
	/* Don't remember why the KEY_UNKNOWN is necessary. */
	bevs[index]->keycode = KEY_UNKNOWN;
	uinput_key_press(bevs[index]);
	bevs[index]->keycode = tmp_kc;
}

/* This function checks if there has been sufficient delay between two
 * occurrances of the same event. Delay is in milliseconds, defined in the
 * configuration file. 
 * Returns 0 if delay is satisfied, -1 if there has not been enough delay. */
static int check_delay(btnx_event *bev) {
	struct timeval now;
	gettimeofday(&now, NULL);
	
	if (bev->last.tv_sec == 0 && bev->last.tv_usec == 0)
		return 0;
	
	/* Perform a millisecond conversion and compare */
	if (((int)(((unsigned int)now.tv_sec - (unsigned int)bev->last.tv_sec) * 1000) +
		(int)(((int)now.tv_usec - (int)bev->last.tv_usec) / 1000))
		> (int)bev->delay)
		return 0;
	return -1;
}

/* Parses command line arguments. */
static void main_args(int argc, char *argv[], int *bg, int *kill_all, char **config_file) {
	g_exec_path = argv[0];
	
	if (argc > 1) {
		int x;
		for (x=1; x<argc; x++) {
			/* Background daemon */
			if (!strncmp(argv[x], "-b", 2))
				*bg=1;
			/* Print version information */
			else if (!strncmp(argv[x], "-v", 2)) {
				printf(	PROGRAM_NAME " v." PROGRAM_VERSION "\n"
						"Author: Olli Salonen <oasalonen@gmail.com>\n"
						"Compatible with btnx-config >= v.0.4.7\n");
				exit(BTNX_EXIT_NORMAL);	
			}
			/* Start with specific configuration */
			else if (!strncmp(argv[x], "-c", 2)) {
				if (x < argc - 1) {
					if (strlen(argv[x+1]) >= CONFIG_NAME_MAX_SIZE) {
						daemon_log(LOG_ERR, OUT_PRE "Error: invalid configuration name.");
						goto usage;
					}
					*config_file = (char *) malloc((strlen(argv[x+1])+1) * sizeof(char));
					strcpy(*config_file, argv[x+1]);
					x++;
				}
				else {
					daemon_log(LOG_ERR, OUT_PRE "Error: -c argument used but no "
							"configuration file name specified.");
					goto usage;
				}
			}
			/* Output to syslog */
			else if (!strncmp(argv[x], "-l", 2))
				daemon_log_use = DAEMON_LOG_SYSLOG;
			else if (!strncmp(argv[x], "-k", 2))
				*kill_all = 1;
			else {
				usage:
				daemon_log(LOG_INFO, PROGRAM_NAME " usage:\n"
						"\tArgument:\tDescription:\n"
						"\t-v\t\tPrint version number\n"
						"\t-b\t\tRun process as a background daemon\n"
						"\t-c CONFIG\tRun with specified configuration\n"
						"\t-k\t\tKill all btnx daemons\n"
				        "\t-l\t\tRedirect output to syslog\n"
						"\t-h\t\tPrint this text");
				exit(BTNX_ERROR_FATAL);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	int fd_daemon=0;
	fd_set fds;
	struct device_fds_t dev_fds;
	hexdump_t hexdump = {.rawcode=0, .pressed=0};
	int max_fd, ready, set_fd;
	btnx_event **bevs = NULL;
	int bev_index;
	int suppress_release=1;
	int bg=0, ret=BTNX_EXIT_NORMAL;
	char *config_name=NULL;
	int kill_all=0;
	int leave_pid_file=0;
	pid_t pid;
	
	daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);
	daemon_log_use = DAEMON_LOG_STDERR;
	
	main_args(argc, argv, &bg, &kill_all, &config_name);
	
	if (kill_all) {
		if ((ret = daemon_pid_file_kill_wait(SIGINT, 5)) < 0) {
			daemon_log(LOG_WARNING, OUT_PRE "Failed to kill previous btnx processes");
		}
		exit(BTNX_EXIT_NORMAL);
	}
	if ((pid = daemon_pid_file_is_running()) >= 0) {
		daemon_log(LOG_WARNING, OUT_PRE "Previous daemon already running. Killing it.");
		if ((ret = daemon_pid_file_kill_wait(SIGINT, 5)) < 0) {
			daemon_log(LOG_WARNING, OUT_PRE "Failed to kill previous btnx process %u", pid);
			daemon_log(LOG_WARNING, OUT_PRE "Exit forced");
			kill(pid, SIGKILL);
			daemon_pid_file_remove();
			//exit(BTNX_ERROR_FATAL);
		}
	}
	
	if (system("modprobe uinput") != 0)	{
		daemon_log(LOG_WARNING, OUT_PRE "Modprobe uinput failed. Make sure the uinput "
				"module is loaded before running btnx. If it's already running,"
				" no problem.");
	}
	else
		daemon_log(LOG_INFO, OUT_PRE "uinput modprobed successfully.");
	
	/* Loop through the configurations until no more are left or a configured
	 * mouse is detected. */
	device_fds_init(&dev_fds);
	while (bevs == NULL) {
		bevs = config_parse(&config_name);		
		if (bevs == NULL) {
			daemon_log(LOG_ERR, OUT_PRE "Configuration file error.");
			exit(BTNX_ERROR_NO_CONFIG);
		}
		
		if (find_handler(&dev_fds, O_RDONLY, device_get_vendor_id(), 
		                 device_get_product_id()) == 0) {
			daemon_log(LOG_ERR, OUT_PRE "No configured mouse handler detected: %s", 
			           strerror(errno));
			free(bevs);
			bevs = NULL;
			if (config_get_next() != NULL && config_name != NULL) {
				free(config_name);
				config_name = malloc(sizeof(strlen(config_get_next())) + 1);
				strcpy(config_name, config_get_next());
			}
			else
				exit(BTNX_ERROR_OPEN_HANDLER);
		}
	}
	
	uinput_init();
	
	revoco_launch();
	
	daemon_log(LOG_INFO, OUT_PRE "No startup errors.");
		
	if (bg) {
		if ((pid = daemon_fork()) < 0) {
			daemon_log(LOG_ERR, OUT_PRE "Daemon fork failed. Quitting.");
			exit(BTNX_ERROR_FATAL);
		}
		else if (pid > 0) {
			daemon_log(LOG_DEBUG, OUT_PRE "Parent done.");
			exit(BTNX_EXIT_NORMAL);
		}
	}
	
	if (daemon_pid_file_create() < 0) {
		daemon_log(LOG_ERR, OUT_PRE "Could not create PID file: %s", strerror(errno));
		ret = BTNX_ERROR_CREATE_PID_FILE;
		
		/* Do not delete the pid file in finish_daemon in this case, leads to
		 * extra btnx processes */
		leave_pid_file = 1;
		goto finish_daemon;
	}
	if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, 0) < 0) {
		daemon_log(LOG_ERR, OUT_PRE "Could not register signal handlers: %s.", strerror(errno));
		ret = BTNX_ERROR_INIT_SIGNALS;
		goto finish_daemon;
	}
	
	fd_daemon = daemon_signal_fd();
	if (fd_daemon > dev_fds.max_fd)
		max_fd = fd_daemon;
	else
		max_fd = dev_fds.max_fd;
	
	gettimeofday(&exec_time, NULL);
	
	for (;;) {
	    int read_status;
	    
		FD_ZERO(&fds);
		device_fds_fill_fds(&dev_fds, &fds);
		FD_SET(fd_daemon, &fds);
	
		ready = select(max_fd+1, &fds, NULL, NULL, NULL);
		
		if (ready == -1)
			daemon_log(LOG_WARNING, OUT_PRE "select() error: %s", strerror(errno));
		else if (ready == 0)
			continue;
		else {
			set_fd = device_fds_find_set_fd(&dev_fds, &fds);
			if (set_fd != NULL_FD) {
				hexdump = btnx_event_read(set_fd, &read_status);
				if (read_status < 0) {
				    daemon_log(LOG_ERR, OUT_PRE "Handler read failed.");
				    goto finish_daemon;
				}
			}
			else if (FD_ISSET(fd_daemon, &fds)) {
				int sig;
				if ((sig = daemon_signal_next()) <= 0) {
					daemon_log(LOG_ERR, OUT_PRE "daemon_signal_next() failed.");
					goto finish_daemon;
				}
				
				switch (sig) {
				case SIGINT:
				case SIGQUIT:
				case SIGTERM:
					daemon_log(LOG_INFO, OUT_PRE "Received quit signal.");
					goto finish_daemon;
				}
			}
			else {
			    daemon_log(LOG_WARNING, OUT_PRE "Unexpected fd status.");
				continue;
			}
			
			if (hexdump.rawcode == 0)
				continue;
			if ((bev_index = btnx_event_get(bevs, hexdump.rawcode, hexdump.pressed)) != -1) {
				if (bevs[bev_index]->pressed == 1 || bevs[bev_index]->type == BUTTON_IMMEDIATE
					|| bevs[bev_index]->type == BUTTON_RELEASE) {
					if (check_delay(bevs[bev_index]) < 0)
						continue;
					gettimeofday(&(bevs[bev_index]->last), NULL);
				}
				/* Force release, ignore button release */
				if (bevs[bev_index]->type == BUTTON_RELEASE &&
					bevs[bev_index]->pressed == 0)
					continue;
				if ((bevs[bev_index]->type == BUTTON_IMMEDIATE ||
					bevs[bev_index]->type == BUTTON_RELEASE) && 
					bevs[bev_index]->keycode < BTNX_EXTRA_EVENTS) {
					
					bevs[bev_index]->pressed = 1;
					uinput_key_press(bevs[bev_index]);
					bevs[bev_index]->pressed = 0;
					uinput_key_press(bevs[bev_index]);
				}
				else if (bevs[bev_index]->keycode > BTNX_EXTRA_EVENTS) {
					if (bevs[bev_index]->type == BUTTON_NORMAL) {
						if ((suppress_release = !suppress_release) != 1)
							send_extra_event(bevs, bev_index);
					}
					else if (bevs[bev_index]->type == BUTTON_IMMEDIATE ||
							bevs[bev_index]->type == BUTTON_RELEASE)
						send_extra_event(bevs, bev_index);
				}
				else
					uinput_key_press(bevs[bev_index]);
			}
		}
		
		/* Clean up the undead */
		waitpid(-1, NULL, WNOHANG);	
	}
	
finish_daemon:
	daemon_log(LOG_INFO, OUT_PRE "Exiting...");
	uinput_close();
	device_fds_close(&dev_fds);
	daemon_signal_done();
	if (leave_pid_file == 0)
	  daemon_pid_file_remove();
	
	return ret;
}
