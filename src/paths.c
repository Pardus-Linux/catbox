/*
** Copyright (c) 2006, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include <Python.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static char *
get_cwd(pid_t pid)
{
	// FIXME: lame function
	char buf[256];
	static char buf2[1024];
	int i;

	sprintf(buf, "/proc/%d/cwd", pid);
	i = readlink(buf, buf2, 1020);
	if (i == -1) {
		return NULL;
	}
	buf2[i] = '\0';
	return buf2;
}

int
path_writable(char **pathlist, pid_t pid, char *path)
{
	// FIXME: spaghetti code ahead
	char *canonical = NULL;
	char *pwd = NULL;
	int ret = 0;
	int i;
	int flag = 0;

	if (!pathlist) return 0;

	if (path[0] != '/') {
		char *tmp;
		pwd = get_cwd(pid);
		if (!pwd) return 0;
		tmp = malloc(strlen(path) + 1 + strlen(pwd));
		if (!tmp) return 0;
		sprintf(tmp, "%s/%s", pwd, path);
		path = tmp;
	}

	canonical = realpath(path, NULL);
	if (!canonical) {
		if (errno == ENOENT) {
			char *t;
			t = strrchr(path, '/');
			if (t && t[1] != '\0') {
				++t;
				*t = '\0';
				flag = 1;
				canonical = realpath(path, NULL);
				if (!canonical) {
					goto out;
				}
				goto turka;
			}
		}
		goto out;
	}
turka:
	for (i = 0; pathlist[i]; i++) {
		size_t size = strlen(pathlist[i]);
		if (flag == 1 && pathlist[i][size-1] == '/') --size;
		if (strncmp(pathlist[i], canonical, size) == 0) {
			ret = 1;
			goto out;
		}
	}

out:
	if (canonical) free(canonical);
	if (pwd) free(path);

	return ret;
}

void
free_pathlist(char **pathlist)
{
	int i;

	for (i = 0; pathlist[i]; i++)
		free(pathlist[i]);
	free(pathlist);
}

char **
make_pathlist(PyObject *paths)
{
	PyObject *item;
	char **pathlist;
	char *str;
	unsigned int count;
	unsigned int i;

	if (PyList_Check(paths) == 0 && PyTuple_Check(paths) == 0) {
		PyErr_SetString(PyExc_TypeError, "writable_paths should be a list or tuple object");
		return NULL;
	}

	count = PySequence_Size(paths);
	pathlist = calloc(count + 1, sizeof(char *));
	if (!pathlist) return NULL;

	for (i = 0; i < count; i++) {
		item = PySequence_GetItem(paths, i);
		if (!item) {
			free_pathlist(pathlist);
			return NULL;
		}
		str = PyString_AsString(item);
		if (!str) {
			Py_DECREF(item);
			free_pathlist(pathlist);
			return NULL;
		}
		if (str[0] != '/') {
			Py_DECREF(item);
			free_pathlist(pathlist);
			PyErr_SetString(PyExc_TypeError, "paths should be absolute");
			return NULL;
		}
		pathlist[i] = strdup(str);
		Py_DECREF(item);
		if (!pathlist[i]) {
			free_pathlist(pathlist);
			return NULL;
		}
	}

	return pathlist;
}
