/*
** Copyright (c) 2006, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include <Python.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

struct traced_child {
	pid_t pid;
	int proc_mem_fd;
	int need_setup;
	int in_syscall;
	unsigned long orig_eax;
};

struct trace_context {
	PyObject *func;
	char **pathlist;
	PyObject *log_func;
	unsigned int nr_children;
	struct traced_child children[512];
};

int path_writable(char **pathlist, pid_t pid, char *path);
void free_pathlist(char **pathlist);
char **make_pathlist(PyObject *paths);

int before_syscall(struct trace_context *ctx, pid_t pid, int syscall);

PyObject *core_trace_loop(struct trace_context *ctx, pid_t pid);
