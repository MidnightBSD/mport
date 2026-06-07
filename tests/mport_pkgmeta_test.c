#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <fcntl.h>
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

#define TEST_ROOT "test-pkgmeta-root"
#define SORT_TEST_PACKAGE_COUNT 4

static void
cleanup_test_root(void)
{
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-wal");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-shm");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db");
	(void)unlink(TEST_ROOT "/var/db/mport/infrastructure/alpha-1.2.3/pkg-message");
	(void)rmdir(TEST_ROOT "/var/db/mport/infrastructure/alpha-1.2.3");
	(void)rmdir(TEST_ROOT "/var/db/mport/infrastructure");
	(void)rmdir(TEST_ROOT "/var/db/mport");
	(void)rmdir(TEST_ROOT "/var/db");
	(void)rmdir(TEST_ROOT "/var");
	(void)rmdir(TEST_ROOT);
}

static mportInstance *
create_test_instance(void)
{
	mportInstance *mport;

	cleanup_test_root();
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT, 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var", 0755));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var/db", 0755));

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_instance_init(mport, TEST_ROOT, "root", false, MPORT_VQUIET));

	return mport;
}

static void
insert_package(mportInstance *mport, const char *name)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang) VALUES "
		"(%Q, '1.0', %Q, '/usr/local', '')",
		name, name));
}

static void
insert_query_package(mportInstance *mport, const char *name, const char *version,
    const char *origin, const char *comment, int automatic, int locked, int64_t flatsize)
{
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO packages (pkg, version, origin, prefix, lang, options, comment, "
		"os_release, cpe, locked, deprecated, expiration_date, no_provide_shlib, "
		"flavor, automatic, install_date, type, flatsize) VALUES "
		"(%Q, %Q, %Q, '/usr/local', '', 'DOCS=on EXAMPLES=off', %Q, '4.0', '', "
		"%d, '', 0, 0, '', %d, 1234, 0, %lld)",
		name, version, origin, comment, locked, automatic, (long long)flatsize));
}

static int
sorted_index(mportPackageMeta **sorted, int package_count, const char *name)
{
	int i;

	for (i = 0; i < package_count; i++) {
		if (strcmp(sorted[i]->name, name) == 0)
			return i;
	}

	return -1;
}

static void
require_before(mportPackageMeta **sorted, int package_count, const char *first, const char *second)
{
	int first_index = sorted_index(sorted, package_count, first);
	int second_index = sorted_index(sorted, package_count, second);

	ATF_REQUIRE_MSG(first_index >= 0, "missing package %s from sorted result", first);
	ATF_REQUIRE_MSG(second_index >= 0, "missing package %s from sorted result", second);
	ATF_REQUIRE_MSG(first_index < second_index, "expected %s before %s", first, second);
}

static void
populate_dependency_graph(mportInstance *mport)
{
	insert_package(mport, "a");
	insert_package(mport, "b");
	insert_package(mport, "c");
	insert_package(mport, "d");

	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) VALUES "
		"('a', 'b', '1.0', 'ports/b'), "
		"('a', 'c', '1.0', 'ports/c'), "
		"('b', 'd', '1.0', 'ports/d')"));
}

ATF_TC_WITH_CLEANUP(sort_dependencies_dependency_first);
ATF_TC_HEAD(sort_dependencies_dependency_first, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "dependency-first sort updates lower-level dependencies before dependents");
}
ATF_TC_BODY(sort_dependencies_dependency_first, tc)
{
	mportInstance *mport;
	mportPackageMeta a = { .name = "a" };
	mportPackageMeta b = { .name = "b" };
	mportPackageMeta c = { .name = "c" };
	mportPackageMeta d = { .name = "d" };
	mportPackageMeta *flat[] = { &a, &b, &c, &d };
	mportPackageMeta **sorted;

	(void)tc;

	mport = create_test_instance();
	populate_dependency_graph(mport);

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, SORT_TEST_PACKAGE_COUNT, true);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "d", "b");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "b", "a");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "c", "a");

	free(sorted);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sort_dependencies_dependency_first, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(sort_dependencies_dependent_first);
ATF_TC_HEAD(sort_dependencies_dependent_first, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "dependent-first sort removes dependents before lower-level dependencies");
}
ATF_TC_BODY(sort_dependencies_dependent_first, tc)
{
	mportInstance *mport;
	mportPackageMeta a = { .name = "a" };
	mportPackageMeta b = { .name = "b" };
	mportPackageMeta c = { .name = "c" };
	mportPackageMeta d = { .name = "d" };
	mportPackageMeta *flat[] = { &a, &b, &c, &d };
	mportPackageMeta **sorted;

	(void)tc;

	mport = create_test_instance();
	populate_dependency_graph(mport);

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, SORT_TEST_PACKAGE_COUNT, false);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "a", "b");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "a", "c");
	require_before(sorted, SORT_TEST_PACKAGE_COUNT, "b", "d");

	free(sorted);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sort_dependencies_dependent_first, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(query_formats_installed_metadata);
ATF_TC_HEAD(query_formats_installed_metadata, tc)
{
	atf_tc_set_md_var(tc, "descr", "query formatter prints installed package metadata");
}
ATF_TC_BODY(query_formats_installed_metadata, tc)
{
	mportInstance *mport;
	mportPackageMeta **packs = NULL;
	mportQueryOptions opts;
	char *buf = NULL;
	size_t len = 0;
	FILE *fp;

	(void)tc;

	mport = create_test_instance();
	insert_query_package(mport, "alpha", "1.2.3", "devel/alpha", "Alpha package", 0, 1, 4096);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(
		mport->db, "INSERT INTO categories (pkg, category) VALUES ('alpha', 'devel')"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO depends (pkg, depend_pkgname, depend_pkgversion, depend_port) "
		"VALUES ('alpha', 'beta', '1.0', 'devel/beta')"));
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_db_do(mport->db,
		"INSERT INTO assets (pkg, type, data, checksum, owner, grp, mode) VALUES "
		"('alpha', %d, '/usr/local/bin/alpha', '', '', '', '')",
		ASSET_FILE));
	ATF_REQUIRE_EQ(0, mkdir(TEST_ROOT "/var/db/mport/infrastructure/alpha-1.2.3", 0755));
	int message_fd = open(TEST_ROOT "/var/db/mport/infrastructure/alpha-1.2.3/pkg-message",
	    O_WRONLY | O_CREAT | O_TRUNC, 0644);
	ATF_REQUIRE(message_fd >= 0);
	ssize_t write_result = write(message_fd, "Read alpha notes", strlen("Read alpha notes"));
	int close_result = close(message_fd);
	ATF_REQUIRE_EQ((ssize_t)strlen("Read alpha notes"), write_result);
	ATF_REQUIRE_EQ(0, close_result);
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_annotation_set(mport, "alpha", "maintainer", "ports@MidnightBSD.org"));
	ATF_REQUIRE_EQ(
	    MPORT_OK, mport_annotation_set(mport, "alpha", "www", "https://example.test"));

	memset(&opts, 0, sizeof(opts));
	opts.all = true;
	opts.case_sensitive = false;
	opts.match = MPORT_QUERY_MATCH_EXACT;
	ATF_REQUIRE_EQ(MPORT_OK, mport_query_installed(mport, &opts, &packs));
	ATF_REQUIRE(packs != NULL);

	fp = open_memstream(&buf, &len);
	ATF_REQUIRE(fp != NULL);
	ATF_REQUIRE_EQ(MPORT_OK,
	    mport_query_print(mport, packs, "%n|%v|%o|%m|%w|%c|%k|%s|%C|%d|%F|%#C|%M", fp));
	close_result = fclose(fp);
	fp = NULL;
	ATF_REQUIRE_EQ(0, close_result);

	ATF_REQUIRE_STREQ("alpha|1.2.3|devel/alpha|ports@MidnightBSD.org|"
			  "https://example.test|Alpha package|1|4096|devel|beta|"
			  "/usr/local/bin/alpha|1|Read alpha notes\n",
	    buf);

	free(buf);
	mport_pkgmeta_vec_free(packs);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(query_formats_installed_metadata, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TC_WITH_CLEANUP(query_filters_patterns_and_expressions);
ATF_TC_HEAD(query_filters_patterns_and_expressions, tc)
{
	atf_tc_set_md_var(tc, "descr", "query filters by pattern and scalar expression");
}
ATF_TC_BODY(query_filters_patterns_and_expressions, tc)
{
	mportInstance *mport;
	mportPackageMeta **packs = NULL;
	mportQueryOptions opts;
	char *patterns[] = { "*" };

	(void)tc;

	mport = create_test_instance();
	insert_query_package(mport, "alpha", "1.2.3", "devel/alpha", "Alpha package", 0, 0, 4096);
	insert_query_package(mport, "beta", "0.9", "devel/beta", "Beta package", 1, 0, 1024);

	memset(&opts, 0, sizeof(opts));
	opts.case_sensitive = false;
	opts.match = MPORT_QUERY_MATCH_GLOB;
	opts.patterns = patterns;
	opts.pattern_count = 1;
	opts.expression = "%n='alpha'&&%v>=1.0||%n='missing'";

	ATF_REQUIRE_EQ(MPORT_OK, mport_query_installed(mport, &opts, &packs));
	ATF_REQUIRE(packs != NULL);
	ATF_REQUIRE(packs[0] != NULL);
	ATF_REQUIRE_STREQ("alpha", packs[0]->name);
	ATF_REQUIRE(packs[1] == NULL);

	mport_pkgmeta_vec_free(packs);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(query_filters_patterns_and_expressions, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sort_dependencies_dependency_first);
	ATF_TP_ADD_TC(tp, sort_dependencies_dependent_first);
	ATF_TP_ADD_TC(tp, query_formats_installed_metadata);
	ATF_TP_ADD_TC(tp, query_filters_patterns_and_expressions);

	return atf_no_error();
}
