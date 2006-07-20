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
#include <fcntl.h>
#include <sys/ptrace.h>
#include <linux/user.h>
#include <linux/unistd.h>

int got_sig = 0;

static void sigusr1(int dummy) {
	got_sig = 1;
}

static PyObject *
do_run(PyObject *func, char **pathlist)
{
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
		// child process
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		kill(getppid(), SIGUSR1);
		while (!got_sig) ;

		PyObject_Call(func, PyTuple_New(0), NULL);
		exit(0);
	}

	// parent process
	while (!got_sig) ;
	kill(pid, SIGUSR1);
	waitpid(pid, NULL, 0);

	setup_kid(pid);
	ptrace(PTRACE_SYSCALL, pid, 0, (void *) SIGUSR1);

	core_trace_loop(pathlist, pid);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
catbox_run(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {
		"function",
		"writable_paths",
		NULL
	};
	PyObject *ret;
	PyObject *func;
	PyObject *paths = NULL;
	char **pathlist = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &func, &paths))
		return NULL;

	if (PyCallable_Check(func) == 0) {
		PyErr_SetString(PyExc_TypeError, "First argument should be a callable function");
		return NULL;
	}

	if (paths) {
		pathlist = make_pathlist(paths);
		if (!pathlist) return NULL;
	}

	ret = do_run(func, pathlist);

	if (pathlist) {
		free_pathlist(pathlist);
	}

	return ret;
}

static PyMethodDef methods[] = {
	{ "run", (PyCFunction) catbox_run, METH_VARARGS | METH_KEYWORDS,
	  "Run given function in a sandbox."},
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initcatbox(void)
{
	PyObject *m;

	m = Py_InitModule("catbox", methods);
}
