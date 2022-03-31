/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>

#define CLOSED_FILE -1

/*
 * Put your function declarations and data types here ...
 */
// File descriptor table
struct file_descriptor_table {
	int FDs[OPEN_MAX];
};

// Open file table entry
struct open_file {
	struct vnode *vptr;
	off_t offset;
	int flags;
	uint32_t refcount;
};

// Open file table
struct open_file_table {
	struct open_file *OFs[OPEN_MAX]; // Array of the open files
	struct lock *OF_table_lock; // Open File lock
};

struct open_file_table *OF_table;

int init_OF_table(void);
int init_FD_table(void);

int sys_open(userptr_t filenameoff_t, int flags, mode_t mode, int *retval);
// ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval);
// ssize_t sys_write(int fd, const void *buf, size_t nBytes, int *retval);
off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval);
int sys_close(int fd);
int sys_dup2(int oldfd, int newfd, int *retval);
ssize_t sys_read(int FD, void *buf, size_t buflen, int *retval);
ssize_t sys_write(int FD, void *buf, size_t nBytes, int *retval);
// off_t sys_lseek(int fd, off_t pos, int whence, int *retval);
int sys_close(int FD);
// int sys_dup2(int oldfd, int newfd, int *retval);

// Helper Functions
int valid_FD(int FD);
int file_open(char *filename, int flags, mode_t mode, int *retval);
int file_rw(int FD, void *buf, size_t nbytes, mode_t mode, int *retval);

#endif /* _FILE_H_ */
