/*
** Copyright (c) 2006, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"

#include <sys/ptrace.h>
#include <linux/unistd.h>
#include <fcntl.h>


static char *
get_str(pid_t pid, unsigned long ptr)
{
	// FIXME: lame function
	char buf1[512];
	static char buf2[5120];
	int i = 0;
	int f;

	sprintf(buf1, "/proc/%ld/mem", pid);
	f = open(buf1, O_RDONLY);
	lseek(f, ptr, 0);
	while (1) {
		read(f, buf2+i, 1);
		if (buf2[i] == '\0')
			break;
		i++;
	}
	close(f);
	return buf2;
}

static int
path_arg_writable(char **pathlist, pid_t pid, int argno)
{
	unsigned long arg;
	char *path;

	arg = ptrace(PTRACE_PEEKUSER, pid, argno * 4, 0);
	path = get_str(pid, arg);
	if (!path_writable(pathlist, pid, path))
		return 0;

	return 1;
}

int
before_syscall(char **pathlist, pid_t pid, int syscall)
{
	// too much code duplication, using a table could be better

	if (syscall == __NR_open) {
		// open(path, flags, mode)
		unsigned long flags;
		flags = ptrace(PTRACE_PEEKUSER, pid, 4, 0);
		if (flags & O_WRONLY || flags & O_RDWR) {
			if (!path_arg_writable(pathlist, pid, 0))
				return -1;
		}
	}

	else if (syscall == __NR_creat) {
		// creat(path, mode)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_truncate) {
		// truncate(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_truncate64) {
		// truncate64(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_unlink) {
		// unlink(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_link) {
		// link(oldname, newname)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
		if (!path_arg_writable(pathlist, pid, 1))
			return -1;
	}

	else if (syscall == __NR_symlink) {
		// symlink(oldname, newname)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
		if (!path_arg_writable(pathlist, pid, 1))
			return -1;
	}

	else if (syscall == __NR_rename) {
		// rename(oldname, newname)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
		if (!path_arg_writable(pathlist, pid, 1))
			return -1;
	}

	else if (syscall == __NR_mknod) {
		// mknod(path, mode, dev)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_chmod) {
		// chmod(path, mode)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_lchown) {
		// lchown(path, uid, gid)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_chown) {
		// chown(path, ...)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_lchown32) {
		// lchown32(path, uid, gid)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_chown32) {
		// chown32(path, ...)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_mkdir) {
		// mkdir(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_rmdir) {
		// rmdir(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_mount) {
		// mount(path, ...)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_umount) {
		// umount(path)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	else if (syscall == __NR_utime) {
		// utime(path, times)
		if (!path_arg_writable(pathlist, pid, 0))
			return -1;
	}

	return 0;
}
