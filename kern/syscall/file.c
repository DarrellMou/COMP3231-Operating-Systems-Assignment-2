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

int sys_open(const_userptr_t filenameoff_t, int flags, mode_t mode) {
    char filename[NAME_MAX];
    size_t filesize;
    int res;
    copyinstr(filenameoff_t, filename, NAME_MAX, &filesize);

    int fd;
    for (fd = 3; fd < OPEN_MAX; fd++) {
        if (curproc->ofptrs[fd] != NULL) continue;

        curproc->ofptrs[fd] = kmalloc(sizeof(struct fd));

        res = vfs_open(filename, flags, mode, &curproc->ofptrs[fd]->vptr);
        if (res) {
            kfree(curproc->ofptrs[fd]);
            return res;
        }
        break;
    }

    if (fd < OPEN_MAX + 1) return fd;

    return EMFILE;
}

// ssize_t sys_read(int fd, void *buf, size_t buflen) {

// }

// ssize_t sys_write(int fd, const void *buf, size_t nBytes) {

// }

// off_t sys_lseek(int fd, off_t pos, int whence) {

// }

int sys_close(int fd) {
    vfs_close(curproc->ofptrs[fd]->vptr);
    kfree(curproc->ofptrs[fd]);
    return 0;
}

// int sys_dup2(int oldfd, int newfd) {

// }