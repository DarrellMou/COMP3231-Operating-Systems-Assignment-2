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


#endif /* _FILE_H_ */
