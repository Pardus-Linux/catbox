/*
** Copyright (c) 2006, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <linux/user.h>
#include <linux/unistd.h>
#include <fcntl.h>

struct traced_child {
	pid_t pid;
	int proc_mem_fd;
	int need_setup;
	int in_syscall;
	unsigned long orig_eax;
};

static void
handle_syscall(char **pathlist, pid_t pid, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct u_in, u_out;

	ptrace(PTRACE_GETREGS, pid, 0, &u_in);
	syscall = u_in.orig_eax;

	if (before_syscall(pathlist, pid, syscall) != 0) {
		kid->orig_eax = u_in.orig_eax;
		ptrace(PTRACE_POKEUSER, pid, 44, 0xbadca11);
	}
	ptrace(PTRACE_SYSCALL, pid, 0, 0);
}

static void
handle_syscall_return(pid_t pid, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct u_in, u_out;
	ptrace(PTRACE_GETREGS, pid, 0, &u_in);
	syscall = u_in.orig_eax;
	if (syscall == 0xbadca11) {
		ptrace(PTRACE_POKEUSER, pid, 44, kid->orig_eax);
		ptrace(PTRACE_POKEUSER, pid, 24, -13);
	}

	ptrace(PTRACE_SYSCALL, pid, 0, 0);
}

int
setup_kid(pid_t pid)
{
	int e;
	e = ptrace(PTRACE_SETOPTIONS, pid, 0,
		PTRACE_O_TRACECLONE |PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK);
	if (e != 0)
	printf("ptrace opts error %s\n",strerror(errno));
}

struct traced_children {
	struct traced_child **children;
	unsigned int max_children;
	unsigned int nr_children;
};

static struct traced_child *
add_child(struct traced_children *kids, pid_t pid)
{
	struct traced_child *kid;

	kid = calloc(1, sizeof(struct traced_child));
	if (!kid) return NULL;
	kid->pid = pid;
	kid->need_setup = 1;
	// FIXME: check nr < max!!
	kids->children[kids->nr_children++] = kid;
	return kid;
}

static struct traced_child *
find_child(struct traced_children *kids, pid_t pid)
{
	int i;

	for (i = 0; kids->children[i]; i++) {
		if (kids->children[i]->pid == pid)
			return kids->children[i];
	}
	return NULL;
}

int
core_trace_loop(char **pathlist, pid_t pid)
{
	struct traced_children kids;
	int status;
	struct traced_child *kid;

	kids.children = calloc(16, sizeof(struct traced_child *));
	kids.max_children = 16;
	kids.nr_children = 0;
	add_child(&kids, pid);
	kids.children[0]->need_setup = 0;

	while (kids.nr_children) {
		pid = wait(&status);
		if (pid == (pid_t) -1) return -1;
		kid = find_child(&kids, pid);
		if (!kid) {
			puts("borkbork");
			continue;
		}
		if (WIFEXITED(status)) {
			kids.nr_children -= 1;
		}

		if (WIFSTOPPED(status)) {
			if (WSTOPSIG(status) == SIGSTOP && kid->need_setup) {
				setup_kid(pid);
				kid->need_setup = 0;
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
			} else if (WSTOPSIG(status) == SIGTRAP) {
				unsigned int event;
				event = (status >> 16) & 0xffff;
				if (event == PTRACE_EVENT_FORK
					|| event == PTRACE_EVENT_CLONE
					|| event == PTRACE_EVENT_VFORK) {
					pid_t kpid;
					int e;
					e = ptrace(PTRACE_GETEVENTMSG, pid, 0, &kpid);
					if (e != 0) printf("geteventmsg %s\n", strerror(e));
					add_child(&kids, kpid);
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WSTOPSIG(status));
				} else {
					if (kid->in_syscall) {
						handle_syscall_return(pid, kid);
						kid->in_syscall = 0;
					} else {
						handle_syscall(pathlist, pid, kid);
						kid->in_syscall = 1;
					}
				}
			} else {
				ptrace(PTRACE_CONT, pid, 0, (void*) WSTOPSIG(status));
			}
		}
	}
	return 0;
}
