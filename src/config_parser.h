
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */

#ifndef CONFIG_PARSER_H_
#define CONFIG_PARSER_H_

#define CONFIG_NAME				"btnx_config"
//#define CONFIG_PATH				"/etc/btnx"
#define EVENTS_NAME				"events"
//#define DEFAULTS_CONFIG_PATH	"/etc/btnx/defaults"
//#define DEFAULT_CONFIG_NAME		"default_config_"
#define CONFIG_MANAGER_FILE		CONFIG_PATH "/btnx_manager"

#define CONFIG_PARSE_BUFFER_SIZE			512
#define CONFIG_PARSE_OPTION_SIZE			64
#define CONFIG_PARSE_VALUE_SIZE				512
#define CONFIG_NAME_MAX_SIZE				64

#define IS_ENCLOSING(c) ((c) == '\'' || (c) == '"' || (c) == '`')


/* Return configuration file names */
const char *config_get_next(void);
const char *config_get_prev(void);

/* Parse the configuration file */
btnx_event **config_parse(char **config_name);

#endif /*CONFIG_PARSER_H_*/
