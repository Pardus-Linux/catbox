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

static void
handle_syscall(struct trace_context *ctx, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct u_in;

	ptrace(PTRACE_GETREGS, kid->pid, 0, &u_in);
	syscall = u_in.orig_eax;

	if (before_syscall(ctx, kid->pid, syscall) != 0) {
		kid->orig_eax = u_in.orig_eax;
		ptrace(PTRACE_POKEUSER, kid->pid, 44, 0xbadca11);
	}
	ptrace(PTRACE_SYSCALL, kid->pid, 0, 0);
}

static void
handle_syscall_return(pid_t pid, struct traced_child *kid)
{
	int syscall;
	struct user_regs_struct u_in;

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

static void
add_child(struct trace_context *ctx, pid_t pid)
{
	struct traced_child *kid;

	kid = &ctx->children[ctx->nr_children++];
	memset(kid, 0, sizeof(struct traced_child));
	kid->pid = pid;
	kid->need_setup = 1;
}

static struct traced_child *
find_child(struct trace_context *ctx, pid_t pid)
{
	int i;

	for (i = 0; i < ctx->nr_children; i++) {
		if (ctx->children[i].pid == pid)
			return &ctx->children[i];
	}
	return NULL;
}

static void
rem_child(struct trace_context *ctx, pid_t pid)
{
	int i;

	for (i = 0; i < ctx->nr_children; i++) {
		if (ctx->children[i].pid == pid)
			goto do_rem;
	}
	puts("bjorkbjork");
do_rem:
	ctx->nr_children -= 1;
	ctx->children[i] = ctx->children[ctx->nr_children];
}

PyObject *
core_trace_loop(struct trace_context *ctx, pid_t pid)
{
	int status;
	long retcode = 0;
	struct traced_child *kid;

	ctx->nr_children = 0;
	add_child(ctx, pid);
	ctx->children[0].need_setup = 0;

	while (ctx->nr_children) {
		pid = wait(&status);
		if (pid == (pid_t) -1) return NULL;
		kid = find_child(ctx, pid);
		if (!kid) {
			puts("borkbork");
			continue;
		}
		if (WIFEXITED(status)) {
			if (kid == &ctx->children[0]) {
				// keep ret value
				retcode = WEXITSTATUS(status);
			}
			rem_child(ctx, pid);
		}
		// FIXME: handle WIFSIGNALLED here

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
					add_child(ctx, kpid);
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WSTOPSIG(status));
				} else {
					if (kid->in_syscall) {
						handle_syscall_return(pid, kid);
						kid->in_syscall = 0;
					} else {
						handle_syscall(ctx, kid);
						kid->in_syscall = 1;
					}
				}
			} else {
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WSTOPSIG(status));
			}
		}
	}

	return PyInt_FromLong(retcode);
}
