/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
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
#include <linux/unistd.h>
#include <fcntl.h>
#include <errno.h>

static void
setup_kid(struct traced_child *kid)
{
	int e;

	// We want to trace all sub children, and want special notify
	// to distinguish between normal sigtrap and syscall sigtrap.
	e = ptrace(PTRACE_SETOPTIONS,
		kid->pid,
		0,
		PTRACE_O_TRACESYSGOOD
		| PTRACE_O_TRACECLONE
		| PTRACE_O_TRACEFORK
		| PTRACE_O_TRACEVFORK
	);
	if (e != 0) printf("ptrace opts error %s\n",strerror(errno));
	kid->need_setup = 0;
}

static int
pid_hash(pid_t pid)
{
	return ((unsigned long) pid) % PID_TABLE_SIZE;
}

static struct traced_child *
find_child(struct trace_context *ctx, pid_t pid)
{
	int hash;
	struct traced_child *kid;

	hash = pid_hash(pid);
	for (kid = ctx->children[hash]; kid; kid = kid->next) {
		if (kid->pid == pid) return kid;
	}
	return NULL;
}

static struct traced_child *
add_child(struct trace_context *ctx, pid_t pid)
{
	struct traced_child *kid;
	int hash;

	kid = find_child(ctx, pid);
	if (kid) {
		printf("BORKBORK: Trying to add existing child\n");
	}

	kid = malloc(sizeof(struct traced_child));
	memset(kid, 0, sizeof(struct traced_child));
	kid->pid = pid;
	kid->need_setup = 1;

	hash = pid_hash(pid);

	if (!ctx->children[hash]) {
		ctx->children[hash] = kid;
	} else {
		kid->next = ctx->children[hash];
		ctx->children[hash] = kid;
	}

	ctx->nr_children++;

	return kid;
}

static void
rem_child(struct trace_context *ctx, pid_t pid)
{
	struct traced_child *kid, *temp;
	int hash;

	hash = pid_hash(pid);
	kid = ctx->children[hash];
	if (kid) {
		if (kid->pid == pid) {
			ctx->children[hash] = kid->next;
			free(kid);
			ctx->nr_children--;
			return;
		} else {
			temp = kid;
			for (kid = kid->next; kid; kid = kid->next) {
				if (kid->pid == pid) {
					temp->next = kid->next;
					free(kid);
					ctx->nr_children--;
					return;
				}
				temp = kid;
			}
		}
	}
	puts("BORKBORK: trying to remove non-tracked child");
}

enum {
	E_SETUP = 0,
	E_SETUP_PREMATURE,
	E_SYSCALL,
	E_FORK,
	E_EXECV,
	E_GENUINE,
	E_EXIT,
	E_EXIT_SIGNAL,
	E_UNKNOWN
};

static int
decide_event(struct trace_context *ctx, struct traced_child *kid, int status)
{
	unsigned int event;
	int sig;

	// We got a signal from child, and want to know the cause of it
	if (WIFSTOPPED(status)) {
		// 1. reason: Execution of child stopped by a signal
		sig = WSTOPSIG(status);
		if (sig == SIGSTOP && kid && kid->need_setup) {
			// 1.1. reason: Child is born and ready for tracing
			return E_SETUP;
		}
		if (sig == SIGSTOP && !kid) {
			// 1.2. reason: Child is born before fork event, and ready for tracing
			return E_SETUP_PREMATURE;
		}
		if (sig & SIGTRAP) {
			// 1.3. reason: We got a signal from ptrace
			if (sig == (SIGTRAP | 0x80)) {
				// 1.3.1. reason: Child made a system call
				return E_SYSCALL;
			}
			event = (status >> 16) & 0xffff;
			if (event == PTRACE_EVENT_FORK
			    || event == PTRACE_EVENT_VFORK
			    || event == PTRACE_EVENT_CLONE) {
				// 1.3.2. reason: Child made a fork
				return E_FORK;
			}
			if (kid && kid->in_execve) {
				// 1.3.3. reason: Spurious sigtrap after execve call
				return E_EXECV;
			}
		}
		// 1.4. reason: Genuine signal directed to the child itself
		return E_GENUINE;
	} else if (WIFEXITED(status)) {
		// 2. reason: Child is exited normally
		return E_EXIT;
	} else if (WIFSIGNALED(status)) {
		// 3. reason: Child is terminated by a signal
		return E_EXIT_SIGNAL;
	}
	return E_UNKNOWN;
}

static PyObject *
core_trace_loop(struct trace_context *ctx)
{
	int status;
	int event;
	pid_t pid;
	pid_t kpid;
	int e;
	long retcode = 0;
	struct traced_child *kid;

	while (ctx->nr_children) {
		pid = waitpid(-1, &status, __WALL);
		if (pid == (pid_t) -1) return NULL;
		kid = find_child(ctx, pid);
		event = decide_event(ctx, kid, status);
		if (!kid && event != E_SETUP_PREMATURE) {
			// This shouldn't happen
			printf("BORKBORK: nr %d, pid %d, status %x, event %d\n", ctx->nr_children, pid, status, event);
		}

		switch (event) {
			case E_SETUP:
				setup_kid(kid);
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_SETUP_PREMATURE:
				kid = add_child(ctx, pid);
				setup_kid(kid);
				break;
			case E_SYSCALL:
				if (kid) catbox_syscall_handle(ctx, kid);
				break;
			case E_FORK:
				e = ptrace(PTRACE_GETEVENTMSG, pid, 0, &kpid); //get the new kid's pid
				if (e != 0) printf("geteventmsg %s\n", strerror(e));
				if (find_child(ctx, kpid)) {
					// Kid is prematurely born, let it continue its life
					ptrace(PTRACE_SYSCALL, kpid, 0, 0);
				} else {
					// Add the new kid (setup will be done later)
					add_child(ctx, kpid);
				}
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_EXECV:
				kid->in_execve = 0;
				ptrace(PTRACE_SYSCALL, pid, 0, 0);
				break;
			case E_GENUINE:
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WSTOPSIG(status));
				break;
			case E_EXIT:
				if (kid == ctx->first_child) {
					// If it is our first child, keep its return value
					retcode = WEXITSTATUS(status);
				}
				rem_child(ctx, pid);
				break;
			case E_EXIT_SIGNAL:
				ptrace(PTRACE_SYSCALL, pid, 0, (void*) WTERMSIG(status));
				retcode = 1;
				rem_child(ctx, pid);
				break;
			case E_UNKNOWN:
				printf("BORKBORK: Unknown signal %x pid %d\n", status, pid);
				break;
		}
	}

	catbox_retval_set_exit_code(ctx, retcode);
	return ctx->retval;
}

// Syncronization value, it has two copies in parent and child's memory spaces
static int volatile got_sig = 0;

static void sigusr1(int dummy) {
	got_sig = 1;
}

PyObject *
catbox_core_run(struct trace_context *ctx)
{
	struct traced_child *kid;
	void (*oldsig)(int);
	pid_t pid;

	got_sig = 0;
	oldsig = signal(SIGUSR1, sigusr1);

	pid = fork();
	if (pid == (pid_t) -1) {
		PyErr_SetString(PyExc_RuntimeError, "fork failed");
		return NULL;
	}

	if (pid == 0) {
		// child process comes to life
		PyObject *ret;
		PyObject *args;

		// set up tracing mode
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		// tell the parent we are ready
		kill(getppid(), SIGUSR1);
		// wait until parent tells us to continue
		while (!got_sig);

		// let the callable do its job
		args = ctx->func_args;
		if (!args) args = PyTuple_New(0);
		ret = PyObject_Call(ctx->func, args, NULL);

		if (!ret) {
			PyObject *e;
			PyObject *val;
			PyObject *tb;
			PyErr_Fetch(&e, &val, &tb);
			if (PyErr_GivenExceptionMatches(e, PyExc_SystemExit)) {
				if (PyInt_Check(val))
					// Callable exits by sys.exit(n)
					exit(PyInt_AsLong(val));
				else
					// Callable exits by sys.exit()
					exit(2);
			}
			// Callable exits by unhandled exception
			// So let child print error and value to stderr
			PyErr_Display( e,val,tb );
			/*
			 * FIXME: In a perfect world following works better
			 * but pisi.api.cleanup didn't like what i want - caglar
			PySys_SetObject("last_type", e);
			PySys_SetObject("last_value", val);
			PySys_SetObject("last_traceback", tb);
			PyObject *hook = PySys_GetObject("excepthook");
			if (hook) {
				PyObject *args = PyTuple_Pack(3, e, val, tb);
				PyObject *result = PyEval_CallObject(hook, args);
				
				// excepthook is a borrowed reference 
				Py_XDECREF(result);
				Py_XDECREF(args);
				Py_XDECREF(tb);
				Py_XDECREF(val);
				Py_XDECREF(e);
				PyErr_Clear();
			}
			*/
			exit(1);
		}
		// Callable exits by returning from function normally
		exit(0);
	}

	// parent process continues

	// wait until child set ups tracing mode, and sends a signal
	while (!got_sig);
	// tell the kid that it can start given callable now
	kill(pid, SIGUSR1);
	waitpid(pid, NULL, 0);

	kid = add_child(ctx, pid);
	setup_kid(kid);
	ctx->first_child = kid;
	ptrace(PTRACE_SYSCALL, pid, 0, (void *) SIGUSR1);

	return core_trace_loop(ctx);
}
