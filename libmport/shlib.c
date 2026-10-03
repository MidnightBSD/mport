/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Shared library analysis at package creation.
 *
 * Every regular file staged for the package is read as ELF.  An object with
 * DT_SONAME provides that library; every DT_NEEDED is a library the package
 * requires.  The rules follow FreeBSD pkg(8) so the same port knobs behave
 * the same way:
 *
 *   SHLIB_PROVIDE_PATHS_NATIVE, _COMPAT_32, _COMPAT_LINUX, _COMPAT_LINUX_32
 *       Comma separated directories.  A library counts as provided only
 *       when the directory it is installed into is one of them (chosen by
 *       the object's OS and word size).  An unset or empty list disables
 *       the filter.  Libraries elsewhere are private to the package and
 *       still cancel the package's own requirements.
 *   SHLIB_PROVIDE_IGNORE_GLOB, SHLIB_PROVIDE_IGNORE_REGEX
 *   SHLIB_REQUIRE_IGNORE_GLOB, SHLIB_REQUIRE_IGNORE_REGEX
 *       Comma separated patterns dropped from the respective list.
 *
 * Names carry the same suffixes pkg uses for non-native objects: ":32",
 * ":Linux" and ":Linux:32".  The ports framework exports these variables
 * in PKG_ENV when it runs mport.create(1).
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <gelf.h>
#include <libelf.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mport.h"
#include "mport_private.h"

struct mport_shlib_scan {
	stringlist_t paths[MPORT_SHLIB_NFLAGS];
	stringlist_t provide_ignore_glob;
	stringlist_t provide_ignore_regex;
	stringlist_t require_ignore_glob;
	stringlist_t require_ignore_regex;

	stringlist_t provided; /* regular files in a provide path */
	stringlist_t maybe_provided; /* symlinks in a provide path */
	stringlist_t internal; /* sonames found outside the provide paths */
	stringlist_t required; /* every DT_NEEDED seen */
	stringlist_t basenames; /* of every regular file, for the self-check */
};

static void
split_env_list(const char *name, stringlist_t *out)
{
	const char *value = getenv(name);
	char *copy, *tok, *save;

	if (value == NULL || value[0] == '\0')
		return;
	if ((copy = strdup(value)) == NULL)
		return;
	for (tok = strtok_r(copy, ",", &save); tok != NULL; tok = strtok_r(NULL, ",", &save)) {
		while (*tok == ' ' || *tok == '\t')
			tok++;
		size_t len = strlen(tok);
		while (len > 0 && (tok[len - 1] == ' ' || tok[len - 1] == '\t'))
			tok[--len] = '\0';
		if (len == 0)
			continue;
		char *item = strdup(tok);
		if (item != NULL)
			tll_push_back(*out, item);
	}
	free(copy);
}

static bool
list_contains(const stringlist_t *list, const char *s)
{
	tll_foreach(*list, it) {
		if (strcmp(it->item, s) == 0)
			return true;
	}
	return false;
}

static void
list_add_unique(stringlist_t *list, char *s)
{
	if (s == NULL)
		return;
	if (list_contains(list, s)) {
		free(s);
		return;
	}
	tll_push_back(*list, s);
}

static int
cmp_str(const char *a, const char *b)
{
	return strcmp(a, b);
}

/* pattern lists: fnmatch(3) globs and POSIX extended regexps */
static bool
matches_ignore(const char *name, const stringlist_t *globs, const stringlist_t *regexps)
{
	tll_foreach(*globs, it) {
		if (fnmatch(it->item, name, 0) == 0)
			return true;
	}
	tll_foreach(*regexps, it) {
		regex_t re;
		int rc;

		if (regcomp(&re, it->item, REG_EXTENDED | REG_NOSUB) != 0)
			continue;
		rc = regexec(&re, name, 0, NULL, 0);
		regfree(&re);
		if (rc == 0)
			return true;
	}
	return false;
}

/* strip a trailing slash so "/usr/local/lib/" and "/usr/local/lib" agree */
static bool
dir_paths_equal(const char *a, const char *b)
{
	size_t la = strlen(a), lb = strlen(b);

	while (la > 1 && a[la - 1] == '/')
		la--;
	while (lb > 1 && b[lb - 1] == '/')
		lb--;
	return la == lb && strncmp(a, b, la) == 0;
}

/* whether the directory holding installed_path is one of the listed paths */
static bool
in_provide_paths(const stringlist_t *paths, const char *installed_path)
{
	char dir[PATH_MAX];
	const char *slash;
	size_t len;

	if (tll_length(*paths) == 0)
		return true; /* unset: no filtering, as pkg does */

	slash = strrchr(installed_path, '/');
	if (slash == NULL)
		return false;
	len = (slash == installed_path) ? 1 : (size_t)(slash - installed_path);
	if (len >= sizeof(dir))
		return false;
	memcpy(dir, installed_path, len);
	dir[len] = '\0';

	tll_foreach(*paths, it) {
		if (dir_paths_equal(dir, it->item))
			return true;
	}
	return false;
}

char *
mport_shlib_name_with_flags(const char *name, int flags)
{
	const char *os = (flags & MPORT_SHLIB_LINUX) ? ":Linux" : "";
	const char *arch = (flags & MPORT_SHLIB_COMPAT_32) ? ":32" : "";
	char *out;

	if (asprintf(&out, "%s%s%s", name, os, arch) == -1)
		return NULL;
	return out;
}

/*
 * Read one file.  On return *provided holds the soname (caller frees) or
 * NULL, *flags the object's class, and required gains every DT_NEEDED.
 * Non-ELF files, static objects and objects for an architecture this host
 * cannot run are silently skipped.  Only libelf failures return an error.
 */
int
mport_shlib_analyse_elf(const char *path, char **provided, int *flags, stringlist_t *required)
{
	int fd;
	Elf *elf;
	GElf_Ehdr ehdr;
	Elf_Scn *scn = NULL;
	Elf_Scn *dynamic = NULL;
	size_t sh_link = 0;
	size_t numdyn = 0;
	Elf_Data *data;
	struct stat sb;
	int ret = MPORT_OK;

	*provided = NULL;
	*flags = MPORT_SHLIB_NATIVE;

	/* follow a symlink; what matters is the object it points at */
	if (stat(path, &sb) != 0 || !S_ISREG(sb.st_mode) || sb.st_size == 0)
		return MPORT_OK;

	if (elf_version(EV_CURRENT) == EV_NONE)
		RETURN_ERRORX(MPORT_ERR_FATAL, "ELF library initialization failed: %s",
		    elf_errmsg(-1));

	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
		return MPORT_OK;

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		close(fd);
		return MPORT_OK;
	}

	if (elf_kind(elf) != ELF_K_ELF || gelf_getehdr(elf, &ehdr) == NULL ||
	    (ehdr.e_type != ET_DYN && ehdr.e_type != ET_EXEC))
		goto out;

	/*
	 * Class and OS.  The toolchain tags every MidnightBSD object with
	 * ELFOSABI_FREEBSD.  Anything else is treated as a Linux object, the
	 * same fallback the kernel's image activator applies.  A 32-bit object
	 * on a 64-bit host is a compat32 object; a 64-bit object on a 32-bit
	 * host cannot run here and is skipped.
	 */
	if (ehdr.e_ident[EI_OSABI] != ELFOSABI_FREEBSD)
		*flags |= MPORT_SHLIB_LINUX;
#if defined(__LP64__)
	if (ehdr.e_ident[EI_CLASS] == ELFCLASS32)
		*flags |= MPORT_SHLIB_COMPAT_32;
#else
	if (ehdr.e_ident[EI_CLASS] == ELFCLASS64)
		goto out;
#endif

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;

		if (gelf_getshdr(scn, &shdr) != &shdr)
			continue;
		if (shdr.sh_type == SHT_DYNAMIC) {
			if (shdr.sh_entsize == 0)
				goto out;
			dynamic = scn;
			sh_link = shdr.sh_link;
			numdyn = shdr.sh_size / shdr.sh_entsize;
			break;
		}
	}
	if (dynamic == NULL || (data = elf_getdata(dynamic, NULL)) == NULL)
		goto out; /* statically linked */

	for (size_t i = 0; i < numdyn; i++) {
		GElf_Dyn dyn;
		const char *name;

		if (gelf_getdyn(data, (int)i, &dyn) != &dyn)
			break;
		if (dyn.d_tag == DT_NULL)
			break;
		if (dyn.d_tag != DT_SONAME && dyn.d_tag != DT_NEEDED)
			continue;

		name = elf_strptr(elf, sh_link, dyn.d_un.d_val);
		if (name == NULL || strncmp(name, "lib", 3) != 0)
			continue; /* pkg ignores anything not named lib*; so do we */

		if (dyn.d_tag == DT_SONAME) {
			if (*provided != NULL) {
				free(*provided);
				*provided = NULL;
				ret = SET_ERRORX(MPORT_ERR_FATAL,
				    "malformed ELF file %s has multiple DT_SONAME entries", path);
				goto out;
			}
			*provided = strdup(name);
		} else if (name[0] != '/') {
			/* a few builds record a full path; pkg skips those too */
			list_add_unique(required, mport_shlib_name_with_flags(name, *flags));
		}
	}

out:
	elf_end(elf);
	close(fd);
	return ret;
}

mportShlibScan *
mport_shlib_scan_new(void)
{
	mportShlibScan *scan = calloc(1, sizeof(*scan));

	if (scan == NULL)
		return NULL;
	for (int i = 0; i < MPORT_SHLIB_NFLAGS; i++) {
		stringlist_t l = tll_init();
		scan->paths[i] = l;
	}
	stringlist_t a = tll_init(), b = tll_init(), c = tll_init(), d = tll_init();
	scan->provide_ignore_glob = a;
	scan->provide_ignore_regex = b;
	scan->require_ignore_glob = c;
	scan->require_ignore_regex = d;
	stringlist_t e = tll_init(), f = tll_init(), g = tll_init(), h = tll_init(),
		     k = tll_init();
	scan->provided = e;
	scan->maybe_provided = f;
	scan->internal = g;
	scan->required = h;
	scan->basenames = k;

	split_env_list("SHLIB_PROVIDE_PATHS_NATIVE", &scan->paths[MPORT_SHLIB_NATIVE]);
	split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_32", &scan->paths[MPORT_SHLIB_COMPAT_32]);
	split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_LINUX", &scan->paths[MPORT_SHLIB_LINUX]);
	split_env_list("SHLIB_PROVIDE_PATHS_COMPAT_LINUX_32",
	    &scan->paths[MPORT_SHLIB_LINUX | MPORT_SHLIB_COMPAT_32]);
	split_env_list("SHLIB_PROVIDE_IGNORE_GLOB", &scan->provide_ignore_glob);
	split_env_list("SHLIB_PROVIDE_IGNORE_REGEX", &scan->provide_ignore_regex);
	split_env_list("SHLIB_REQUIRE_IGNORE_GLOB", &scan->require_ignore_glob);
	split_env_list("SHLIB_REQUIRE_IGNORE_REGEX", &scan->require_ignore_regex);

	return scan;
}

void
mport_shlib_scan_free(mportShlibScan *scan)
{
	if (scan == NULL)
		return;
	for (int i = 0; i < MPORT_SHLIB_NFLAGS; i++)
		tll_free_and_free(scan->paths[i], free);
	tll_free_and_free(scan->provide_ignore_glob, free);
	tll_free_and_free(scan->provide_ignore_regex, free);
	tll_free_and_free(scan->require_ignore_glob, free);
	tll_free_and_free(scan->require_ignore_regex, free);
	tll_free_and_free(scan->provided, free);
	tll_free_and_free(scan->maybe_provided, free);
	tll_free_and_free(scan->internal, free);
	tll_free_and_free(scan->required, free);
	tll_free_and_free(scan->basenames, free);
	free(scan);
}

/*
 * Record one staged file.  staged_path is where the file is now,
 * installed_path where the package puts it (absolute, used for the
 * provide-path filter).
 */
int
mport_shlib_scan_file(mportShlibScan *scan, const char *staged_path, const char *installed_path)
{
	char *provided = NULL;
	int flags = MPORT_SHLIB_NATIVE;
	struct stat sb;
	const char *base;

	if (scan == NULL)
		return MPORT_OK;

	if (lstat(staged_path, &sb) != 0)
		return MPORT_OK;

	if (S_ISREG(sb.st_mode)) {
		base = strrchr(installed_path, '/');
		base = (base == NULL) ? installed_path : base + 1;
		list_add_unique(&scan->basenames, strdup(base));
	}

	if (mport_shlib_analyse_elf(staged_path, &provided, &flags, &scan->required) != MPORT_OK)
		return mport_err_code();

	if (provided == NULL)
		return MPORT_OK;

	char *named = mport_shlib_name_with_flags(provided, flags);
	free(provided);
	if (named == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");

	if (in_provide_paths(&scan->paths[flags & (MPORT_SHLIB_NFLAGS - 1)], installed_path)) {
		if (S_ISREG(sb.st_mode))
			list_add_unique(&scan->provided, named);
		else
			list_add_unique(&scan->maybe_provided, named);
	} else {
		list_add_unique(&scan->internal, named);
	}

	return MPORT_OK;
}

/*
 * Settle the lists and hand them to the package: provided sonames the
 * package exports, required sonames it needs from elsewhere.  Both are
 * sorted and unique.  no_provide_shlib is set when the package exports
 * nothing; a value the caller already set stands.
 */
int
mport_shlib_scan_finish(mportShlibScan *scan, mportPackageMeta *pack)
{
	if (scan == NULL || pack == NULL)
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid shared library scan");

	/*
	 * A symlink in a provide path whose target is a private copy of the
	 * same soname (libfoo.so.1 -> ../private/libfoo.so.1.2) exports that
	 * soname after all.
	 */
	tll_foreach(scan->maybe_provided, mp) {
		tll_foreach(scan->internal, in) {
			if (strcmp(mp->item, in->item) == 0) {
				list_add_unique(&scan->provided, strdup(mp->item));
				tll_remove_and_free(scan->internal, in, free);
				break;
			}
		}
	}

	/* requirements the package meets itself are not requirements */
	tll_foreach(scan->required, it) {
		const char *s = it->item;
		const char *colon = strchr(s, ':');
		size_t plain_len = (colon == NULL) ? strlen(s) : (size_t)(colon - s);
		bool self = list_contains(&scan->provided, s) ||
		    list_contains(&scan->internal, s) || list_contains(&scan->maybe_provided, s);

		if (!self) {
			tll_foreach(scan->basenames, bn) {
				if (strlen(bn->item) == plain_len &&
				    strncmp(bn->item, s, plain_len) == 0) {
					self = true;
					break;
				}
			}
		}
		if (self ||
		    matches_ignore(s, &scan->require_ignore_glob, &scan->require_ignore_regex)) {
			tll_remove_and_free(scan->required, it, free);
			continue;
		}
	}

	tll_foreach(scan->provided, it) {
		if (matches_ignore(it->item, &scan->provide_ignore_glob,
			&scan->provide_ignore_regex))
			tll_remove_and_free(scan->provided, it, free);
	}

	tll_free_and_free(pack->shlibs_provided, free);
	tll_free_and_free(pack->shlibs_required, free);
	tll_foreach(scan->provided, it)
		tll_push_back(pack->shlibs_provided, strdup(it->item));
	tll_foreach(scan->required, it)
		tll_push_back(pack->shlibs_required, strdup(it->item));
	tll_sort(pack->shlibs_provided, cmp_str);
	tll_sort(pack->shlibs_required, cmp_str);

	if (tll_length(pack->shlibs_provided) == 0)
		pack->no_provide_shlib = 1;

	return MPORT_OK;
}
