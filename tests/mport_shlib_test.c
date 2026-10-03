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
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT_TEMPLATE "/tmp/mport-shlib-test-root.XXXXXX"
#define LIBZ "/lib/libz.so.6"
#define GZIP "/usr/bin/gzip"

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
make_test_root(void)
{
	(void)strlcpy(test_root, TEST_ROOT_TEMPLATE, sizeof(test_root));
	ATF_REQUIRE(mkdtemp(test_root) != NULL);
}

static void
cleanup_test_root(void)
{
	if (test_root[0] != '\0' && access(test_root, F_OK) == 0)
		(void)mport_rmtree(test_root);
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

static void
copy_file(const char *from, const char *to)
{
	FILE *in, *out;
	char buf[8192];
	size_t n;

	/* ATF_REQUIRE aborts the case on a NULL stream; cppcheck cannot see that */
	/* cppcheck-suppress-begin nullPointerOutOfResources */
	in = fopen(from, "rb");
	ATF_REQUIRE(in != NULL);
	out = fopen(to, "wb");
	ATF_REQUIRE(out != NULL);
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		ATF_REQUIRE_EQ(n, fwrite(buf, 1, n, out));
	ATF_REQUIRE_EQ(0, fclose(out));
	ATF_REQUIRE_EQ(0, fclose(in));
	/* cppcheck-suppress-end nullPointerOutOfResources */
}

static bool
list_has(const stringlist_t *list, const char *s)
{
	tll_foreach(*list, it)
	{
		if (strcmp(it->item, s) == 0)
			return true;
	}
	return false;
}

static void
clear_shlib_env(void)
{
	static const char *const names[] = { "SHLIB_PROVIDE_PATHS_NATIVE",
		"SHLIB_PROVIDE_PATHS_COMPAT_32", "SHLIB_PROVIDE_PATHS_COMPAT_LINUX",
		"SHLIB_PROVIDE_PATHS_COMPAT_LINUX_32", "SHLIB_PROVIDE_IGNORE_GLOB",
		"SHLIB_PROVIDE_IGNORE_REGEX", "SHLIB_REQUIRE_IGNORE_GLOB",
		"SHLIB_REQUIRE_IGNORE_REGEX" };

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
		(void)unsetenv(names[i]);
}

/* ---- mport_shlib_analyse_elf ------------------------------------------ */

ATF_TC(analyse_shared_library);
ATF_TC_HEAD(analyse_shared_library, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "a shared library yields its soname and its needs");
}
ATF_TC_BODY(analyse_shared_library, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(LIBZ, &provided, &flags, &required));
	ATF_REQUIRE(provided != NULL);
	ATF_REQUIRE_STREQ("libz.so.6", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_NATIVE, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&required, "libz.so.6"));

	free(provided);
	tll_free_and_free(required, free);
}

ATF_TC(analyse_executable);
ATF_TC_HEAD(analyse_executable, tc)
{
	atf_tc_set_md_var(tc, "require.files", GZIP);
	atf_tc_set_md_var(tc, "descr", "an executable provides nothing and lists its needs");
}
ATF_TC_BODY(analyse_executable, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(GZIP, &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE(list_has(&required, "libz.so.6"));
	ATF_REQUIRE(list_has(&required, "libc.so.7"));

	tll_free_and_free(required, free);
}

ATF_TC_WITH_CLEANUP(analyse_plain_file);
ATF_TC_HEAD(analyse_plain_file, tc)
{
	atf_tc_set_md_var(tc, "descr", "a non-ELF file is skipped without error");
}
ATF_TC_BODY(analyse_plain_file, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	make_test_root();
	write_file(test_path("/notes.txt"), "not an elf\n");
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_analyse_elf(test_path("/notes.txt"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE_EQ(0, tll_length(required));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf(test_path("/absent"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
}
ATF_TC_CLEANUP(analyse_plain_file, tc)
{
	(void)tc;

	cleanup_test_root();
}

#define LINUX_LIBZ "/compat/linux/lib64/libz.so.1"

ATF_TC(analyse_linux_object);
ATF_TC_HEAD(analyse_linux_object, tc)
{
	atf_tc_set_md_var(tc, "require.files", LINUX_LIBZ);
	atf_tc_set_md_var(tc, "descr", "an object not tagged for FreeBSD is a Linux object");
}
ATF_TC_BODY(analyse_linux_object, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	char target[PATH_MAX];

	/* the compat path is a symlink; analyse_elf reads regular files only */
	ATF_REQUIRE(realpath(LINUX_LIBZ, target) != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(target, &provided, &flags, &required));
	ATF_REQUIRE(provided != NULL);
	ATF_REQUIRE_STREQ("libz.so.1", provided);
	ATF_REQUIRE_EQ(MPORT_SHLIB_LINUX, flags);
	ATF_REQUIRE(list_has(&required, "libc.so.6:Linux"));

	free(provided);
	tll_free_and_free(required, free);
}

ATF_TC(name_with_flags);
ATF_TC_HEAD(name_with_flags, tc)
{
	atf_tc_set_md_var(tc, "descr", "non-native objects carry the pkg(8) suffixes");
}
ATF_TC_BODY(name_with_flags, tc)
{
	char *s;

	(void)tc;

	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_NATIVE);
	ATF_REQUIRE_STREQ("libfoo.so.1", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_COMPAT_32);
	ATF_REQUIRE_STREQ("libfoo.so.1:32", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_LINUX);
	ATF_REQUIRE_STREQ("libfoo.so.1:Linux", s);
	free(s);
	s = mport_shlib_name_with_flags("libfoo.so.1", MPORT_SHLIB_LINUX | MPORT_SHLIB_COMPAT_32);
	ATF_REQUIRE_STREQ("libfoo.so.1:Linux:32", s);
	free(s);
}

#define LDSO "/usr/libexec/ld-elf.so.1"

static void stage_lib_and_user(const char **, const char **);

ATF_TC(analyse_object_without_soname);
ATF_TC_HEAD(analyse_object_without_soname, tc)
{
	atf_tc_set_md_var(tc, "require.files", LDSO);
	atf_tc_set_md_var(tc, "descr", "an ET_DYN object with no DT_SONAME provides nothing");
}
ATF_TC_BODY(analyse_object_without_soname, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_analyse_elf(LDSO, &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	tll_free_and_free(required, free);
}

ATF_TC_WITH_CLEANUP(analyse_refuses_symlink);
ATF_TC_HEAD(analyse_refuses_symlink, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "analyse_elf does not follow a symlink itself");
}
ATF_TC_BODY(analyse_refuses_symlink, tc)
{
	char *provided = NULL;
	int flags = -1;
	stringlist_t required = tll_init();

	(void)tc;

	make_test_root();
	ATF_REQUIRE_EQ(0, symlink(LIBZ, test_path("/link.so")));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_analyse_elf(test_path("/link.so"), &provided, &flags, &required));
	ATF_REQUIRE(provided == NULL);
	ATF_REQUIRE_EQ(0, tll_length(required));
}
ATF_TC_CLEANUP(analyse_refuses_symlink, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_symlinks_stay_inside_the_stage);
ATF_TC_HEAD(scan_symlinks_stay_inside_the_stage, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a link to a staged library is read; one pointing at the build host is ignored");
}
ATF_TC_BODY(scan_symlinks_stay_inside_the_stage, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	/* the real library lives in a private directory; the provide path holds
	 * links: one relative into the stage, one absolute as the package would
	 * install it (resolved against the stage root), one to the host */
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/lib/private"), 0755));
	ATF_REQUIRE_EQ(0, rename(lib, test_path("/stage/usr/local/lib/private/libtestz.so.6")));
	ATF_REQUIRE_EQ(
	    0, symlink("private/libtestz.so.6", test_path("/stage/usr/local/lib/libz.so.6")));
	ATF_REQUIRE_EQ(0,
	    symlink("/usr/local/lib/private/libtestz.so.6",
		test_path("/stage/usr/local/lib/libabs.so")));
	ATF_REQUIRE_EQ(0, symlink("/lib/libc.so.7", test_path("/stage/usr/local/lib/libhost.so")));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(scan, test_path("/stage/usr/local/lib/private/libtestz.so.6"),
		"/usr/local/lib/private/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libz.so.6"), "/usr/local/lib/libz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libabs.so"), "/usr/local/lib/libabs.so"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_shlib_scan_file(
		scan, test_path("/stage/usr/local/lib/libhost.so"), "/usr/local/lib/libhost.so"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	/* libz.so.6 is provided through the in-stage links; the host's libc
	 * never entered the picture */
	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_provided));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	ATF_REQUIRE(!list_has(&pack->shlibs_provided, "libc.so.7"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_symlinks_stay_inside_the_stage, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_normalises_installed_path);
ATF_TC_HEAD(scan_normalises_installed_path, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr",
	    "doubled slashes, dot and dot-dot components do not defeat the path filter");
}
ATF_TC_BODY(scan_normalises_installed_path, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;
	static const char *const spellings[] = { "/usr/local//lib/libtestz.so.6",
		"/usr/local/./lib/libtestz.so.6", "/usr/local/lib/../lib/libtestz.so.6",
		"usr/local/lib/libtestz.so.6", NULL };

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	for (const char *const *sp = spellings; *sp != NULL; sp++) {
		scan = mport_shlib_scan_new();
		ATF_REQUIRE(scan != NULL);
		ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, *sp));
		pack = mport_pkgmeta_new();
		ATF_REQUIRE(pack != NULL);
		ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));
		ATF_REQUIRE_MSG(
		    list_has(&pack->shlibs_provided, "libz.so.6"), "not provided for %s", *sp);
		mport_pkgmeta_free(pack);
		mport_shlib_scan_free(scan);
	}

	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_normalises_installed_path, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC(bad_ignore_regex_is_an_error);
ATF_TC_HEAD(bad_ignore_regex_is_an_error, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "a pattern that does not compile stops the scan, not silently");
}
ATF_TC_BODY(bad_ignore_regex_is_an_error, tc)
{
	mportShlibScan *scan;

	(void)tc;

	clear_shlib_env();
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_REGEX", "lib(unclosed", 1));
	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan == NULL);
	ATF_REQUIRE_MSG(strstr(mport_err_string(), "SHLIB_REQUIRE_IGNORE_REGEX") != NULL, "%s",
	    mport_err_string());
	clear_shlib_env();

	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_IGNORE_REGEX", "^libz\\.so, ^libfoo", 1));
	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}

/* ---- the scan over a staged package ----------------------------------- */

/* stage a copy of libz as lib/libtestz.so.6 and gzip as bin/gzip */
static void
stage_lib_and_user(const char **libpath, const char **binpath)
{
	make_test_root();
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/lib"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/stage/usr/local/bin"), 0755));
	copy_file(LIBZ, test_path("/stage/usr/local/lib/libtestz.so.6"));
	copy_file(GZIP, test_path("/stage/usr/local/bin/gzip"));
	*libpath = test_path("/stage/usr/local/lib/libtestz.so.6");
	*binpath = test_path("/stage/usr/local/bin/gzip");
}

ATF_TC_WITH_CLEANUP(scan_provides_and_requires);
ATF_TC_HEAD(scan_provides_and_requires, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a library in a provide path is provided and cancels the package's own need for it");
}
ATF_TC_BODY(scan_provides_and_requires, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(
	    0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib,/usr/local/lib/foo", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_provided));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	/* gzip needs libz, but the package ships it: not a requirement */
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE_EQ(0, pack->no_provide_shlib);

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_provides_and_requires, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_private_library);
ATF_TC_HEAD(scan_private_library, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(tc, "descr",
	    "a library outside the provide paths is private: not provided, still not required");
}
ATF_TC_BODY(scan_private_library, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib64", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(0, tll_length(pack->shlibs_provided));
	ATF_REQUIRE_EQ(1, pack->no_provide_shlib);
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_private_library, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_unset_paths_provide_everything);
ATF_TC_HEAD(scan_unset_paths_provide_everything, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ);
	atf_tc_set_md_var(tc, "descr", "with no provide paths every library is provided");
}
ATF_TC_BODY(scan_unset_paths_provide_everything, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/libexec/odd/libtestz.so.6"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
}
ATF_TC_CLEANUP(scan_unset_paths_provide_everything, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(scan_ignore_lists);
ATF_TC_HEAD(scan_ignore_lists, tc)
{
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(
	    tc, "descr", "glob and regex ignore lists drop provided and required names");
}
ATF_TC_BODY(scan_ignore_lists, tc)
{
	mportShlibScan *scan;
	mportPackageMeta *pack;
	const char *lib, *bin;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_GLOB", "libbz2.so.*, libc.so.*", 1));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_REQUIRE_IGNORE_REGEX", "^liblzma\\.so\\.[0-9]+$", 1));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_IGNORE_REGEX", "^libz\\.so", 1));

	scan = mport_shlib_scan_new();
	ATF_REQUIRE(scan != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, lib, "/usr/local/lib/libtestz.so.6"));
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_file(scan, bin, "/usr/local/bin/gzip"));

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_shlib_scan_finish(scan, pack));

	ATF_REQUIRE_EQ(0, tll_length(pack->shlibs_provided));
	ATF_REQUIRE_EQ(1, pack->no_provide_shlib);
	/* gzip needs libz (shipped), liblzma, libprivatezstd, libbz2 and libc;
	 * the glob drops libbz2 and libc, the regex drops liblzma */
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libbz2.so.4"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "liblzma.so.5"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libprivatezstd.so.5"));
	ATF_REQUIRE_EQ(1, tll_length(pack->shlibs_required));

	mport_pkgmeta_free(pack);
	mport_shlib_scan_free(scan);
	clear_shlib_env();
}
ATF_TC_CLEANUP(scan_ignore_lists, tc)
{
	(void)tc;

	cleanup_test_root();
}

/* ---- the lists reach the package file --------------------------------- */

ATF_TC_WITH_CLEANUP(create_stores_shlib_tables);
ATF_TC_HEAD(create_stores_shlib_tables, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
	atf_tc_set_md_var(tc, "require.files", LIBZ " " GZIP);
	atf_tc_set_md_var(
	    tc, "descr", "mport_create_primative records shlibs_provided and shlibs_required");
}
ATF_TC_BODY(create_stores_shlib_tables, tc)
{
	mportInstance *mport;
	mportAssetList *assetlist;
	mportPackageMeta *pack;
	mportCreateExtras *extra;
	mportBundleRead *bundle;
	const char *lib, *bin;
	FILE *fp;
	int count;

	(void)tc;

	clear_shlib_env();
	stage_lib_and_user(&lib, &bin);
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var"), 0755));
	ATF_REQUIRE_EQ(0, mkdir(test_path("/var/db"), 0755));
	ATF_REQUIRE_EQ(0, setenv("SHLIB_PROVIDE_PATHS_NATIVE", "/usr/local/lib", 1));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, test_root, "root", false, MPORT_VQUIET));

	write_file(test_path("/plist"), "lib/libtestz.so.6\nbin/gzip\n");
	assetlist = mport_assetlist_new();
	ATF_REQUIRE(assetlist != NULL);
	/* cppcheck-suppress-begin nullPointerOutOfResources */
	fp = fopen(test_path("/plist"), "r");
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(0, mport_parse_plistfile(fp, assetlist));
	(void)fclose(fp);
	/* cppcheck-suppress-end nullPointerOutOfResources */

	pack = mport_pkgmeta_new();
	ATF_REQUIRE(pack != NULL);
	pack->name = strdup("shlibpkg");
	pack->version = strdup("1.0");
	pack->prefix = strdup("/usr/local");
	pack->origin = strdup("misc/shlibpkg");
	pack->lang = strdup("");
	pack->comment = strdup("shared library test package");
	pack->type = MPORT_TYPE_APP;

	extra = mport_createextras_new();
	ATF_REQUIRE(extra != NULL);
	(void)strlcpy(
	    extra->pkg_filename, test_path("/shlibpkg-1.0.mport"), sizeof(extra->pkg_filename));
	(void)strlcpy(extra->sourcedir, test_path("/stage"), sizeof(extra->sourcedir));

	ATF_REQUIRE_MSG(mport_create_primative(mport, assetlist, pack, extra) == MPORT_OK, "%s",
	    mport_err_string());

	/* the caller's meta carries the settled lists */
	ATF_REQUIRE(list_has(&pack->shlibs_provided, "libz.so.6"));
	ATF_REQUIRE(list_has(&pack->shlibs_required, "libc.so.7"));
	ATF_REQUIRE(!list_has(&pack->shlibs_required, "libz.so.6"));
	ATF_REQUIRE_EQ(0, pack->no_provide_shlib);

	/* and so does the package file */
	bundle = mport_bundle_read_new();
	ATF_REQUIRE(bundle != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_init(bundle, test_path("/shlibpkg-1.0.mport")));
	ATF_REQUIRE_MSG(mport_bundle_read_prep_for_install(mport, bundle) == MPORT_OK, "%s",
	    mport_err_string());
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_provided WHERE pkg='shlibpkg' AND name='libz.so.6'"));
	ATF_REQUIRE_EQ(1, count);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_required WHERE pkg='shlibpkg' AND name='libc.so.7'"));
	ATF_REQUIRE_EQ(1, count);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_count(mport->db, &count,
		"SELECT count(*) FROM stub.shlibs_required WHERE pkg='shlibpkg' AND name='libz.so.6'"));
	ATF_REQUIRE_EQ(0, count);
	ATF_REQUIRE_EQ(MPORT_OK, mport_bundle_read_finish(mport, bundle));

	mport_assetlist_free(assetlist);
	mport_pkgmeta_free(pack);
	mport_createextras_free(extra);
	mport_instance_free(mport);
	clear_shlib_env();
}
ATF_TC_CLEANUP(create_stores_shlib_tables, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, analyse_shared_library);
	ATF_TP_ADD_TC(tp, analyse_executable);
	ATF_TP_ADD_TC(tp, analyse_plain_file);
	ATF_TP_ADD_TC(tp, analyse_linux_object);
	ATF_TP_ADD_TC(tp, name_with_flags);
	ATF_TP_ADD_TC(tp, analyse_object_without_soname);
	ATF_TP_ADD_TC(tp, analyse_refuses_symlink);
	ATF_TP_ADD_TC(tp, scan_symlinks_stay_inside_the_stage);
	ATF_TP_ADD_TC(tp, scan_normalises_installed_path);
	ATF_TP_ADD_TC(tp, bad_ignore_regex_is_an_error);
	ATF_TP_ADD_TC(tp, scan_provides_and_requires);
	ATF_TP_ADD_TC(tp, scan_private_library);
	ATF_TP_ADD_TC(tp, scan_unset_paths_provide_everything);
	ATF_TP_ADD_TC(tp, scan_ignore_lists);
	ATF_TP_ADD_TC(tp, create_stores_shlib_tables);

	return atf_no_error();
}
