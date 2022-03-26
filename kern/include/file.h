/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */
// file descriptor
struct fd {
    off_t fp;
    struct vnode *vptr;
};

int sys_open(const_userptr_t filenameoff_t, int flags, mode_t mode);
// ssize_t sys_read(int fd, void *buf, size_t buflen);
// ssize_t sys_write(int fd, const void *buf, size_t nBytes);
// off_t sys_lseek(int fd, off_t pos, int whence);
int sys_close(int fd);
// int sys_dup2(int oldfd, int newfd);

#endif /* _FILE_H_ */
