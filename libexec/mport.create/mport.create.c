/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014, 2025 Lucas Holt
 * Copyright (c) 2007 Chris Reinhardt
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

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mport.h>

#define MPORT_LUA_PRE_INSTALL_FILE "pkg-pre-install.lua"
#define MPORT_LUA_POST_INSTALL_FILE "pkg-post-install.lua"
#define MPORT_LUA_PRE_DEINSTALL_FILE "pkg-pre-deinstall.lua"
#define MPORT_LUA_POST_DEINSTALL_FILE "pkg-post-deinstall.lua"

static void usage(void);

static void check_for_required_args(const mportPackageMeta *, const mportCreateExtras *);

int main(int argc, char *argv[])
{
	int ch;
	int plist_seen = 0;
	mportInstance *mport = mport_instance_new();
	mportPackageMeta *pack = mport_pkgmeta_new();
	mportCreateExtras *extra = mport_createextras_new();
	mportAssetList *assetlist = mport_assetlist_new();
	FILE *fp;
	struct tm expDate;
	int result = EXIT_SUCCESS;

	if (mport == NULL || pack == NULL || extra == NULL || assetlist == NULL) {
		errx(EXIT_FAILURE, "Failed to allocate memory");
	}

	// we need this to know if the user customized the "target_os" configuration.
	// the caveat is that the userland it was built against could be wrong.
	if (mport_instance_init(mport, NULL, NULL, false, false) != MPORT_OK) {
		mport_instance_free(mport);
		mport_pkgmeta_free(pack);
		mport_createextras_free(extra);
		mport_assetlist_free(assetlist);
		errx(EXIT_FAILURE, "%s", mport_err_string());
	}

	while ((ch = getopt(argc, argv, "C:D:E:L:M:O:P:S:c:d:e:f:i:j:l:m:n:o:p:r:s:t:v:x:")) != -1) {
		switch (ch) {
			case 'o':
				strlcpy(extra->pkg_filename, optarg, sizeof(extra->pkg_filename));
				break;
			case 'n':
				if (optarg != NULL) {
					pack->name = strdup(optarg);
				}
				break;
			case 'v':
				if (optarg != NULL) {
					pack->version = strdup(optarg);
				}
				break;
			case 'c':
				if (optarg != NULL) {
					pack->comment = strdup(optarg);
				}
				break;
			case 'f':
				if (optarg != NULL) {
					pack->flavor = strdup(optarg);
				}
				break;
			case 'e':
				if (optarg != NULL) {
					pack->cpe = strdup(optarg);
				}
				break;
			case 'l':
				if (optarg != NULL) {
					pack->lang = strdup(optarg);
				}
				break;
			case 's':
				strlcpy(extra->sourcedir, optarg, sizeof(extra->sourcedir));
				break;
			case 'd':
				if (optarg != NULL) {
					pack->desc = strdup(optarg);
				}
				break;
			case 'p':
				if ((fp = fopen(optarg, "r")) == NULL) {
					err(1, "%s", optarg);
				}
				if (mport_parse_plistfile(fp, assetlist) != 0) {
					warnx("Could not parse plist file '%s'.\n", optarg);
					fclose(fp);
					result = EXIT_FAILURE;
					goto cleanup;
				}
				fclose(fp);

				plist_seen++;

				break;
			case 'P':
				if (optarg != NULL) {
					pack->prefix = strdup(optarg);
				}
				break;
			case 'D':
				mport_parselist(optarg, &(extra->depends), &(extra->depends_count));
				break;
			case 'M':
				extra->mtree = strdup(optarg);
				break;
			case 'O':
				if (optarg != NULL) {
					pack->origin = strdup(optarg);
				}
				break;
			case 'C':
				mport_parselist_tll(optarg, &(extra->conflicts));
				break;
			case 'E':
				strptime(optarg, "%Y-%m-%d", &expDate);
				pack->expiration_date = mktime(&expDate);
				break;
			case 'S':
				if (optarg[0] == '1' || optarg[0] == 'Y' || optarg[0] == 'y' || optarg[0] == 'T' || optarg[0] == 't')
					pack->no_provide_shlib = 1;
				else
					pack->no_provide_shlib = 0;
				break;
			case 'L':
				if (optarg != NULL) {
					asprintf(&extra->luapkgpostinstall, "%s/%s", optarg, MPORT_LUA_POST_INSTALL_FILE);
					asprintf(&extra->luapkgpreinstall, "%s/%s", optarg, MPORT_LUA_PRE_INSTALL_FILE);
					asprintf(&extra->luapkgpostdeinstall, "%s/%s", optarg, MPORT_LUA_POST_DEINSTALL_FILE);
					asprintf(&extra->luapkgpredeinstall, "%s/%s", optarg, MPORT_LUA_PRE_DEINSTALL_FILE);
				}
				break;
			case 'i':
				extra->pkginstall = strdup(optarg);
				break;
			case 'j':
				extra->pkgdeinstall = strdup(optarg);
				break;
			case 'm':
				extra->pkgmessage = strdup(optarg);
				break;
			case 't':
				mport_parselist(optarg, &(pack->categories), &(pack->categories_count));
				break;
			case 'x':
				if (optarg != NULL) {
					pack->deprecated = strdup(optarg);
				}
				break;
			case '?':
			default:
				usage();
				break;
		}
	}

	check_for_required_args(pack, extra);
	if (plist_seen == 0) {
		warnx("Required arg missing: plist");
		usage();
	}

	pack->type = MPORT_TYPE_APP; /* Todo: This should be configurable */

	if (mport_create_primative(mport, assetlist, pack, extra) != MPORT_OK) {
		warnx("%s", mport_err_string());
		result = EXIT_FAILURE;
		goto cleanup;
	}

cleanup:
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);
	/* TODO: fix asset free mport_assetlist_free(assetlist); */
	mport_instance_free(mport);

	return result;
}


#define CHECK_ARG(exp, errmsg) \
  if (exp == NULL) { \
    warnx("Required arg missing: %s", #errmsg); \
    usage(); \
  }

static void check_for_required_args(const mportPackageMeta *pkg, const mportCreateExtras *extra)
{
	CHECK_ARG(pkg->name, "package name")
	CHECK_ARG(pkg->version, "package version");
	CHECK_ARG(extra->pkg_filename, "package filename");
	CHECK_ARG(extra->sourcedir, "source dir");
	CHECK_ARG(pkg->prefix, "prefix");
	CHECK_ARG(pkg->origin, "origin");
	CHECK_ARG(pkg->categories, "categories");
}


static void usage(void)
{
	fprintf(stderr, "\nmport.create <arguments>\n");
	fprintf(stderr, "Arguments:\n");
	fprintf(stderr, "\t-n <package name>\n");
	fprintf(stderr, "\t-v <package version>\n");
	fprintf(stderr, "\t-o <package filename>\n");
	fprintf(stderr, "\t-s <source dir (usually the fake destdir)>\n");
	fprintf(stderr, "\t-p <plist filename>\n");
	fprintf(stderr, "\t-P <prefix>\n");
	fprintf(stderr, "\t-O <origin>\n");
	fprintf(stderr, "\t-c <comment (short description)>\n");
	fprintf(stderr, "\t-e <cpe string>\n");
	fprintf(stderr, "\t-l <package lang>\n");
	fprintf(stderr, "\t-D <package depends>\n");
	fprintf(stderr, "\t-C <package conflicts>\n");
	fprintf(stderr, "\t-d <pkg-descr file>\n");
	fprintf(stderr, "\t-i <pkg-install script>\n");
	fprintf(stderr, "\t-j <pkg-deinstall script>\n");
	fprintf(stderr, "\t-m <pkg-message file>\n");
	fprintf(stderr, "\t-M <mtree file>\n");
	fprintf(stderr, "\t-t <categories>\n");
	exit(1);
}

