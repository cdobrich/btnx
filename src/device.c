
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */
  
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

#include "btnx.h"
#include "device.h"

/* Static variables */
static int product_id=0;
static int vendor_id=0;


int device_get_vendor_id(void) {
	return vendor_id;
}

int device_get_product_id(void) {
	return product_id;
}

void device_set_vendor_id(int id) {
	vendor_id = id;
}

void device_set_product_id(int id) {
	product_id = id;
}

/* Initialize the device_fds_t structure */
void device_fds_init(struct device_fds_t *dev_fds) {
	dev_fds->count = 0;
	dev_fds->fd = NULL;
	dev_fds->max_fd = NULL_FD;
}

/* Set the maximum file descriptor in a struct device_fds_t */
void device_fds_set_max_fd(struct device_fds_t *dev_fds) {
	int i;
	
	dev_fds->max_fd = NULL_FD;
	for (i = 0; i < dev_fds->count; i++) {
		if (dev_fds->max_fd < dev_fds->fd[i])
			dev_fds->max_fd = dev_fds->fd[i];
	}
}

/* Fills an fd_set with file descriptors from device_fds_t */
void device_fds_fill_fds(struct device_fds_t *dev_fds, fd_set *fds) {
	int i;
	
	for (i = 0; i < dev_fds->count; i++) {
		FD_SET(dev_fds->fd[i], fds);
	}
}

/* Finds a set fd in an fd_set */
int device_fds_find_set_fd(struct device_fds_t *dev_fds, fd_set *fds) {
	int i;
	
	for (i = 0; i < dev_fds->count; i++) {
		if (FD_ISSET(dev_fds->fd[i], fds))
			return dev_fds->fd[i];
	}
	return NULL_FD;
}

/* Add a file descriptor to a device_fds_t */
void device_fds_add_fd(struct device_fds_t *dev_fds, int fd) {
	if (dev_fds->count == 0)
		dev_fds->fd = malloc(sizeof(fd));
	else
		dev_fds->fd = realloc(dev_fds->fd, sizeof(fd) * (dev_fds->count + 1));
	
	dev_fds->fd[dev_fds->count] = fd;
	dev_fds->count++;
}

/* Close all file descriptors of a device_fds_t */
void device_fds_close(struct device_fds_t *dev_fds) {
	int i;
	
	if (!(dev_fds->count))
		return;
	for (i = 0; i < dev_fds->count; i++) {
		close(dev_fds->fd[i]);
	}
	free(dev_fds->fd);
	device_fds_init(dev_fds);
}
