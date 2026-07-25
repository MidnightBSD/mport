#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

/* SPLINT_SKIP_FILE: Splint cannot parse/model ATF test macros and fixture setup. */
/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT_TEMPLATE "/tmp/mport-install-test-root.XXXXXX"
#define PKG_NAME "testpkg"
#define PKG_VERSION "1.0"
#define PKG_PREFIX "/usr/local"
#define PKG_FILE_REL "share/testpkg/catalog.mk"
#define PKG_FILE_ABS PKG_PREFIX "/" PKG_FILE_REL

static char test_root[PATH_MAX];

static const char *
test_path(const char *suffix)
{
	static char paths[8][PATH_MAX];
	static unsigned int next_path;
	char *path;

	path = paths[next_path++ % 8];
	(void)snprintf(path, PATH_MAX, "%s%s", test_root, suffix);
	return path;
}

static void
cleanup_test_root(void)
{
	int cwd_fd;

	cwd_fd = open(".", O_RDONLY | O_DIRECTORY);
	if (test_root[0] != '\0' && access(test_root, F_OK) == 0)
		(void)mport_rmtree(test_root);
	if (cwd_fd >= 0) {
		(void)fchdir(cwd_fd);
		(void)close(cwd_fd);
	}
	test_root[0] = '\0';
}

static void
write_file(const char *path, const char *contents)
{
	int fd;
	size_t len;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(fd >= 0);
	len = strlen(contents);
	ATF_REQUIRE_EQ((ssize_t)len, write(fd, contents, len));
	ATF_REQUIRE_EQ(0, close(fd));
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/usr/local"), 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	return mport;
}

/* Build a minimal one-file package under the test root and return its path. */
static const char *
create_test_package(mportInstance *mport)
{
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	FILE *fp;

	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/share"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/share/testpkg"), 0755));
	write_file(test_path("/stage" PKG_FILE_ABS), "catalog\n");
	write_file(test_path("/plist"), PKG_FILE_REL "\n");

	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	fp = fopen(test_path("/plist"), "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup(PKG_NAME);
	pack->version = strdup(PKG_VERSION);
	pack->prefix = strdup(PKG_PREFIX);
	pack->origin = strdup("misc/testpkg");
	pack->lang = strdup("");
	pack->comment = strdup("test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)strlcpy(
	    extra->pkg_filename, test_path("/testpkg-1.0.mport"), sizeof(extra->pkg_filename));
	(void)strlcpy(extra->sourcedir, test_path("/stage"), sizeof(extra->sourcedir));

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());

	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);

	return test_path("/testpkg-1.0.mport");
}

/* Number of registry rows for the test package, and its os_release. */
static int
count_installed(mportInstance *mport, /*@out@*/ char *os_release, size_t os_release_len)
{
	mportPackageMeta **found = NULL;
	int count = 0;

	if (os_release != NULL)
		os_release[0] = '\0';

	ATF_REQUIRE_EQ(MPORT_OK, mport_pkgmeta_search_master(mport, &found, "pkg=%Q", PKG_NAME));
	if (found == NULL)
		return 0;

	for (count = 0; found[count] != NULL; count++) {
		if (count == 0 && os_release != NULL && found[0]->os_release != NULL)
			(void)strlcpy(os_release, found[0]->os_release, os_release_len);
	}

	mport_pkgmeta_vec_free(found);

	return count;
}

/*
 * Regression test for issue #180: a package installed under a previous OS
 * release must be replaced, not reported as a file conflict.
 */
ATF_TC_WITH_CLEANUP(install_replaces_previous_os_release);
ATF_TC_HEAD(install_replaces_previous_os_release, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "installing over a copy registered under an older os_release replaces it");
}
ATF_TC_BODY(install_replaces_previous_os_release, tc)
{
	mportInstance *mport;
	const char *pkgfile;
	char os_release[64];
	char *system_os_release;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	/* pretend the install happened on the previous OS release */
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "UPDATE packages SET os_release='0.0-OLD' WHERE pkg=%Q", PKG_NAME));

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());

	/* the stale copy is replaced, not duplicated */
	ATF_REQUIRE_EQ(1, count_installed(mport, os_release, sizeof(os_release)));
	system_os_release = mport_get_osrelease(mport);
	ATF_REQUIRE(system_os_release != NULL);
	ATF_REQUIRE_STREQ(system_os_release, os_release);
	free(system_os_release);
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(install_replaces_previous_os_release, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* Installing the same package twice under the current OS release is a no-op. */
ATF_TC_WITH_CLEANUP(install_same_os_release_is_rejected);
ATF_TC_HEAD(install_same_os_release_is_rejected, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "a package installed under the current os_release is left alone");
}
ATF_TC_BODY(install_same_os_release_is_rejected, tc)
{
	mportInstance *mport;
	const char *pkgfile;

	(void)tc;

	mport = create_test_instance();
	pkgfile = create_test_package(mport);

	ATF_REQUIRE_MSG(mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT) == MPORT_OK,
	    "%s", mport_err_string());
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));

	/* the second install must not delete or duplicate the installed copy */
	(void)mport_install_primative(mport, pkgfile, NULL, MPORT_EXPLICIT);
	ATF_REQUIRE_EQ(1, count_installed(mport, NULL, 0));
	ATF_REQUIRE_EQ(0, access(test_path(PKG_FILE_ABS), F_OK));

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(install_same_os_release_is_rejected, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, install_replaces_previous_os_release);
	ATF_TP_ADD_TC(tp, install_same_os_release_is_rejected);

	return atf_no_error();
}
