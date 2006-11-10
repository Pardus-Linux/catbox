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
#include <linux/user.h>


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
path_arg_writable(struct trace_context *ctx, pid_t pid, int argno, const char *name)
{
	unsigned long arg;
	char *path;

	arg = ptrace(PTRACE_PEEKUSER, pid, argno * 4, 0);
	path = get_str(pid, arg);
	if (!path_writable(ctx->pathlist, pid, path)) {
		if (ctx->log_func != Py_None) {
			PyObject *args;
			PyObject *list;
			PyObject *a = PyString_FromString(name);
			PyObject *b = PyString_FromString(path);
			args = PyTuple_New(2);
			PyTuple_SetItem(args, 0, PyString_FromString("denied"));
			list = PyList_New(2);
			PyList_SetItem(list, 0, a);
			PyList_SetItem(list, 1, b);
			//PyObject_SetAttrString(list, "operation", a);
			//PyObject_SetAttrString(list, "path", b);
			PyTuple_SetItem(args, 1, list);
			PyObject_Call(ctx->log_func, args, NULL);
		}
		return 0;
	}

	return 1;
}

#define CHECK_PATH 1
#define CHECK_PATH2 2

// TRAP_xxxxx check for remode operations
// xxown(): get the real uid/gid, store path and replace with process uid/gid
// xxmod(): get the real mode
// xxid(): return uid=0, gid=0
#define TRAP_xxOWN 4
#define TRAP_xxMOD 8
#define TRAP_xxID  16

static struct syscall_def {
	int no;
	unsigned int flags;
	const char *name;
} system_calls[] = {
	{ __NR_open,       CHECK_PATH, "open" },
	{ __NR_creat,      CHECK_PATH, "creat" },
	{ __NR_truncate,   CHECK_PATH, "truncate" },
	{ __NR_truncate64, CHECK_PATH, "truncate64" },
	{ __NR_unlink,     CHECK_PATH, "unlink" },
	{ __NR_link,       CHECK_PATH | CHECK_PATH2, "link" },
	{ __NR_symlink,    CHECK_PATH | CHECK_PATH2, "symlink" },
	{ __NR_rename,     CHECK_PATH | CHECK_PATH2, "rename" },
	{ __NR_mknod,      CHECK_PATH, "mknod" },
	{ __NR_chmod,      CHECK_PATH | TRAP_xxMOD, "chmod" },
	{ __NR_lchown,     CHECK_PATH | TRAP_xxOWN, "lchown" },
	{ __NR_chown,      CHECK_PATH | TRAP_xxOWN, "chown" },
	{ __NR_lchown32,   CHECK_PATH | TRAP_xxOWN, "lchown32" },
	{ __NR_chown32,    CHECK_PATH | TRAP_xxOWN, "chown32" },
	{ __NR_mkdir,      CHECK_PATH, "mkdir" },
	{ __NR_rmdir,      CHECK_PATH, "rmdir" },
	{ __NR_mount,      CHECK_PATH, "mount" },
	{ __NR_umount,     CHECK_PATH, "umount" },
	{ __NR_utime,      CHECK_PATH, "utime" },
    { __NR_getuid,     TRAP_xxID, "getuid" },
    { __NR_getuid32,   TRAP_xxID, "getuid32" },
    { __NR_geteuid,    TRAP_xxID, "geteuid" },
    { __NR_geteuid32,  TRAP_xxID, "geteuid32" },
    { __NR_getgid,     TRAP_xxID, "getgid" },
    { __NR_getgid32,   TRAP_xxID, "getgid32" },
    { __NR_getegid,    TRAP_xxID, "getegid" },
    { __NR_getegid32,  TRAP_xxID, "getegid32" },
	{ 0, 0, NULL }
};

int
before_syscall(struct trace_context *ctx, pid_t pid, int syscall)
{
	int i;
	unsigned int flags;
	const char *name;
	// exception, we have to check access mode for open
	if (syscall == __NR_open) {
		// open(path, flags, mode)
		flags = ptrace(PTRACE_PEEKUSER, pid, 4, 0);
		if (flags & O_WRONLY || flags & O_RDWR) {
			if (!path_arg_writable(ctx, pid, 0, "open"))
				return -1;
		}
		return 0;
	}

	for (i = 0; system_calls[i].name; i++) {
		if (system_calls[i].no == syscall)
			goto found;
	}
	return 0;
found:
	flags = system_calls[i].flags;
	name = system_calls[i].name;

	if (flags & CHECK_PATH) {
		if (!path_arg_writable(ctx, pid, 0, name))
			return -1;
	}

	if (flags & CHECK_PATH2) {
		if (!path_arg_writable(ctx, pid, 1, name))
			return -1;
	}

    //below we only trap changes to owner/mode within the fishbowl. 
    // The rest are taken care of in the above blocks
    if(flags & TRAP_xxOWN) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
        const char* path = get_str(pid, regs.ebx);
        uid_t uid = (uid_t)regs.ecx;
        gid_t gid = (gid_t)regs.edx;
        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "ownerships" );
        PyDict_SetItem( dict, PyString_FromString(path), PyTuple_Pack( 2, PyInt_FromLong(uid), PyInt_FromLong(gid)) );
        return 1;
    }
    if(flags & TRAP_xxMOD) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
        const char* path = get_str(pid, regs.ebx);
        mode_t mode = (mode_t)regs.ecx;
        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "modes" );
        PyDict_SetItem( dict, PyString_FromString(path), PyInt_FromLong(mode) );
        return 1;
    }
    if(flags & TRAP_xxID) {
        return 2;
    }
	return 0;
}
