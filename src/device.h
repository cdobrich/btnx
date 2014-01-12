
 /* 
  * Copyright (C) 2007  Olli Salonen <oasalonen@gmail.com>
  * see btnx.c for detailed license information
  */
  
#ifndef DEVICE_H_
#define DEVICE_H_

/* Contains all device fds to listen to */
struct device_fds_t {
	int count;
	int *fd;
	int max_fd;
};

int device_get_vendor_id(void);
int device_get_product_id(void);
void device_set_vendor_id(int id);
void device_set_product_id(int id);

void device_fds_init(struct device_fds_t *dev_fds);
void device_fds_set_max_fd(struct device_fds_t *dev_fds);
void device_fds_fill_fds(struct device_fds_t *dev_fds, fd_set *fds);
int device_fds_find_set_fd(struct device_fds_t *dev_fds, fd_set *fds);
void device_fds_add_fd(struct device_fds_t *dev_fds, int fd);
void device_fds_close(struct device_fds_t *dev_fds);

#endif /*DEVICE_H_*/
