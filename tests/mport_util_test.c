#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../libmport/mport.h"

int mport_rmdir(const char *, int);

ATF_TC_WITH_CLEANUP(mport_rmdir_empty);
ATF_TC_HEAD(mport_rmdir_empty, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_rmdir successfully removes an empty directory");
}
ATF_TC_BODY(mport_rmdir_empty, tc)
{
	const char *test_dir = "test_empty_dir";

	ATF_REQUIRE(mkdir(test_dir, 0755) == 0);
	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 0));
	ATF_REQUIRE(access(test_dir, F_OK) != 0);
}
ATF_TC_CLEANUP(mport_rmdir_empty, tc)
{
	rmdir("test_empty_dir");
}

ATF_TC_WITH_CLEANUP(mport_rmdir_nonempty_ignore);
ATF_TC_HEAD(mport_rmdir_nonempty_ignore, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_rmdir handles non-empty directory with ignore_nonempty=1");
}
ATF_TC_BODY(mport_rmdir_nonempty_ignore, tc)
{
	const char *test_dir = "test_nonempty_dir_ignore";
	const char *test_file = "test_nonempty_dir_ignore/file.txt";

	ATF_REQUIRE(mkdir(test_dir, 0755) == 0);
	int fd = open(test_file, O_CREAT | O_WRONLY, 0644);
	ATF_REQUIRE(fd != -1);
	close(fd);

	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 1));
	ATF_REQUIRE(access(test_dir, F_OK) == 0); /* Directory should still exist */

	unlink(test_file);
}
ATF_TC_CLEANUP(mport_rmdir_nonempty_ignore, tc)
{
	unlink("test_nonempty_dir_ignore/file.txt");
	rmdir("test_nonempty_dir_ignore");
}

ATF_TC_WITH_CLEANUP(mport_rmdir_nonempty_noignore);
ATF_TC_HEAD(mport_rmdir_nonempty_noignore, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_rmdir fails on non-empty directory with ignore_nonempty=0");
}
ATF_TC_BODY(mport_rmdir_nonempty_noignore, tc)
{
	const char *test_dir = "test_nonempty_dir_noignore";
	const char *test_file = "test_nonempty_dir_noignore/file.txt";

	ATF_REQUIRE(mkdir(test_dir, 0755) == 0);
	int fd = open(test_file, O_CREAT | O_WRONLY, 0644);
	ATF_REQUIRE(fd != -1);
	close(fd);

	ATF_REQUIRE(mport_rmdir(test_dir, 0) != MPORT_OK);

	unlink(test_file);
}
ATF_TC_CLEANUP(mport_rmdir_nonempty_noignore, tc)
{
	unlink("test_nonempty_dir_noignore/file.txt");
	rmdir("test_nonempty_dir_noignore");
}

ATF_TC_WITH_CLEANUP(mport_rmdir_notfound_ignore);
ATF_TC_HEAD(mport_rmdir_notfound_ignore, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_rmdir handles non-existent directory with ignore_nonempty=1");
}
ATF_TC_BODY(mport_rmdir_notfound_ignore, tc)
{
	const char *test_dir = "test_notfound_dir";

	ATF_REQUIRE(access(test_dir, F_OK) != 0);
	ATF_REQUIRE_EQ(MPORT_OK, mport_rmdir(test_dir, 1));
}
ATF_TC_CLEANUP(mport_rmdir_notfound_ignore, tc)
{
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_rmdir_empty);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_ignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_noignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_notfound_ignore);

	return atf_no_error();
}
