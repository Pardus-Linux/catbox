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
#include <fcntl.h>
#include <linux/unistd.h>

static char doc_catbox[] = "Simple and fast sandboxing module.";
static char doc_run[] = "Run given function in a sandbox.\n"
"\n"
"    function: Python callable, will run inside the sandbox as a child\n"
"              process. It can read all Python variables, but can't modify\n"
"              caller's values.\n"
"    writable_paths: A list of allowed paths.\n"
"    network: Give a false value for disabling network communication.\n"
"    logger: Another callable, will be called with operation, path, resolved path\n"
"            arguments for each sandbox violation.\n"
"\n"
"    Everything except function are optional. Return value is an object\n"
"    with two attributes:\n"
"\n"
"    code: Exit code of the child process.\n"
"    violations: List of violation records.";
static char doc_canonical[] = "Resolve and simplify given path.\n"
"\n"
"    path: Path string.\n"
"    follow: Boolean, should we follow latest part of the path if it is\n"
"            a symlink, default is no.\n"
"    pid: Resolve path in the context of the process with given pid.\n"
"         Relative paths and /proc/self are resolved for that process.\n"
"\n"
"    Everything except path are optional. Return value is the resolved\n"
"    path string.";

static PyObject *
catbox_run(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {
		"function",
		"writable_paths",
		"network",
		"logger",
		"args",
		NULL
	};
	PyObject *ret;
	PyObject *paths = NULL;
	PyObject *net = NULL;
	struct trace_context ctx;
	struct traced_child *child, *temp;
	int i;

	memset(&ctx, 0, sizeof(struct trace_context));

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOOO",
		kwlist, &ctx.func, &paths, &net, &ctx.logger, &ctx.func_args))
			return NULL;

	if (PyCallable_Check(ctx.func) == 0) {
		PyErr_SetString(PyExc_TypeError, "First argument should be a callable function");
		return NULL;
	}

	if (ctx.logger && PyCallable_Check(ctx.logger) == 0) {
		PyErr_SetString(PyExc_TypeError, "Logger should be a callable function");
		return NULL;
	}

	if (paths) {
		ctx.pathlist = make_pathlist(paths);
		if (!ctx.pathlist) return NULL;
	}

	if (net == NULL || PyObject_IsTrue(net))
		ctx.network_allowed = 1;
	else
		ctx.network_allowed = 0;

	catbox_retval_init(&ctx);

	// setup is complete, run sandbox
	ret = catbox_core_run(&ctx);

	if (ctx.pathlist) {
		free_pathlist(ctx.pathlist);
	}

	for (i = 0; i < PID_TABLE_SIZE; i++) {
		temp = NULL;
		for (child = ctx.children[i]; child; child = temp) {
			temp = child->next;
			free(child);
		}
	}

	return ret;
}

static PyObject *
catbox_canonical(PyObject *self, PyObject *args, PyObject *kwargs)
{
	static char *kwlist[] = {
		"path",
		"follow",
		"pid",
		NULL
	};
	char *path;
	char *canonical;
	PyObject *follow = NULL;
	int dont_follow = 0;
	pid_t pid = -1;
	PyObject *ret;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|Oi", kwlist, &path, &follow, &pid))
		return NULL;

	if (follow && !PyObject_IsTrue(follow))
		dont_follow = 1;

	if (pid == -1)
		pid = getpid();

	canonical = catbox_paths_canonical(pid, path, dont_follow);
	if (!canonical) {
		return NULL;
	}

	ret = PyString_FromString(canonical);
	return ret;
}

static PyMethodDef methods[] = {
	{ "run", (PyCFunction) catbox_run, METH_VARARGS | METH_KEYWORDS, doc_run },
	{ "canonical", (PyCFunction) catbox_canonical, METH_VARARGS | METH_KEYWORDS, doc_canonical },
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initcatbox(void)
{
	PyObject *m;

	m = Py_InitModule3("catbox", methods, doc_catbox);
}
