#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

#define TEST_DIR "test_mport_mkdir_dir"

ATF_TC_WITH_CLEANUP(mport_mkdir_success);
ATF_TC_HEAD(mport_mkdir_success, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_mkdir creates a directory successfully with mode 755");
}
ATF_TC_BODY(mport_mkdir_success, tc)
{
	struct stat sb;
	mode_t old_umask;

	(void)tc;

	/* Ensure the directory does not exist before starting */
	(void)rmdir(TEST_DIR);

	/* Set a known umask to verify permissions accurately */
	old_umask = umask(0022);

	/* Call mport_mkdir */
	ATF_REQUIRE_EQ(MPORT_OK, mport_mkdir(TEST_DIR));

	/* Verify the directory was created */
	ATF_REQUIRE_EQ(0, stat(TEST_DIR, &sb));

	/* Verify the directory is actually a directory */
	ATF_REQUIRE(S_ISDIR(sb.st_mode));

	/* Verify the mode is 755 - umask(022) = 755 */
	ATF_REQUIRE_EQ(0755, sb.st_mode & 0777);

	/* Restore umask */
	umask(old_umask);
}
ATF_TC_CLEANUP(mport_mkdir_success, tc)
{
	(void)tc;
	(void)rmdir(TEST_DIR);
}

ATF_TC_WITH_CLEANUP(mport_mkdir_existing);
ATF_TC_HEAD(mport_mkdir_existing, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_mkdir returns MPORT_OK if directory already exists");
}
ATF_TC_BODY(mport_mkdir_existing, tc)
{
	struct stat sb;

	(void)tc;

	/* Ensure directory doesn't exist, then create it */
	(void)rmdir(TEST_DIR);
	ATF_REQUIRE_EQ(0, mkdir(TEST_DIR, 0755));

	/* Call mport_mkdir on the existing directory */
	ATF_REQUIRE_EQ(MPORT_OK, mport_mkdir(TEST_DIR));

	/* Verify it still exists */
	ATF_REQUIRE_EQ(0, stat(TEST_DIR, &sb));
	ATF_REQUIRE(S_ISDIR(sb.st_mode));
}
ATF_TC_CLEANUP(mport_mkdir_existing, tc)
{
	(void)tc;
	(void)rmdir(TEST_DIR);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_mkdir_success);
	ATF_TP_ADD_TC(tp, mport_mkdir_existing);

	return atf_no_error();
}
