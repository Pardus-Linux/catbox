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
do_run(struct trace_context *ctx)
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
		PyObject *ret;
		ptrace(PTRACE_TRACEME, 0, 0, 0);
		kill(getppid(), SIGUSR1); //send a signal to parent
		while (!got_sig) ; //wait for confirmation

		ret = PyObject_Call(ctx->func, PyTuple_New(0), NULL); //run the callable
        
		if (!ret) { //handle exception
			PyObject *e;
			PyObject *val;
			PyObject *tb;
			PyErr_Fetch(&e, &val, &tb);
			if (PyErr_GivenExceptionMatches(e, PyExc_SystemExit)) {
				// extract exit code
				if (PyInt_Check(val))
					exit(PyInt_AsLong(val));
				else
					exit(2);
			}
			// error
			exit(1);
		}
		exit(0);
	}

	// parent process
	while (!got_sig) ; //wait for the signal from child
	kill(pid, SIGUSR1); //send a confirmation
	waitpid(pid, NULL, 0); // ??

	setup_kid(pid);
	ptrace(PTRACE_SYSCALL, pid, 0, (void *) SIGUSR1);

	return core_trace_loop(ctx, pid);
}

static PyObject *
catbox_run(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {
		"function",
        "ret_object",
		"writable_paths",
		"logger",
		NULL
	};
 	PyObject *ret;
	PyObject *paths = NULL;
	struct trace_context ctx;

	memset(&ctx, 0, sizeof(struct trace_context));
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OO|OO", kwlist, &ctx.func, &ctx.ret_object, &paths, &ctx.log_func))
		return NULL;

	if (PyCallable_Check(ctx.func) == 0) {
		PyErr_SetString(PyExc_TypeError, "First argument should be a callable function");
		return NULL;
	}

	if (ctx.log_func != Py_None && !PyCallable_Check(ctx.log_func)) {
		PyErr_SetString(PyExc_TypeError, "Logger should be a callable function");
		return NULL;
	}

	if (paths) {
		ctx.pathlist = make_pathlist(paths);
		if (!ctx.pathlist) return NULL;
	}

	ret = do_run(&ctx);
	if (ctx.pathlist) {
		free_pathlist(ctx.pathlist);
	}

 	return ret;
}


static PyMethodDef methods[] = {
    { "run", (PyCFunction) catbox_run, METH_VARARGS | METH_KEYWORDS,
      "Run given function in a sandbox."},
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initcatbox_int(void)
{
	PyObject *m;

	m = Py_InitModule("catbox_int", methods);
}
