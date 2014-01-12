/*
 * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
 * see btnx.c for detailed license information
 *
 * Revoco code removed because of licensing issues.
 * The remaining code is licensed under the same
 * license as the rest of the btnx code.
 */

#include <stdio.h>
#include <stdlib.h>

#include "revoco.h"
#include "../config.h"
#include "btnx.h"

/* Static variables */
static int revoco_mode=0;
static int revoco_btn=3;
static int revoco_up_scroll=0;
static int revoco_down_scroll=0;

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
    fprintf(stderr, OUT_PRE "revoco has been removed from this build.\n");
    return 0;
}
