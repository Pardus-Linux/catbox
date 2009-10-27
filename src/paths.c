/*
** Copyright (c) 2006-2007, TUBITAK/UEKAE
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

char *
catbox_paths_canonical(pid_t pid, char *path, int dont_follow)
{
	char *canonical = NULL;
	char *old_path = NULL;
	char *tmp;
	size_t len;

	// prepend current dir to the relative paths
	if (path[0] != '/') {
		char *cwd;

		cwd = get_cwd(pid);
		if (!cwd) return NULL;
		tmp = malloc(strlen(path) + 2 + strlen(cwd));
		if (!tmp) return NULL;
		sprintf(tmp, "%s/%s", cwd, path);
		old_path = path;
		path = tmp;
	}

	// Special case for very special /proc/self symlink
	// This link resolves to the /proc/1234 (pid number)
	// since we are parent process, we get a diffent view
	// of filesystem if we let realpath to resolve this.
	if (strncmp(path, "/proc/self", 10) == 0) {
		tmp = malloc(strlen(path) + 24);
		if (!tmp) {
			if (old_path) free(path);
			return NULL;
		}
		sprintf(tmp, "/proc/%d/%s", pid, path + 10);
		if (old_path)
			free(path);
		else
			old_path = path;
		path = tmp;
	}

	// strip last character if it is a dir separator
	len = strlen(path);
	if (path[len-1] == '/') {
		if (!old_path) {
			old_path = path;
			path = strdup(path);
			if (!path) return NULL;
		}
		path[len-1] = '\0';
	}

	// resolve symlinks in the path
	if (!dont_follow) {
		canonical = realpath(path, NULL);
		if (!canonical && errno == ENAMETOOLONG) {
			if (old_path) free(path);
			return NULL;
		}
	}
	if (!canonical) {
		if (dont_follow || errno == ENOENT) {
			if (!old_path) {
				old_path = path;
				path = strdup(path);
				if (!path) return NULL;
			}
			char *t;
			t = strrchr(path, '/');
			if (t && t[1] != '\0') {
				*t = '\0';
				canonical = realpath(path, NULL);
				if (canonical) {
					char *tmp;
					++t;
					tmp = malloc(strlen(canonical) + 2 + strlen(t));
					if (tmp) {
						sprintf(tmp, "%s/%s", canonical, t);
						free(canonical);
						canonical = tmp;
					}
				}
			}
		}
	}

	if (old_path) free(path);

	return canonical;
}

int
path_writable(char **pathlist, const char *canonical, int mkdir_case)
{
	int i;

	if (!pathlist) return 0;

	for (i = 0; pathlist[i]; i++) {
		size_t size = strlen(pathlist[i]);
		if (pathlist[i][size-1] == '/' && strlen(canonical) == (size - 1)) --size;
		if (strncmp(pathlist[i], canonical, size) == 0) {
			return 1;
		} else if (mkdir_case && strncmp(pathlist[i], canonical, strlen(canonical)) == 0) {
			return -1;
		}
	}
	return 0;
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
