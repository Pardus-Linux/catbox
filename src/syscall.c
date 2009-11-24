/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"

#include <sys/ptrace.h>
#include <sys/user.h>
#include <linux/unistd.h>
#include <fcntl.h>

// System call dispatch flags
#define CHECK_PATH    1  // First argument should be a valid path
#define CHECK_PATH2   2  // Second argument should be a valid path
#define DONT_FOLLOW   4  // Don't follow last symlink in the path while checking
#define OPEN_MODE     8  // Check the Write mode of open flags
#define LOG_OWNER    16  // Don't do the chown operation but log the new owner
#define LOG_MODE     32  // Don't do the chmod operation but log the new mode
#define FAKE_ID      64  // Fake builder identity as root
#define NET_CALL    128  // System call depends on network allowed flag

// System call dispatch table
static struct syscall_def {
	int no;
	const char *name;
	unsigned int flags;
} system_calls[] = {
	{ __NR_open,       "open",       CHECK_PATH | OPEN_MODE },
	{ __NR_creat,      "creat",      CHECK_PATH },
	{ __NR_truncate,   "truncate",   CHECK_PATH },
#ifdef __i386__
	{ __NR_truncate64, "truncate64", CHECK_PATH },
#endif
	{ __NR_unlink,     "unlink",     CHECK_PATH | DONT_FOLLOW },
	{ __NR_link,       "link",       CHECK_PATH | CHECK_PATH2 },
	{ __NR_symlink,    "symlink",    CHECK_PATH2 | DONT_FOLLOW },
	{ __NR_rename,     "rename",     CHECK_PATH | CHECK_PATH2 },
	{ __NR_mknod,      "mknod",      CHECK_PATH },
	{ __NR_chmod,      "chmod",      CHECK_PATH | LOG_MODE },
	{ __NR_lchown,     "lchown",     CHECK_PATH | LOG_MODE | DONT_FOLLOW },
	{ __NR_chown,      "chown",      CHECK_PATH | LOG_OWNER },
#ifdef __i386__
	{ __NR_lchown32,   "lchown32",   CHECK_PATH | LOG_OWNER | DONT_FOLLOW },
	{ __NR_chown32,    "chown32",    CHECK_PATH | LOG_OWNER },
#endif
	{ __NR_mkdir,      "mkdir",      CHECK_PATH },
	{ __NR_rmdir,      "rmdir",      CHECK_PATH },
	{ __NR_mount,      "mount",      CHECK_PATH },
#ifndef __i386__
	{ __NR_umount2,    "umount",     CHECK_PATH },
#else
	{ __NR_umount,     "umount",     CHECK_PATH },
#endif
	{ __NR_utime,      "utime",      CHECK_PATH },
	{ __NR_getuid,     "getuid",     FAKE_ID },
	{ __NR_geteuid,    "geteuid",    FAKE_ID },
	{ __NR_getgid,     "getgid",     FAKE_ID },
	{ __NR_getegid,    "getegid",    FAKE_ID },
#ifdef __i386__
	{ __NR_getuid32,   "getuid32",   FAKE_ID },
	{ __NR_geteuid32,  "geteuid32",  FAKE_ID },
	{ __NR_getgid32,   "getgid32",   FAKE_ID },
	{ __NR_getegid32,  "getegid32",  FAKE_ID },
#endif
#ifndef __i386__
	{ __NR_socket,     "socketcall", NET_CALL },
#else
	{ __NR_socketcall, "socketcall", NET_CALL },
#endif
	{ 0, NULL, 0 }
};

// Architecture dependent register offsets
#ifndef __i386__
// x64
#define orig_eax orig_rax
#define eax rax
#define R_ARG1 112
#define R_ARG2 104
#define R_CALL 80
#define R_ERROR 120
#else
// i386
#define R_ARG1 0
#define R_ARG2 4
#define R_CALL 44
#define R_ERROR 24
#endif

static char *
get_str(pid_t pid, unsigned long ptr)
{
	// FIXME: lame function
	char buf1[512];
	static char buf2[5120];
	int i = 0;
	int f;

	sprintf(buf1, "/proc/%d/mem", pid);
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
path_arg_writable(struct trace_context *ctx, pid_t pid, char *path, const char *name, int dont_follow)
{
	char *canonical;
	int ret;
	int mkdir_case;
	int err = 0;

	canonical = catbox_paths_canonical(pid, path, dont_follow);
	if (canonical) {
		mkdir_case = strcmp("mkdir", name) == 0;
		ret = path_writable(ctx->pathlist, canonical, mkdir_case);
		if (ret == 0) {
			if (strcmp("open", name) == 0) {
				// Special case for kernel build
				unsigned int flags;
				struct stat st;
				flags = ptrace(PTRACE_PEEKUSER, pid, R_ARG2, 0);
				if ((flags & O_CREAT) == 0 && stat(canonical, &st) == -1 && errno == ENOENT) {
					free(canonical);
					return ENOENT;
				}
			}
			catbox_retval_add_violation(ctx, name, path, canonical);
			err = -EACCES;
		} else if (ret == -1) {
			err = -EEXIST;
		}
		free(canonical);
	} else {
		if (errno == ENAMETOOLONG)
			err = -ENAMETOOLONG;
		else if (errno == ENOENT)
			err = -ENOENT;
		else
			err = -EACCES;
	}

	return err;
}

static int
handle_syscall(struct trace_context *ctx, pid_t pid, int syscall)
{
	int i;
	int ret;
	unsigned long arg;
	char *path;
	unsigned int flags;
	const char *name;

	for (i = 0; system_calls[i].name; i++) {
		if (system_calls[i].no == syscall)
			goto found;
	}
	return 0;
found:
	flags = system_calls[i].flags;
	name = system_calls[i].name;

	if (flags & CHECK_PATH) {
		arg = ptrace(PTRACE_PEEKUSER, pid, R_ARG1, 0);
		path = get_str(pid, arg);
		if (flags & OPEN_MODE) {
			flags = ptrace(PTRACE_PEEKUSER, pid, R_ARG2, 0);
			if (!(flags & O_WRONLY || flags & O_RDWR)) return 0;
		}
		ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);
		if (ret) return ret;
	}

	if (flags & CHECK_PATH2) {
		arg = ptrace(PTRACE_PEEKUSER, pid, R_ARG2, 0);
		path = get_str(pid, arg);
		ret = path_arg_writable(ctx, pid, path, name, flags & DONT_FOLLOW);
		if (ret) return ret;
	}

	if (flags & NET_CALL && !ctx->network_allowed) {
		catbox_retval_add_violation(ctx, name, "", "");
		return -EACCES;
	}

return 0;
    //below we only trap changes to owner/mode within the fishbowl. 
    // The rest are taken care of in the above blocks
    if(0 & LOG_OWNER) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
//        const char* path = get_str(pid, regs.ebx);
//        uid_t uid = (uid_t)regs.ecx;
//        gid_t gid = (gid_t)regs.edx;
//        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "ownerships" );
//        PyDict_SetItem( dict, PyString_FromString(path), PyTuple_Pack( 2, PyInt_FromLong(uid), PyInt_FromLong(gid)) );
        return 1;
    }
    if(0 & LOG_MODE) {
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, pid, 0, &regs);
//        const char* path = get_str(pid, regs.ebx);
//        mode_t mode = (mode_t)regs.ecx;
//        PyObject* dict = PyObject_GetAttrString( ctx->ret_object, "modes" );
//        PyDict_SetItem( dict, PyString_FromString(path), PyInt_FromLong(mode) );
        return 1;
    }
    if(0 & FAKE_ID) {
        return 2;
    }
	return 0;
}

void
catbox_syscall_handle(struct trace_context *ctx, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct regs;
	pid_t pid;

	pid = kid->pid;
	ptrace(PTRACE_GETREGS, pid, 0, &regs);
	syscall = regs.orig_eax;

	if (kid->in_syscall) {
		// returning from syscall
		if (syscall == 0xbadca11) {
			// restore real call number, and return our error code
			ptrace(PTRACE_POKEUSER, pid, R_CALL, kid->orig_call);
			ptrace(PTRACE_POKEUSER, pid, R_ERROR, kid->error_code);
		}
		kid->in_syscall = 0;
	} else {
		// entering syscall

		// skip extra sigtrap from execve call
		if (syscall == __NR_execve) {
			kid->in_execve = 1;
			goto out;
		}

		int ret = handle_syscall(ctx, pid, syscall);
		if (ret != 0) {
			kid->error_code = ret;
			kid->orig_call = regs.orig_eax;
			// prevent the call by giving an invalid call number
			ptrace(PTRACE_POKEUSER, pid, R_CALL, 0xbadca11);
		}
		kid->in_syscall = 1;
	}
out:
	// continue tracing
	ptrace(PTRACE_SYSCALL, pid, 0, 0);
}
