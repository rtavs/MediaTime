
#ifndef __CAMSS_H__
#define __CAMSS_H__



typedef int (*camss_data_cb)(void *handle, void *data);


void *camss_open(const char *devname, int width, int height, int frate);

int camss_close(void *handle);

// stream on
int camss_start(void *handle);

//stream off
int camss_stop(void *handle);


// install camera data callback
int camss_install_cb(void *handle, camss_data_cb callback);


#endif
