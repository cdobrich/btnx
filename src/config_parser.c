
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */

#define _GNU_SOURCE				// Needed for strcasestr()

#include "btnx.h"
#include "config_parser.h"
#include "device.h"
#include "revoco.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <libdaemon/dlog.h>

#define CONFIG_MOUSE_BEGIN		"Mouse"
#define CONFIG_MOUSE_END		"EndMouse"
#define CONFIG_BUTTON_BEGIN		"Button"
#define CONFIG_BUTTON_END		"EndButton"
#define MAX_BEVS	10

/* Value used to indicate what type of block is currently being parsed */
enum
{
	BLOCK_NONE,		/* Not parsing a block */
	BLOCK_MOUSE,	/* Parsing a Mouse block */
	BLOCK_BUTTON	/* Parsing a Button block */
};

/* Static variables */
static char next_config[CONFIG_NAME_MAX_SIZE];	/* Name of next config */
static char prev_config[CONFIG_NAME_MAX_SIZE];	/* Name of previous config */
static char first_config[CONFIG_NAME_MAX_SIZE] = ""; /* Name of current config */
static int have_next_config=0;					/* Is next config defined? */
static int have_prev_config=0;					/* Is previous config defined? */

/* Static function declarations */
static inline void strip_newline(char *str, int size);
static char *config_get_names(char *config_name);
static const char *config_add_value(btnx_event *e, 
									int type, 
									const char *option, 
									char *value);
static void config_add_mod(btnx_event *e, int mod);
static int config_get_keycode(const char *value);
static char **config_split_command(char *cmd);
static char *config_set_command(btnx_event *e, char *value);
static void config_set_switch_type(btnx_event *e, char *value);
static void config_set_switch_name(btnx_event *e, char *value);

/* Strip newlines from a string. Used for config name parsing. */
static inline void strip_newline(char *str, int size)
{
	int x;
	if (str == NULL) return;
	for (x=0; x<size; x++)
		if (str[x] == '\n') str[x] = '\0';
}

/* Return the name of the next configuration */
const char *config_get_next(void)
{
	if (have_next_config)
		return next_config;
	return NULL;
}

/* Return the name of the previous configuration */
const char *config_get_prev(void)
{
	if (have_prev_config)
		return prev_config;
	return NULL;
}

/* Parse config names from the config manager file. If no current
 * name is set or the current name does not exist, set the current
 * config name to the default config and return its name */
static char *config_get_names(char *config_name)
{
	FILE *fp;
	char buffer[CONFIG_PARSE_BUFFER_SIZE];
	char tmp_buffer[CONFIG_PARSE_BUFFER_SIZE];
	char name_prev[CONFIG_NAME_MAX_SIZE];
	char name_first[CONFIG_NAME_MAX_SIZE];
	int prev=0, next=0, first=0, loops=0, found=0;
	
	name_prev[0] = '\0';
	name_first[0] = '\0';
	
	if (!(fp = fopen(CONFIG_MANAGER_FILE,"r")))
	{
		perror(OUT_PRE "Could not read the config manager file");
		if (config_name != NULL) free(config_name);
		config_name = NULL;
		return NULL;
	}
	
	while (fgets(tmp_buffer, CONFIG_PARSE_BUFFER_SIZE-1, fp) != NULL)
	{
		loops++;
		strip_newline(tmp_buffer, CONFIG_PARSE_BUFFER_SIZE);
		memcpy(buffer, tmp_buffer, CONFIG_PARSE_BUFFER_SIZE);
		
		if (first == 1 && next == 1) continue;
		if (loops == 1)
			memcpy(name_first, buffer, CONFIG_NAME_MAX_SIZE);
		
		if (config_name == NULL)
		{
			config_name = (char *) malloc(CONFIG_NAME_MAX_SIZE * sizeof(char));
			memcpy(config_name, buffer, CONFIG_NAME_MAX_SIZE);
			first = 1;
			found = 1;
			continue;
		}
		if (first == 1 && next == 0)
		{
			memcpy(next_config, buffer, CONFIG_NAME_MAX_SIZE);
			next = 1;
			continue;
		}
		if (found == 1 && next == 0)
		{
			memcpy(next_config, buffer, CONFIG_NAME_MAX_SIZE);
			next = 1;
			if (prev == 1)
				break;
			else
				continue;
		}
		if (strncmp(buffer, config_name, CONFIG_NAME_MAX_SIZE) == 0)
		{
			found = 1;
			if (loops == 1)
				first = 1;
			else
			{
				memcpy(prev_config, name_prev, CONFIG_NAME_MAX_SIZE);
				prev = 1;
			}
			continue;
		}
		else
		{
			memcpy(name_prev, buffer, CONFIG_NAME_MAX_SIZE);
			continue;
		}
	}
	
	if (loops == 0)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Warning: config manager file is empty.");
		return NULL;
	}
	if (loops > 0 && found == 0 && name_first[0] != '\0')
	{
		daemon_log(LOG_WARNING, OUT_PRE "Warning: attempted to open an unexisting "
				"configuration name: %s. Using the default one.", config_name);
		if (config_name != NULL) free(config_name);
		fclose(fp);
		return (config_name = config_get_names(NULL));
	}
	if (first == 1)
	{
		memcpy(prev_config, buffer, CONFIG_NAME_MAX_SIZE);
		prev = 1;
	}
	if (found == 1 && next == 0)
	{
		memcpy(next_config, name_first, CONFIG_NAME_MAX_SIZE);
		next = 1;
	}
	
	have_prev_config = prev;
	have_next_config = next;
	
	fclose(fp);
	
	return config_name;
}

/* Converts the string representation of a keycode to its integer value */
static int config_get_keycode(const char *value)
{
	FILE *fp;
	char buffer[128];
	char *loc_beg;
	unsigned int len;
	
	/* Length is longer than any defined event */
	if (strlen(value) > 20)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Warning: possibly malformed keycode or "
				"modifier value. Ignoring.");
		return 0;
	}
	
	/* Explicitly no defined keycode */
	if (!strcasecmp(value, "none"))
		return 0;
	
	/* btnx's own defined events */
	if (!strcasecmp(value, "REL_WHEELFORWARD"))
		return REL_WHEELFORWARD;
	else if (!strcasecmp(value, "REL_WHEELBACK"))
		return REL_WHEELBACK;
	
	/* use cat and grep to our advantage here to find the keycode in the
	 * events file */
	sprintf(buffer, "cat %s/%s | /bin/grep %s", CONFIG_PATH, EVENTS_NAME, value);
	fp = popen(buffer, "r");
	while (fgets(buffer, 127, fp) != NULL)
	{
		len = strlen(buffer);
		if (len > 0 && len > strlen(value))
		{
			loc_beg = strcasestr(buffer, value) + strlen(value);
			if (!isspace(*loc_beg))
				continue;
			pclose(fp);
			return strtol(loc_beg, NULL, 0);
		}
	}
	pclose(fp);
	
	return 0;
}

/* Adds a modifier key value to an event structure */
static void config_add_mod(btnx_event *e, int mod)
{
	int i;
	
	for (i=0; i<MAX_MODS; i++)
	{
		if (e->mod[i] == 0)
		{
			e->mod[i] = mod;
			return;
		}
	}
	
	daemon_log(LOG_WARNING, OUT_PRE "Warning: attempting to add more mods than allowed "
			"by MAX_MODS");
}

/* Split an execute command string into a string vector of its executable path
 * and its arguments for execv() */
static char **config_split_command(char *cmd)
{
	char *beg, *end, closing='\0';
	int stop=0, i=0, enclosed=0;
	char **args=NULL;
	
	if (cmd == NULL)
		return NULL;
	args = (char **) malloc(sizeof(char*));
	if (args == NULL)
		return NULL;
	args[0] = NULL;
	
	beg = cmd;
	
	while (1)
	{
		while (*beg == '\t' || (*beg == ' ' && *(beg-1) != '\\')) beg++;
		if (*beg == '\0') break;
		end = beg;
		
		while (	(*end != '\0' && *end != '\t' && *end != ' ') ||
				(*end == ' ' && *(end-1) == '\\') ||
				(*end == ' ' && enclosed))
		{
			if (IS_ENCLOSING(*end) && enclosed == 0)
			{
				closing = *end;
				enclosed = 1;
			}
			else if (IS_ENCLOSING(*end) && enclosed == 1)
			{
				if (closing == *end)
					enclosed = 0;
			}
			end++;
		}
		if (*end == '\0') stop = 1;
		*end = '\0';
		
		i++;
		args = (char **) realloc(args, (i+1) * sizeof(char*));
		args[i-1] = beg; args[i] = NULL;
		
		if (stop)
			break;
		beg = end + 1;
	}
	
	if (i < 1)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Error: invalid arguments for command execution configuration option.");
		daemon_log(LOG_WARNING, OUT_PRE "You must specify at least one item: /path/to/executable_name");
		daemon_log(LOG_WARNING, OUT_PRE "Example: /usr/bin/gedit");
		daemon_log(LOG_WARNING, OUT_PRE "Then append optional arguments: /usr/bin/gedit --new-window /etc/btnx/btnx_config");
		
		return NULL;
	}
	
	return args;
}

/* Set event type, the command and its arguments for a command execution event */
static char *config_set_command(btnx_event *e, char *value)
{
	e->command = (char *) malloc((strlen(value) + 1)*sizeof(char));
	
	if (e->command == NULL)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Error: could not allocate command: %s", strerror(errno));
		return NULL;
	}
	e->keycode = COMMAND_EXECUTE;
	strcpy(e->command, value);
	
	e->args = config_split_command(e->command);
	if (e->args == NULL)
	{
		daemon_log(LOG_WARNING, OUT_PRE "Fatal error in config_split_command. Exiting...");
		exit(BTNX_ERROR_BAD_CONFIG);
	}
	
	return e->command;
	
}

/* Set the configuration switch type for an event. */
static void config_set_switch_type(btnx_event *e, char *value)
{
	int num;
	
	e->keycode = CONFIG_SWITCH;
	num = strtol(value, NULL, 10);
	e->switch_type = num;
}

/* Set the configuration switch name for an event. */
static void config_set_switch_name(btnx_event *e, char *value)
{
	if (e->switch_name != NULL)
		free(e->switch_name);
	e->switch_name = (char *) malloc((strlen(value)+1) * sizeof(char));
	strcpy(e->switch_name, value);
}

/* Compares the parsed option name to defined option names. If a match is found,
 * set the value to the correct variable in the event structure. */
static const char *config_add_value(btnx_event *e, 
									int type, 
									const char *option, 
									char *value)
{
	/* Button values */
	if (type == BLOCK_BUTTON)
	{
		if (!strcasecmp(option, "rawcode"))
		{
			e->rawcode = strtol(value, NULL, 16);
			return option;
		}
		if (!strcasecmp(option, "enabled"))
		{
			e->enabled = strtol(value, NULL, 10);
			return option;
		}
		if (!strcasecmp(option, "type"))
		{
			e->type = strtol(value, NULL, 10);
			return option;
		}
		if (!strcasecmp(option, "delay"))
		{
			e->delay = strtol(value, NULL, 10);
			return option;
		}
		if (!strcasecmp(option, "keycode"))
		{
			e->keycode = config_get_keycode(value);
			return option;
		}
		if (!strcasecmp(option, "mod1"))
		{
			config_add_mod(e, config_get_keycode(value));
			return option;
		}
		if (!strcasecmp(option, "mod2"))
		{
			config_add_mod(e, config_get_keycode(value));
			return option;
		}
		if (!strcasecmp(option, "mod3"))
		{
			config_add_mod(e, config_get_keycode(value));
			return option;
		}
		if (!strcasecmp(option, "command"))
		{
			config_set_command(e, value);
			return option;
		}
		if (!strcasecmp(option, "uid"))
		{
			e->uid = strtol(value, NULL, 10);
			return option;
		}
		if (!strcasecmp(option, "switch_type"))
		{
			config_set_switch_type(e, value);
			return option;
		}
		if (!strcasecmp(option, "switch_name"))
		{
			config_set_switch_name(e, value);
			return option;
		}
		if (!strcasecmp(option, "force_release"))
		{
			if (strtol(value, NULL, 10) == 1)
				e->type = BUTTON_RELEASE;
			return option;
		}
		if (!strcasecmp(option, "name"))
			return option;
	}
	/* Mouse values */
	else if (type == BLOCK_MOUSE)
	{
		if (!strcasecmp(option, "vendor_name"))
			return option;
		if (!strcasecmp(option, "product_name"))
			return option;
		if (!strcasecmp(option, "vendor_id"))
		{
			device_set_vendor_id(strtol(value, NULL, 16));
			return option;
		}
		if (!strcasecmp(option, "product_id"))
		{
			device_set_product_id(strtol(value, NULL, 16));
			return option;
		}
		if (!strcasecmp(option, "revoco_mode"))
		{
			revoco_set_mode(strtol(value, NULL, 10));
			return option;
		}
		if (!strcasecmp(option, "revoco_btn"))
		{
			revoco_set_btn(strtol(value, NULL, 10));
			return option;
		}
		if (!strcasecmp(option, "revoco_up_scroll"))
		{
			revoco_set_up_scroll(strtol(value, NULL, 10));
			return option;
		}
		if (!strcasecmp(option, "revoco_down_scroll"))
		{
			revoco_set_down_scroll(strtol(value, NULL, 10));
			return option;
		}
	}
	
	return NULL;
}


/* Parse a configuration file and return the btnx_event structures */
btnx_event **config_parse(char **config_name)
{
	FILE *fp;
	char buffer[CONFIG_PARSE_BUFFER_SIZE];
	char option[CONFIG_PARSE_OPTION_SIZE];
	char value[CONFIG_PARSE_VALUE_SIZE];
	char *loc_eq, *loc_com, *loc_beg, *loc_end;
	int block_begin = 0, block_end = 1;
	btnx_event **bevs;
	int i=-1, block_type=BLOCK_NONE;
	
	if ((*config_name = config_get_names(*config_name)) == NULL)
		sprintf(buffer,"%s/%s", CONFIG_PATH, CONFIG_NAME);
	else
		sprintf(buffer,"%s/%s_%s", CONFIG_PATH, CONFIG_NAME, *config_name);
	
	/* When looping through configurations, stop when reaching the original
	 * configuration. */
	if (first_config[0] == '\0')
		strcpy(first_config, *config_name);
	else if (strcmp(first_config, *config_name) == 0) {
		daemon_log(LOG_WARNING, OUT_PRE "Looped through all configurations. Stopping.");
		return NULL;
	}
	
	daemon_log(LOG_WARNING, OUT_PRE "Opening config file: %s", buffer);
	
	if (!(fp = fopen(buffer,"r")))
	{
		daemon_log(LOG_WARNING, OUT_PRE "Could not read the config file: %s", strerror(errno));
		return NULL;
	}
	
	bevs = (btnx_event **) calloc(MAX_BEVS+1, sizeof(btnx_event*));
	
	while (fgets(buffer, CONFIG_PARSE_BUFFER_SIZE-1, fp) != NULL)
	{
		if (buffer[0] != '#')
		{
			loc_eq = strchr(buffer, '=');
			loc_com = strchr(buffer,'#');
			loc_beg = buffer;
			while (isspace(*loc_beg)) loc_beg++;
			
			if (loc_eq && (loc_com == NULL || loc_com > loc_eq) && block_type != BLOCK_NONE)
			{
				if (loc_com != NULL && loc_com != loc_eq + 1)
					*loc_com = '\0';
				
				loc_beg = buffer;
				while (isspace(*loc_beg)) loc_beg++;
				loc_end = loc_eq;
				while (isspace(*(loc_end-1))) loc_end--;
				*loc_end = '\0';
				strcpy(option, loc_beg);
				
				loc_beg = loc_eq;
				while (isspace(*(loc_beg+1))) loc_beg++;
				loc_beg++;
				loc_end = loc_beg + strlen(loc_beg);
				while (isspace(*(loc_end-1))) loc_end--;
				*loc_end = '\0';		
				strcpy(value, loc_beg);
				
				if (!config_add_value(bevs[i], block_type, option, value))
					daemon_log(LOG_WARNING, OUT_PRE "Warning: parse error: %s = %s", option, value);
				
				memset(value, '\0', CONFIG_PARSE_VALUE_SIZE * sizeof(char));
				memset(option, '\0', CONFIG_PARSE_OPTION_SIZE * sizeof(char));
				memset(buffer, '\0', CONFIG_PARSE_BUFFER_SIZE * sizeof(char));
			}
			else if (*loc_beg == '\0')
				continue;
			else
			{
				loc_end = loc_beg;
				
				if (loc_com == NULL)
					loc_com = &buffer[strlen(buffer)-1];
				
				while (!isspace(*loc_end) && loc_end < loc_com && *loc_end != '\0') loc_end++;
				*loc_end = '\0';
				
				if (strcasecmp(loc_beg, CONFIG_BUTTON_BEGIN) == 0)
				{
					if (block_end == 0)
					{
						daemon_log(LOG_WARNING, OUT_PRE "Warning: config file parse error");
						continue;
					}
					block_begin = 1;
					i++;
					if (i >= MAX_BEVS)
						bevs = (btnx_event **) realloc(bevs, (i+2)*sizeof(bevs));
					bevs[i] = (btnx_event *) calloc(1, sizeof(btnx_event));
					bevs[i+1] = NULL;
					bevs[i]->enabled = 1;
					bevs[i]->delay = 0;
					bevs[i]->last.tv_sec = 0;
					bevs[i]->last.tv_usec = 0;
					bevs[i]->switch_type = CONFIG_SWITCH_NONE;
					bevs[i]->switch_name = NULL;
					bevs[i]->command = NULL;
					bevs[i]->args = NULL;
					block_type = BLOCK_BUTTON;
				}
				else if (strcasecmp(loc_beg, CONFIG_BUTTON_END) == 0)
				{
					if (block_begin == 0)
					{
						daemon_log(LOG_WARNING, OUT_PRE "Warning: config file parse error");
						continue;
					}
					block_end = 1;
					block_type = BLOCK_NONE;
				}
				else if (strcasecmp(loc_beg, CONFIG_MOUSE_BEGIN) == 0)
				{
					if (block_end == 0)
					{
						daemon_log(LOG_WARNING, OUT_PRE "Warning: config file parse error");
						continue;
					}
					block_begin = 1;
					block_type = BLOCK_MOUSE;
				}
				else if (strcasecmp(loc_beg, CONFIG_MOUSE_END) == 0)
				{
					if (block_begin == 0)
					{
						daemon_log(LOG_WARNING, OUT_PRE "Warning: config file parse error");
						continue;
					}
					block_end = 1;
					block_type = BLOCK_NONE;
				}
			}
		}
	}
	
	fclose(fp);
	
	return bevs;
}
