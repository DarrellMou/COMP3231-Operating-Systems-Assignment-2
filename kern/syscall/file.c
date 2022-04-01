#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */
int valid_FD(int FD) {

    if (FD < 0 || FD >= OPEN_MAX) {
        return -1;
    }
    int OF_key = curproc->FD_table->FDs[FD];
    if (OF_table->OFs[OF_key] == NULL) {
        return -1;
    }
    return OF_key;
}

int init_OF_table(void) {

	if (OF_table == NULL) {
		OF_table = kmalloc(sizeof(struct open_file_table));
		if (OF_table == NULL) {
            return ENOMEM;
        }

		OF_table->OF_table_lock = lock_create("OF_table_lock");
        if (OF_table->OF_table_lock == NULL) {
            return ENOMEM;
		}

	    for(int i = 0; i < OPEN_MAX; i++) {
            OF_table->OFs[i] = NULL;
        }
	}

	return 0;
}

int init_FD_table(void) {

	int result;
	int FD;

	curproc->FD_table = kmalloc(sizeof(struct file_descriptor_table));
	if (curproc->FD_table == NULL) {
        return ENOMEM;
    }
    
	for (int i = 0; i < OPEN_MAX; i++) {
        curproc->FD_table->FDs[i] = CLOSED_FILE;
    }

	char pathStdin[5] = "con:\0";
	result = file_open(pathStdin, O_RDONLY, 0, &FD);
	if (result) {
		kfree(curproc->FD_table);
		return result;
	}

	char pathStdout[5] = "con:\0";
	result = file_open(pathStdout, O_WRONLY, 0, &FD);
	if (result) {
		kfree(curproc->FD_table);
		return result;
	}

	char pathStderror[5] = "con:\0";
	result = file_open(pathStderror, O_WRONLY, 0, &FD);
	if (result) {
		kfree(curproc->FD_table);
		return result;
	}

	return 0;
}

int file_open(char *filename, int flags, mode_t mode, int *retval) {

	int allflags = O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;
	int result;
	int FD = -1;
	int OF = -1;
	struct vnode *vn;

	if ((flags & allflags) != flags) {
		return EINVAL;
	}

	result = vfs_open(filename, flags, mode, &vn);
	if (result) {
		return result;
	}

	struct file_descriptor_table *FD_table = curproc->FD_table;

	lock_acquire(OF_table->OF_table_lock);

	for (int i = 0; i < OPEN_MAX; i++) {
		if (FD_table->FDs[i] == CLOSED_FILE) {
			FD = i;
			break;
		}
	}

	for (int i = 0; i < OPEN_MAX; i++) {
		if (OF_table->OFs[i] == NULL) {
			OF = i;
			break;
		}
	}

	if (FD == -1 || OF == -1) {
		vfs_close(vn);
		lock_release(OF_table->OF_table_lock);
		if (FD == -1) return EMFILE;
		if (OF == -1) return ENFILE;
	}

	struct open_file *OF_entry = kmalloc(sizeof(struct open_file));
	if (OF_entry == NULL) {
		vfs_close(vn);
		lock_release(OF_table->OF_table_lock);
		return ENOMEM;
	}

	FD_table->FDs[FD] = OF;

	OF_entry->vptr = vn;
	OF_entry->refcount = 1;
	OF_entry->flags = flags;
	OF_entry->offset = 0;

	if (flags & O_APPEND) {
        struct stat inode;
        int result;
        result = VOP_STAT(OF_entry->vptr, &inode);
        if (result) {
            return result;
        }
		OF_entry->offset = inode.st_size;
	}
	OF_table->OFs[OF] = OF_entry;

	lock_release(OF_table->OF_table_lock);

	*retval = FD;

	return 0;
}

int sys_open(userptr_t filenameoff_t, int flags, mode_t mode, int *retval) {
    char filename[PATH_MAX];
	int result;

	result = copyinstr(filenameoff_t, filename, PATH_MAX, NULL);
	if (result) {
		return result;
	}
	
    return file_open(filename, flags, mode, retval);
}

ssize_t sys_read(int FD, void *buf, size_t buflen, int *retval) {
    return file_rw(FD, buf, buflen, UIO_READ, retval);
}

ssize_t sys_write(int FD, void *buf, size_t nBytes, int *retval) {
    return file_rw(FD, buf, nBytes, UIO_WRITE, retval);
}

int file_rw(int FD, void *buf, size_t nbytes, mode_t mode, int *retval) {
    lock_acquire(OF_table->OF_table_lock);
    int OFT_key = valid_FD(FD);
    if (OFT_key < 0) {
        lock_release(OF_table->OF_table_lock);
        return EBADF;
    }
    struct open_file *OF = OF_table->OFs[OFT_key];

    if (mode == UIO_WRITE) {
        switch (OF->flags & O_ACCMODE) {
            case O_WRONLY:
                break;
            case O_RDWR:
                break;
            default:
                lock_release(OF_table->OF_table_lock);
                return EBADF;
        }
    }

    if (mode == UIO_READ) {
        switch (OF->flags & O_ACCMODE) {
            case O_RDONLY:
                break;
            case O_RDWR:
                break;
            default:
                lock_release(OF_table->OF_table_lock);
                return EBADF;
        }
    }

    int is_seekable = VOP_ISSEEKABLE(OF->vptr);
    off_t offset = 0;
    if (is_seekable) {
        offset = OF->offset;
    }

    struct iovec iov;
    struct uio ku;
    uio_kinit(&iov, &ku, buf, nbytes, offset, mode);

    int result;
    struct vnode *vn = OF->vptr;
    if (mode == UIO_WRITE) {
        result = VOP_WRITE(vn, &ku);
    } else {
        result = VOP_READ(vn, &ku);
    }

    if (result) {
        lock_release(OF_table->OF_table_lock);
        return result;
    }

    OF->offset = ku.uio_offset;
    lock_release(OF_table->OF_table_lock);

    *retval = nbytes - ku.uio_resid;

    return 0;
}

off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval) {
	int OF_key = valid_FD(fd);
	off_t new_pos = -1;
	if (OF_key < 0) {
		return EBADF;
	}

	lock_acquire(OF_table->OF_table_lock);

	struct open_file *OF_entry = OF_table->OFs[OF_key];

	if (!VOP_ISSEEKABLE(OF_entry->vptr)) {
		lock_release(OF_table->OF_table_lock);
		return ESPIPE; // does not support seeking
	}

	if (whence == SEEK_SET) {
		if (pos < 0) return EINVAL; // pos less than 0
		new_pos = pos;
		OF_entry->offset = new_pos;
	}

	else if (whence == SEEK_CUR) {
		new_pos = OF_entry->offset + pos;
		if (new_pos < 0) return EINVAL; // new_pos less than 0
		OF_entry->offset = new_pos;
	}

	else if (whence == SEEK_END) {
		struct stat inode;
        int v;
        v = VOP_STAT(OF_entry->vptr, &inode);
        if (v) {
            return v;
        }
		new_pos = pos + inode.st_size; // can seek beyond EOF
		OF_entry->offset = new_pos;
	}

	else {
		lock_release(OF_table->OF_table_lock);
		return EINVAL; // whence is invalid
	}

	lock_release(OF_table->OF_table_lock);
	*retval = new_pos;
	return new_pos;
}

int sys_close(int FD) {

    lock_acquire(OF_table->OF_table_lock);
    int OF_key = valid_FD(FD);
    if (OF_key < 0) {
        lock_release(OF_table->OF_table_lock);
        return EBADF;
    }

    struct open_file *OF = OF_table->OFs[OF_key];

    vfs_close(OF->vptr);

    OF->refcount--;
    if (OF->refcount == 0) {
    	kfree(OF);
        OF_table->OFs[OF_key] = NULL;
    }
    lock_release(OF_table->OF_table_lock);
    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval) {
	int FD = -1;
	int OF = -1;
	
	struct file_descriptor_table *FD_table = curproc->FD_table;
	lock_acquire(OF_table->OF_table_lock);
	int old_OF_key = valid_FD(oldfd);
	int new_OF_key = valid_FD(newfd);

	// check not a valid handle file
	if (old_OF_key < 0) {
		return EBADF;
	}

	if (newfd < 0 || newfd >= OPEN_MAX) {
		return EBADF;
	}

	// clone on to itself(no effect)
	if (oldfd == newfd) {
		return oldfd;
	}

	// Check EMFILE and ENFILE
	for (int i = 0; i < OPEN_MAX; i++) {
		if (FD_table->FDs[i] == CLOSED_FILE) {
			FD = i;
			break;
		}
	}

	for (int i = 0; i < OPEN_MAX; i++) {
		if (OF_table->OFs[i] == NULL) {
			OF = i;
			break;
		}
	}

	if (FD == -1 || OF == -1) {
		lock_release(OF_table->OF_table_lock);
		if (FD == -1) return EMFILE; // too many opened file in the process
		if (OF == -1) return ENFILE; // too many opened file in entire system
	}
	
	// if newfd is open, closed it
	if (OF_table->OFs[new_OF_key] != NULL) {
		int check = sys_close(newfd);
		// if there's an error
		if (check) {
			lock_release(OF_table->OF_table_lock);
			return check;
		}
	}
	// points newfd to the same key
	struct open_file *OF_ent = OF_table->OFs[old_OF_key];
	FD_table->FDs[newfd] = old_OF_key;
	OF_ent->refcount++;
	lock_release(OF_table->OF_table_lock);
	*retval = newfd;

	return newfd;

}