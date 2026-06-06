#include <sys/cdefs.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullpass -nullret -nullstate -paramuse@*/
/*@-retvalint -retvalother -type -unrecog@*/

#define TEST_ROOT "test-pkgmeta-root"

static void
cleanup_test_root(void)
{
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-wal");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db-shm");
	(void)unlink(TEST_ROOT "/var/db/mport/master.db");
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
require_before(mportPackageMeta **sorted, const char *first, const char *second)
{
	int first_index = sorted_index(sorted, 4, first);
	int second_index = sorted_index(sorted, 4, second);

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

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, 4, true);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, "d", "b");
	require_before(sorted, "b", "a");
	require_before(sorted, "c", "a");

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

	sorted = mport_pkgmeta_sort_dependencies(mport, flat, 4, false);
	ATF_REQUIRE(sorted != NULL);

	require_before(sorted, "a", "b");
	require_before(sorted, "a", "c");
	require_before(sorted, "b", "d");

	free(sorted);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(sort_dependencies_dependent_first, tc)
{
	(void)tc;

	cleanup_test_root();
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, sort_dependencies_dependency_first);
	ATF_TP_ADD_TC(tp, sort_dependencies_dependent_first);

	return atf_no_error();
}
