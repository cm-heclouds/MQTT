#ifndef ONENET_CONFIG_H
#define ONENET_CONFIG_H

#include <stddef.h>

#ifdef WIN32
#pragma warning(disable:4819)
#pragma warning(disable:4996)
#define inline __inline
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#else // UNIX
#include <sys/uio.h>
#endif // _WIN32

#define MQTT_DEFAULT_ALIGNMENT sizeof(int)

#endif // ONENET_CONFIG_H
