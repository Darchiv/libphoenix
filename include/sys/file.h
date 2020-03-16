/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * sys/fs
 *
 * Copyright 2017-2018 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _LIBPHOENIX_SYS_FILE_H_
#define _LIBPHOENIX_SYS_FILE_H_

#include <sys/types.h>


enum { otDir = 0, otFile, otDev, otSymlink, otUnknown };

/* TODO: make those ioctls or include in kernel message types? */
enum { mtUmount = 0xfaf2, mtSync };


typedef struct {
	long id;
	unsigned mode;
	char fstype[16];
} mount_msg_t;


extern int fileAdd(unsigned int *h, oid_t *oid, unsigned mode);


extern int fileSet(unsigned int h, char flags, oid_t *oid, offs_t offs, unsigned mode);


extern int fileGet(unsigned int h, char flags, oid_t *oid, offs_t *offs, unsigned *mode);


extern int fileRemove(unsigned int h);


extern int flock(int fd, int operation);


#endif
