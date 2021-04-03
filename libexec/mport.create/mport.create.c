/*-
 * Copyright (c) 2014 Lucas Holt
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

static void usage(void);

static void check_for_required_args(const mportPackageMeta *, const mportCreateExtras *);

int main(int argc, char *argv[])
{
	int ch;
	int plist_seen = 0;
	mportPackageMeta *pack = mport_pkgmeta_new();
	mportCreateExtras *extra = mport_createextras_new();
	mportAssetList *assetlist = mport_assetlist_new();
	FILE *fp;
	struct tm expDate;

	while ((ch = getopt(argc, argv, "C:D:E:M:O:P:S:c:d:e:f:i:j:l:m:n:o:p:r:s:t:v:x:")) != -1) {
		switch (ch) {
			case 'o':
				extra->pkg_filename = optarg;
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
				extra->sourcedir = optarg;
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
					exit(1);
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
				mport_parselist(optarg, &(extra->depends));
				break;
			case 'M':
				extra->mtree = optarg;
				break;
			case 'O':
				if (optarg != NULL) {
					pack->origin = strdup(optarg);
				}
				break;
			case 'C':
				mport_parselist(optarg, &(extra->conflicts));
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
			case 'i':
				extra->pkginstall = optarg;
				break;
			case 'j':
				extra->pkgdeinstall = optarg;
				break;
			case 'm':
				extra->pkgmessage = optarg;
				break;
			case 't':
				mport_parselist(optarg, &(pack->categories));
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

	if (mport_create_primative(assetlist, pack, extra) != MPORT_OK) {
		warnx("%s", mport_err_string());
		exit(1);
	}

	return 0;
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

