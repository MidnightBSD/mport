/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010-2018, 2021 Lucas Holt
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

#include <sys/cdefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <err.h>
#include <locale.h>
#include <getopt.h>
#include <mport.h>
#include <mport_private.h>
#include <libutil.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"

static void usage(void);

static void show_version(mportInstance *, int);

static int loadIndex(mportInstance *);

static mportIndexEntry **lookupIndex(mportInstance *, const char *);

static int add(mportInstance *mport, const char *filename, mportAutomatic automatic);
static int install(mportInstance *, const char *, mportAutomatic);

static int cpeList(mportInstance *);
static int cpeGet(mportInstance *mport, const char *packageName);
static int purlList(mportInstance *);
static int purlGet(mportInstance *mport, const char *packageName);

static int configGet(mportInstance *, const char *);

static int configSet(mportInstance *, const char *, const char *);

static int delete(mportInstance *, const char *);

static int deleteMany(mportInstance *mport, int argc, char *argv[], bool skipFirst);

static int deleteAll(mportInstance *);

static int info(mportInstance *, const char *);

static int search(mportInstance *, char **);

static int stats(mportInstance *mport);

static int clean(mportInstance *);

static int verify(mportInstance *);
static int verify_many(mportInstance *mport, int argc, char *argv[], bool skipFirst);

static int lock(mportInstance *, const char *);

static int unlock(mportInstance *, const char *);

static int which(mportInstance *, const char *, bool);

static int audit(mportInstance *, bool);

static int audit_package(mportInstance *, const char*, bool);

static int selectMirror(mportInstance *mport);

static mportPackageMeta **
lookup_package(mportInstance *mport, const char *packageName);

int
main(int argc, char *argv[])
{
	char *flag = NULL, *buf = NULL;
	mportInstance *mport;
	int resultCode = MPORT_ERR_FATAL;
	int tempResultCode;
	int i;
	char **searchQuery;
	signed char ch;
	const char *chroot_path = NULL;
	const char *outputPath = NULL;
	int version = 0;
	int noIndex = 0;
	bool quiet = false;
	bool verbose = false;
	bool force = false;
	bool brief = false;

	struct option longopts[] = {
		{ "no-index", no_argument, NULL, 'U' },
		{ "verbose", no_argument, NULL, 'V' },
		{ "brief", no_argument, NULL, 'b'},
		{ "chroot", required_argument, NULL, 'c' },
		{ "force", no_argument, NULL, 'f' },
		{ "output", required_argument, NULL, 'o' },
		{ "quiet", no_argument, NULL, 'q'},
		{ "version", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0 },
	};

	if (argc < 2)
		usage();

	if (setenv("POSIXLY_CORRECT", "1", 1) == -1)
		err(EXIT_FAILURE, "setenv() failed");

	setlocale(LC_ALL, "");

	while ((ch = getopt_long(argc, argv, "+c:o:bfqUVv", longopts, NULL)) != -1) {
		switch (ch) {
		case 'U':
			noIndex++;
			break;
		case 'V':
			verbose = true;
			break;
		case 'b':
			brief = true;
			break;
		case 'c':
			chroot_path = optarg;
			break;
		case 'f':
			force = true;
			break;
		case 'o':
			outputPath = optarg;
			break;
		case 'q':
			quiet = true;
			break;
		case 'v':
			version++;
			break;
		default:
			errx(EXIT_FAILURE, "Invalid argument provided");
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (chroot_path != NULL) {
		if (chroot(chroot_path) == -1) {
			err(EXIT_FAILURE, "chroot failed");
		}
	}

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL, outputPath, noIndex != 0, mport_verbosity(quiet, verbose, brief)) != MPORT_OK) {
		errx(1, "%s", mport_err_string());
	}
	mport->force = force;

	if (version == 1) {
		show_version(mport, version);
		mport_instance_free(mport);
		exit(EXIT_SUCCESS);
	}

	char *cmd = argv[0];

	if (cmd == NULL)
		usage();

	if (!strcmp(cmd, "add")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}

		int local_argc = argc;
		char *const *local_argv = argv;
		int aflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "A")) != -1) {
				switch (ch2) {
				case 'A':
					aflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		mport->noIndex = true;
		mport->offline = true;

		for (i = 1; i < argc; i++) {
			tempResultCode = add(mport, argv[i], aflag == 1 ? MPORT_AUTOMATIC : MPORT_EXPLICIT);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "install")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}

		int local_argc = argc;
		char *const *local_argv = argv;
		int aflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "A")) != -1) {
				switch (ch2) {
				case 'A':
					aflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		loadIndex(mport);
		for (i = 1; i < argc; i++) {
			tempResultCode = install(mport, argv[i], aflag == 1 ? MPORT_AUTOMATIC : MPORT_EXPLICIT);
			if (tempResultCode != 0)
				resultCode = tempResultCode;
		}
	} else if (!strcmp(cmd, "delete")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		resultCode = deleteMany(mport, argc, argv, true);
	} else if (!strcmp(cmd, "update")) {
		if (argc == 1) {
			mport_instance_free(mport);
			usage();
		}
		loadIndex(mport);

		if (strchr(argv[1], '*') != NULL) {
			mportPackageMeta **packs = NULL;
			mportPackageMeta **packs_orig = NULL;
			char *pkg = mport_string_replace(argv[1], "*", "%");
			if (mport_pkgmeta_search_master(mport, &packs, "pkg like %Q", pkg) != MPORT_OK) {
				warnx("%s", mport_err_string());
				mport_instance_free(mport);
				return (MPORT_ERR_FATAL);
			}

			if (packs == NULL) {
				warnx("No packages installed matching '%s'", argv[1]);
				return (MPORT_ERR_FATAL);
			}

			packs_orig = packs;
			while (*packs != NULL) {
				mport_update(mport, (*packs)->name);
				packs++;
			}
			mport_pkgmeta_free(*packs_orig);
		} else { 
			for (i = 1; i < argc; i++) {
				tempResultCode = mport_update(mport, argv[i]);
				if (tempResultCode != MPORT_OK) {
					resultCode = tempResultCode;
					mport_call_msg_cb(mport, "Error updating package %s: %s", argv[i], mport_err_string());
				}
			}
		}
	} else if (!strcmp(cmd, "download")) {
		loadIndex(mport);
		char *path;

		int local_argc = argc;
		char *const *local_argv = argv;
		int aflag = 0;
		int dflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "ad")) != -1) {
				switch (ch2) {
				case 'a':
					aflag = 1;
					break;
				case 'd':
					dflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		if (aflag) {
			resultCode = mport_download(mport, NULL, true, false, &path);
		} else {
			for (i = 1; i < argc; i++) {
				tempResultCode = mport_download(mport, argv[i], aflag == 1, dflag == 1, &path);
				if (tempResultCode != 0) {
					resultCode = tempResultCode;
				} else if (path != NULL) {
					free(path);
				}
			}
		}
	} else if (!strcmp(cmd, "upgrade")) {
		loadIndex(mport);
		resultCode = mport_upgrade(mport);
	} else if (!strcmp(cmd, "audit")) {
		loadIndex(mport);

		int local_argc = argc;
		char *const *local_argv = argv;
		int rflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "r")) != -1) {
				switch (ch2) {
				case 'r':
					rflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		if (argc > 1) {
			resultCode = audit_package(mport, argv[1], rflag > 0);
		} else {
			resultCode = audit(mport, rflag > 0);
		}
	} else if (!strcmp(cmd, "locks")) {
		asprintf(&buf, "%s%s", MPORT_TOOLS_PATH, "mport.list");
		flag = strdup("-l");
		resultCode = execl(buf, "mport.list", flag, (char *)0);
		free(flag);
		free(buf);
	} else if (!strcmp(cmd, "import")) {
		loadIndex(mport);
		resultCode = mport_import(mport, argv[2]);
	} else if (!strcmp(cmd, "export")) {
		resultCode = mport_export(mport, argv[2]);
	} else if (!strcmp(cmd, "lock")) {
		if (argc > 1) {
			lock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "unlock")) {
		if (argc > 1) {
			unlock(mport, argv[1]);
		} else {
			usage();
		}
	} else if (!strcmp(cmd, "list")) {
		mportListPrint opts;
		opts.verbose = false;
		opts.locks = false;
		opts.prime = false;
		opts.update = false;
		opts.origin = false;
		
		if (argc > 1) {
			if (!strcmp(argv[1], "updates") || !strcmp(argv[1], "up")) {
				opts.update = true;
				loadIndex(mport);
			} else if (!strcmp(argv[1], "prime")) {
				opts.prime = true;
			} else {
				mport_instance_free(mport);
				usage();
			}
		} else {
			opts.verbose = true;
		}
		resultCode = mport_list_print(mport, &opts);
	} else if (!strcmp(cmd, "info")) {
		loadIndex(mport);
		resultCode = info(mport, argv[1]);
	} else if (!strcmp(cmd, "index")) {
		resultCode = mport_index_get(mport);
		if (resultCode != MPORT_OK) {
			fprintf(stderr, "Unable to fetch index: %s\n", mport_err_string());
		}
	} else if (!strcmp(cmd, "search")) {
		loadIndex(mport);
		searchQuery = calloc((size_t)argc, sizeof(char *));
		for (i = 1; i < argc; i++) {
			searchQuery[i - 1] = strdup(argv[i]);
		}
		resultCode = search(mport, searchQuery);
		for (i = 1; i < argc; i++) {
			free(searchQuery[i - 1]);
		}
		free(searchQuery);
	} else if (!strcmp(cmd, "shell")) {
		asprintf(&buf, "%s/%s", "/usr/bin", "sqlite3");
                flag = strdup("/var/db/mport/master.db");
                resultCode = execl(buf, "sqlite3", flag, (char *)0);
                free(flag);
                free(buf);
	} else if (!strcmp(cmd, "stats")) {
		loadIndex(mport);
		resultCode = stats(mport);
	} else if (!strcmp(cmd, "clean")) {
		loadIndex(mport);
		resultCode = clean(mport);
	} else if (!strcmp(cmd, "config")) {
		if (argc < 2) {
			mport_instance_free(mport);
			usage();
		}

		if (!strcmp(argv[1], "list")) {
			char** result = mport_setting_list(mport);
			char **ptr = result;
			if (result != NULL) {
				int c = 0; 
				while (*result != NULL) {
					printf("%s\n", *result);
					result++;
					c++;
				}
				for (int j = 0; j < c; j++)
					free(ptr[j]);
				free(ptr);
			}
			resultCode = MPORT_OK;
		} else if (!strcmp(argv[1], "get")) {
			resultCode = configGet(mport, argv[2]);
		} else if (!strcmp(argv[1], "set")) {
			resultCode = configSet(mport, argv[2], argv[3]);
		}
	} else if (!strcmp(cmd, "mirror")) {
		if (argc < 2) {
			mport_instance_free(mport);
			usage();
		}
		if (!strcmp(argv[1], "list")) {
			loadIndex(mport);
			printf("To set a mirror, use the following command:\n");
			printf("mport set config mirror_region <country>\n\n");
			resultCode = mport_index_print_mirror_list(mport);
		} else if (!strcmp(argv[1], "select")) {
			loadIndex(mport);
			resultCode = selectMirror(mport);	
		}
	} else if (!strcmp(cmd, "cpe")) {
		if (argc == 1) {
			resultCode = cpeList(mport);
		} else {
			for (i = 1; i < argc; i++) {
				tempResultCode = cpeGet(mport, argv[i]);
				if (tempResultCode!= 0)
					resultCode = tempResultCode;
			}
		}
	} else if (!strcmp(cmd, "purl")) {
		if (argc == 1) {
			resultCode = purlList(mport);
		} else {
			for (i = 1; i < argc; i++) {
				tempResultCode = purlGet(mport, argv[i]);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
			}
		}
	} else if (!strcmp(cmd, "deleteall")) {
		resultCode = deleteAll(mport);
	} else if (!strcmp(cmd, "autoremove")) {
		resultCode = mport_autoremove(mport);
	} else if (!strcmp(cmd, "verify")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		int rflag = 0;

		if (local_argc > 1) {
			int ch2;
			while ((ch2 = getopt(local_argc, local_argv, "r")) != -1) {
				switch (ch2) {
				case 'r':
					rflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
		}

		if (rflag) {
			resultCode = MPORT_OK;
			for (int x = 0; x < local_argc; x++) {
				mportPackageMeta **packs = lookup_package(mport, local_argv[x]);
				if (packs == NULL) {
					continue;
				}
				tempResultCode = mport_recompute_checksums(mport, *packs);
				if (tempResultCode != 0)
					resultCode = tempResultCode;
				mport_pkgmeta_vec_free(packs);
			}
		} else {
			if (argc > 1) {
				resultCode = verify_many(mport, argc, argv, true);
			} else {
				resultCode = verify(mport);
			}
		}
	} else if (!strcmp(cmd, "version")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, tflag;
			tflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "t")) != -1) {
				switch (ch2) {
				case 't':
					tflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;

			if (tflag) {
				if (local_argv[0] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				if (local_argv[1] == NULL) {
					fprintf(stderr, "Usage: mport version -t <v1> <v2>\n");
					return -2;
				}
				resultCode = mport_version_cmp(local_argv[0], local_argv[1]);
				printf("%c\n",
				    resultCode == 0	 ? '=' :
					resultCode == -1 ? '<' :
							   '>');
			}
		}
	} else if (!strcmp(cmd, "which")) {
		int local_argc = argc;
		char *const *local_argv = argv;
		if (local_argc > 1) {
			int ch2, oflag;
			oflag = 0;
			while ((ch2 = getopt(local_argc, local_argv, "qo")) != -1) {
				switch (ch2) {
				case 'q':
					mport->verbosity = MPORT_VQUIET;
					break;
				case 'o':
					oflag = 1;
					break;
				}
			}
			local_argc -= optind;
			local_argv += optind;
			which(mport, *local_argv, oflag);
		} else {
			usage();
		}
	} else {
		mport_instance_free(mport);
		usage();
	}

	mport_instance_free(mport);
	exit(resultCode);
}

void
usage(void)
{
	show_version(NULL, 2);

	fprintf(stderr,
	    "usage: mport [-c chroot dir] [-o output] [-b|q|V] [-fUV] <command> [args...]\n\n"
	    "Global options:\n"
	    "  -c <dir>    Set chroot directory\n"
	    "  -o <file>   Set output file\n"
	    "  -f          Force operation\n"
	    "  -q          Quiet mode\n"
	    "  -b          Brief output\n\n"
	    "  -V          Verbose mode\n"
	    "  -U          No index update\n"
	    "  -v          Show version\n"
	    "Commands:\n"
	    "  Package Management:\n"
	    "    add [-A] <package file>     Install package from file\n"
	    "    install [-A] <package>      Install package from repository\n"
	    "    delete <package>            Remove installed package\n"
	    "    update [package]            Update installed package(s)\n"
	    "    upgrade                     Upgrade all outdated packages\n"
	    "    autoremove                  Remove automatically installed packages\n"
	    "    clean                       Clean package cache\n"
	    "    verify [-r] [package]            Verify installed packages\n"
		"    deleteall                   Remove all installed packages\n\n"
	    "  Information:\n"
	    "    search <query>              Search for packages\n"
	    "    info <package>              Display package information\n"
	    "    list [updates|prime]        List installed packages\n"
	    "    which [-qo] <file>          Find which package provides a file\n"
	    "    stats                       Show package statistics\n\n"
	    "  Index and Repository:\n"
	    "    index                       Update package index\n"
	    "    mirror list                 List available mirrors\n"
	    "    mirror select               Select fastest mirror\n"
	    "    download [-ad] <package>    Download package without installing\n\n"
	    "  Configuration:\n"
	    "    config list                 List all settings\n"
	    "    config get <setting>        Get value of a setting\n"
	    "    config set <setting> <val>  Set value of a setting\n\n"
	    "  Security:\n"
	    "    audit [package]             Check for security vulnerabilities\n"
	    "    lock <package>              Lock package against modifications\n"
	    "    unlock <package>            Unlock package\n"
	    "    locks                       List locked packages\n"
	    "    cpe [package]               List Common Platform Enumeration info\n"
	    "    purl [package]              List Package URL info\n"
	    "  Miscellaneous:\n"
	    "    import <file>               Import package list\n"
	    "    export <file>               Export package list\n"
	    "    shell                       Open SQLite shell for package database\n"
	    "    version -t <v1> <v2>        Compare two version strings\n");
	exit(EXIT_FAILURE);
}

void
show_version(mportInstance *mport, int count)
{
	char *version = NULL;
	if (count == 1)
		version = mport_version_short(mport);
	else
		version = mport_version(mport);
	fprintf(stderr, "%s", version);
	if (mport == NULL)
		fprintf(stderr, "(Host OS version, not configured)\n\n");
	free(version);
}

static int
loadIndex(mportInstance *mport)
{
	int result = mport_index_load(mport);
	if (result == MPORT_ERR_WARN)
		warnx("%s", mport_err_string());
	else if (result != MPORT_OK)
		errx(4, "Unable to load index %s", mport_err_string());
	return result;
}

mportIndexEntry **
lookupIndex(mportInstance *mport, const char *packageName)
{
	mportIndexEntry **indexEntries = NULL;

	if (mport_index_lookup_pkgname(mport, packageName, &indexEntries) != MPORT_OK) {
		fprintf(stderr, "Error looking up package name %s: %d %s\n", packageName,
		    mport_err_code(), mport_err_string());
		errx(mport_err_code(), "%s", mport_err_string());
	}

	return (indexEntries);
}

int
selectMirror(mportInstance *mport)
{
	mportMirrorEntry **mirrorEntry = NULL;
	char hostname[256];
 
	mport_index_mirror_list(mport, &mirrorEntry);
	 
	long fastest = 1000;
	const char *country = "us";

	while(mirrorEntry != NULL && *mirrorEntry != NULL) {
		char *p = strchr((*mirrorEntry)->url, '/');
		if (p!= NULL) {
			*p = '\0';
			p++;
			p++;
        	}
		char *end = strchr(p, '/');
		if (end!= NULL) {
			*end = '\0';
       		}
		strlcpy(hostname, p, sizeof(hostname));
		mport_call_msg_cb(mport, "Trying mirror %s %s", (*mirrorEntry)->country, hostname);
		long rtt = ping(hostname);

		if (rtt != -1 && rtt < fastest) {
			fastest = rtt;
			country = (*mirrorEntry)->country;
        	}

		mirrorEntry++;
	}

	mport_call_msg_cb(mport, "Using mirror %s with rtt %ld ms\n", country, fastest);
	int result = mport_setting_set(mport, MPORT_SETTING_MIRROR_REGION, country);

	if (result != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	mport_index_mirror_entry_free_vec(mirrorEntry);

	return MPORT_OK;
}

int
search(mportInstance *mport, char **query)
{
	mportIndexEntry **indexEntry = NULL;

	if (query == NULL || *query == NULL) {
		fprintf(stderr, "Search terms required\n");
		return (1);
	}

	while (query != NULL && *query != NULL) {
		mport_index_search_term(mport, &indexEntry, *query);
		if (indexEntry == NULL || *indexEntry == NULL) {
			query++;
			continue;
		}

		while (indexEntry != NULL && *indexEntry != NULL) {
			fprintf(stdout, "%s\t%s\t%s\n", (*indexEntry)->pkgname,
			    (*indexEntry)->version, (*indexEntry)->comment);
			indexEntry++;
		}

		mport_index_entry_free_vec(indexEntry);
		query++;
	}

	return (0);
}

static int
lock(mportInstance *mport, const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);

	if (packs != NULL) {
		mport_lock_lock(mport, (*packs));
		mport_pkgmeta_free(*packs);
		return (MPORT_OK);
	}

	return (MPORT_ERR_FATAL);
}

static int
unlock(mportInstance *mport, const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);

	if (packs != NULL) {
		mport_lock_unlock(mport, (*packs));
		mport_pkgmeta_free(*packs);
		return (MPORT_OK);
	}

	return (MPORT_ERR_FATAL);
}

static int
stats(mportInstance *mport)
{
	char flatsize_str[8];
	mportStats *s = NULL;
	if (mport_stats(mport, &s) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	humanize_number(flatsize_str, sizeof(flatsize_str), s->pkg_installed_size, "B", HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);

	printf("Local package database:\n");
	printf("\tInstalled packages: %d\n", s->pkg_installed);
	printf("\tDisk space occupied: %s\n", flatsize_str);
	printf("\nRemote package database:\n");
	printf("\tPackages available: %d\n", s->pkg_available);

	free(s);

	return (0);
}

int
info(mportInstance *mport, const char *packageName)
{
	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (1);
	}

	char *out = mport_info(mport, packageName);
	if (out == NULL) {
		warnx("%s", mport_err_string());
		return (1);
	}

	printf("%s", out);
	free(out);

	return (0);
}

int
which(mportInstance *mport, const char *filePath, bool origin)
{
	mportPackageMeta *pack = NULL;

	if (filePath == NULL) {
		warnx("%s", "Specify file path");
		return (1);
	}

	if (mport_asset_get_package_from_file_path(mport, filePath, &pack) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	mport_drop_privileges();

	if (pack != NULL && pack->origin != NULL) {
		if (mport->verbosity == MPORT_VQUIET && origin) {
			printf("%s\n", pack->origin);
		} else if (mport->verbosity == MPORT_VQUIET) {
			printf("%s-%s\n", pack->name, pack->version);
		} else if (origin) {
			printf("%s was installed by package %s\n", filePath, pack->origin);
		} else {
			printf("%s was installed by package %s-%s\n", filePath, pack->name,
			    pack->version);
		}
	}

	return (0);
}

int
add(mportInstance *mport, const char *filename, mportAutomatic automatic) {
	return mport_install_primative(mport, filename, NULL, automatic);
}

int
install(mportInstance *mport, const char *packageName, mportAutomatic automatic)
{
	mportIndexEntry **indexEntry = NULL;
	mportIndexEntry **ie = NULL;
	mportIndexEntry **i2 = NULL;
	int resultCode = MPORT_OK;
	int item;
	int choice;

	indexEntry = lookupIndex(mport, packageName);
	if (indexEntry == NULL || *indexEntry == NULL) {
		int loc = -1;
		size_t len = strlen(packageName);
		for (int i = len - 1; i >= 0; i--) {
			if (packageName[i] == '-') {
				loc = i;
				break;
			}
		}

		if (loc > 0) {
			char *d = strdup(packageName);
			char *v = &d[loc + 1];
			d[loc] = '\0'; /* hack off the version number */
			indexEntry = lookupIndex(mport, d);
			if (indexEntry == NULL || v == NULL || (*indexEntry) == NULL ||
			    strcmp(v, (*indexEntry)->version) != 0) {
				errx(4, "Package %s not found in the index.", packageName);
			}
			free(d);
		}
	}

	if (indexEntry == NULL || *indexEntry == NULL)
		errx(4, "Package %s not found in the index.", packageName);

	ie = indexEntry;
	if (indexEntry[1] != NULL) {
		printf("Multiple packages found. Please select one:\n");
		i2 = indexEntry;
		item = 0;
		while (*i2 != NULL) {
			printf("%d. %s-%s\n", item, (*i2)->pkgname, (*i2)->version);
			item++;
			i2++;
		}
		while (scanf("%d", &choice) < 1 || choice > item || choice < 0) {
			fprintf(stderr, "Please select an entry 0 - %d\n", item - 1);
		}
		item = 0;
		while (indexEntry != NULL) {
			if (item == choice)
				break;
			item++;
			indexEntry++;
		}
	}

	if (indexEntry != NULL && *indexEntry != NULL) {
		resultCode = mport_install(mport, (*indexEntry)->pkgname, (*indexEntry)->version, NULL, automatic);
	}

	mport_index_entry_free_vec(ie);

	return (resultCode);
}

int
deleteMany(mportInstance *mport, int argc, char *argv[], bool skipFirst)
{
    mportPackageMeta **packs = NULL;
    mportPackageMeta **packs_orig = NULL;
    int start = skipFirst ? 1 : 0;
    int package_count = 0;
	int missing = 0;
	int locked = 0;
    long long total_flatsize = 0;
    char flatsize_str[8];
	int resultCode = MPORT_OK;

	printf("Installed packages to be REMOVED:\n\n");

    // First pass: count packages and calculate total flatsize
    for (int i = start; i < argc; i++) {
        if (mport_pkgmeta_search_master(mport, &packs_orig, "LOWER(pkg)=LOWER(%Q)", argv[i]) != MPORT_OK) {
            warnx("%s", mport_err_string());
			missing++;
            continue;
        }

        if (packs_orig == NULL) {
            warnx("No packages installed matching '%s'", argv[i]);
			missing++;
            continue;
        }

        packs = packs_orig;
        while (*packs != NULL) {
			printf("\t%s: %s", (*packs)->name, (*packs)->version);
            if ((*packs)->flavor != NULL) {
                printf(",%s", (*packs)->flavor);
            }
            printf("\n");

			if (mport_lock_islocked((*packs)) == MPORT_LOCKED) {
                locked++;
            }

            package_count++;
            total_flatsize += (*packs)->flatsize;
            packs++;
        }
		printf("\n");

        mport_pkgmeta_vec_free(packs_orig);
        packs_orig = NULL;
    }

	if (package_count == 0 || locked > 0 || missing > 0) {
		printf("%d packages requested for removal: %d locked, %d missing\n", argc - start, locked, missing);
	}

	if (package_count == 0) {
		return (MPORT_ERR_WARN); // No packages to delete
	}

    // Convert total_flatsize to human-readable format
    humanize_number(flatsize_str, sizeof(flatsize_str), total_flatsize, "B", HN_AUTOSCALE, HN_DECIMAL | HN_IEC_PREFIXES);

    // Display information and ask for confirmation
    printf("Packages to be deleted: %d\n", package_count);
    printf("Total disk space to be freed: %s\n", flatsize_str);

    if ((mport->confirm_cb)("Proceed with deinstalling packages?", "Delete", "Don't delete", 0) != MPORT_OK) {
        return (MPORT_ERR_WARN); // User chose not to proceed
    }

    // Second pass: actually delete the packages
    for (int i = start; i < argc; i++) {
        if (mport_pkgmeta_search_master(mport, &packs_orig, "LOWER(pkg)=LOWER(%Q)", argv[i]) != MPORT_OK) {
            warnx("%s", mport_err_string());
            continue;
        }

        if (packs_orig == NULL) {
            warnx("No packages installed matching '%s'", argv[i]);
            continue;
        }

        packs = packs_orig;
        while (*packs != NULL) {

			if (mport_lock_islocked((*packs)) == MPORT_LOCKED) {
				warnx("Package '%s' is locked. skipping", argv[i]);
                continue;
            }

            (*packs)->action = MPORT_ACTION_DELETE;
            if (mport_delete_primative(mport, *packs, mport->force) != MPORT_OK) {
                warnx("%s", mport_err_string());
            }
            packs++;
        }

        mport_pkgmeta_vec_free(packs_orig);
        packs_orig = NULL;
    }

    return (resultCode);
}

static mportPackageMeta **
lookup_package(mportInstance *mport, const char *packageName)
{
    mportPackageMeta **packs = NULL;

	if (packageName == NULL) {
		warnx("%s", "Specify package name");
		return (NULL);
	}

    if (mport_pkgmeta_search_master(mport, &packs, "LOWER(pkg)=LOWER(%Q)", packageName) != MPORT_OK) {
        warnx("%s", mport_err_string());
        mport_pkgmeta_vec_free(packs);
        return NULL;
    }

    if (packs == NULL) {
        warnx("No packages installed matching '%s'", packageName);
        return NULL;
    }

    return packs;
}

int 
delete(mportInstance *mport, const char *packageName)
{
	mportPackageMeta **packs = lookup_package(mport, packageName);
    if (packs == NULL) {
        return (MPORT_ERR_FATAL);
    }

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		(*packs)->action = MPORT_ACTION_DELETE;
		if (mport_delete_primative(mport, *packs, mport->force) != MPORT_OK) {
			warnx("%s", mport_err_string());
			mport_pkgmeta_vec_free(packs_orig);
			return (MPORT_ERR_FATAL);
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	return (MPORT_OK);
}

int
configGet(mportInstance *mport, const char *settingName)
{
	char *val = NULL;

	val = mport_setting_get(mport, settingName);

	mport_drop_privileges();

	if (val != NULL) {
		printf("Setting %s value is %s\n", settingName, val);
	} else {
		printf("Setting %s is undefined.\n", settingName);
	}

	return 0;
}

int
configSet(mportInstance *mport, const char *settingName, const char *val)
{
	int result = mport_setting_set(mport, settingName, val);

	mport_drop_privileges();

	if (result != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	return 0;
}

int
purlGet(mportInstance *mport, const char *packageName)
{
	int purl_total = 0;

    mportPackageMeta **packs = lookup_package(mport, packageName);

	mport_drop_privileges();

    if (packs == NULL) {
        return (MPORT_ERR_FATAL);
    }

    mportPackageMeta **packs_orig = packs;
    while (*packs != NULL) {
	    char *purl = mport_purl_uri(*packs);
		if (purl != NULL) {
	    	printf("%s\n", purl);
	    	free(purl);
		}
	    
	    purl_total++;
	    packs++;
    }

	mport_pkgmeta_vec_free(packs_orig);

	if (purl_total == 0) {
		return (MPORT_ERR_WARN);
	}

	return (MPORT_OK);
}

int
purlList(mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **packs_orig = NULL;
	int purl_total = 0;

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	mport_drop_privileges();

	if (packs_orig == NULL) {
		warnx("No packages installed.");
		return (1);
	}

	packs = packs_orig;
	while (*packs != NULL) {
	    char *purl = mport_purl_uri(*packs);
		if (purl != NULL) {
	    	printf("%s", purl);
	    	free(purl);
		}
	
		purl_total++;
		packs++;
	}
	mport_pkgmeta_vec_free(packs_orig);

	if (purl_total == 0) {
		errx(EX_SOFTWARE, "No packages contained PURL information.");
	}

	return (0);
}

int
cpeGet(mportInstance *mport, const char *packageName)
{
	int cpe_total = 0;

    mportPackageMeta **packs = lookup_package(mport, packageName);

	mport_drop_privileges();

	if (packs == NULL) {
		return (MPORT_ERR_WARN);
	}

    mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		if ((*packs)->cpe != NULL && strlen((*packs)->cpe) > 0) {
			printf("%s\n", (*packs)->cpe);
			cpe_total++;
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs_orig);

	if (cpe_total == 0) {
		return (MPORT_ERR_WARN);
	}

	return (MPORT_OK);
}

int
cpeList(mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **packs_orig = NULL;
	int cpe_total = 0;

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	mport_drop_privileges();

	if (packs_orig == NULL) {
		warnx("No packages installed.");
		return (1);
	}

    packs = packs_orig;
	while (*packs != NULL) {
		if ((*packs)->cpe != NULL && strlen((*packs)->cpe) > 0) {
			printf("%s\n", (*packs)->cpe);
			cpe_total++;
		}
		packs++;
	}
	mport_pkgmeta_vec_free(packs_orig);

	if (cpe_total == 0) {
		errx(EX_SOFTWARE, "No packages contained CPE information.");
	}

	return (0);
}

int
verify_many(mportInstance *mport, int argc, char *argv[], bool skipfirst) 
{
	int total = 0;
	int start = skipfirst ? 1 : 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: mport verify <package>\n");
		return (MPORT_ERR_WARN);
	}

	for (int i = start; i < argc; i++) {
		mportPackageMeta **pkg_matches = lookup_package(mport, argv[i]);

		if (pkg_matches == NULL) {
			continue;
		}
		
		for (mportPackageMeta **pkg = pkg_matches; *pkg != NULL; pkg++) {
			mport_verify_package(mport, *pkg);
			total++;
		}
		
		mport_pkgmeta_vec_free(pkg_matches);
	}

	printf("Packages verified: %d\n", total);

	return (MPORT_OK);
}

int
verify(mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	int total = 0;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return mport_err_code();
	}

	if (packs == NULL) {
		warnx("No packages installed.");
		return (MPORT_ERR_WARN);
	}

	for (mportPackageMeta **pack = packs; *pack != NULL; pack++) {
		mport_verify_package(mport, *pack);
		total++;
	}

	mport_pkgmeta_vec_free(packs);
	printf("Packages verified: %d\n", total);

	return (MPORT_OK);
}

int
deleteAll(mportInstance *mport)
{
	mportPackageMeta **packs = NULL;
	mportPackageMeta **depends = NULL;
	int total = 0;
	int errors = 0;
	int skip;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return (1);
	}

	if ((mport->confirm_cb)("Proceed with removing all packages on the system?", "Delete", "Don't delete", 0) != MPORT_OK) {
		return (MPORT_ERR_WARN); // User chose not to proceed
	}

	while (1) {
		skip = 0;
		while (*packs != NULL) {
			if (mport_pkgmeta_get_updepends(mport, *packs, &depends) == MPORT_OK) {
				if (depends == NULL) {
					if (delete (mport, (*packs)->name) != MPORT_OK) {
						fprintf(
						    stderr, "Error deleting %s\n", (*packs)->name);
						errors++;
					}
					total++;
				} else {
					skip++;
					mport_pkgmeta_vec_free(depends);
				}
			}
			packs++;
		}
		if (skip == 0)
			break;
		mport_pkgmeta_vec_free(packs);
		if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
			warnx("%s", mport_err_string());
			return (1);
		}
	}

	mport_pkgmeta_vec_free(packs);

	printf("Packages deleted: %d\nErrors: %d\nTotal: %d\n", total - errors, errors, total);
	return (0);
}

int
clean(mportInstance *mport)
{
	int ret;
	int result = MPORT_OK;

	ret = mport_clean_database(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_oldpackages(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_oldmtree(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}

	ret = mport_clean_tempfiles(mport);
	if (ret != MPORT_OK) {
		result = ret;
	}
		
	return (result);
}

int
audit(mportInstance *mport, bool dependsOn)
{
	mportPackageMeta **packs = NULL;

	if (mport_pkgmeta_list(mport, &packs) != MPORT_OK) {
		warnx("%s", mport_err_string());
		return (1);
	}

	if (packs == NULL) {
		fprintf(stderr, "No packages installed.\n");
		return (1);
	}

	while (*packs != NULL) {
		char *output = mport_audit(mport, (*packs)->name, dependsOn);
		if (output != NULL && output[0] != '\0') {
			if (mport->verbosity == MPORT_VQUIET)
				printf("%s", output);
			else
				printf("%s\n", output);
			free(output);
		}
		packs++;
	}

	mport_pkgmeta_vec_free(packs);

	return (0);
}

int 
audit_package(mportInstance *mport, const char *packageName, bool dependsOn) 
{
    mportPackageMeta **packs = lookup_package(mport, packageName);
    if (packs == NULL) {
        return (MPORT_ERR_FATAL);
    }

	mportPackageMeta **packs_orig = packs;
	while (*packs != NULL) {
		char *output = mport_audit(mport, (*packs)->name, dependsOn);
		if (output != NULL && output[0] != '\0') {
			if (mport->verbosity == MPORT_VQUIET)
				printf("%s", output);
			else
				printf("%s\n", output);
			free(output);
		}
		packs++;
	}

    mport_pkgmeta_vec_free(packs_orig);

	return (0);
}
