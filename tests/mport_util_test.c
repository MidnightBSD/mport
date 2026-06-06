#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

extern int mport_count_spaces(const char *);

int mport_rmdir(const char *, int);


ATF_TC_WITH_CLEANUP(mport_rmdir_empty);
ATF_TC_HEAD(mport_rmdir_empty, tc)

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


ATF_TC_WITHOUT_HEAD(count_spaces_empty);
ATF_TC_BODY(count_spaces_empty, tc)

bool mport_check_answer_bool(char *ans);

ATF_TC(mport_check_answer_bool_null);
ATF_TC_HEAD(mport_check_answer_bool_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_check_answer_bool with NULL");
}
ATF_TC_BODY(mport_check_answer_bool_null, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(false, mport_check_answer_bool(NULL));
}

ATF_TC(mport_check_answer_bool_true);
ATF_TC_HEAD(mport_check_answer_bool_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_check_answer_bool with true values");
}
ATF_TC_BODY(mport_check_answer_bool_true, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("Y"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("y"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("T"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("t"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("1"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("Yes")); /* Only checks first char */

	/* Leading/trailing whitespace around answers */
	ATF_REQUIRE_EQ(true, mport_check_answer_bool(" Y"));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("Y "));
	ATF_REQUIRE_EQ(true, mport_check_answer_bool("  Y  "));
}

ATF_TC(mport_check_answer_bool_false);
ATF_TC_HEAD(mport_check_answer_bool_false, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_check_answer_bool with false values");
}
ATF_TC_BODY(mport_check_answer_bool_false, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("N"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("n"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("F"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("f"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("0"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("No")); /* Only checks first char */
}

ATF_TC(mport_check_answer_bool_invalid);
ATF_TC_HEAD(mport_check_answer_bool_invalid, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test mport_check_answer_bool with invalid values");
}
ATF_TC_BODY(mport_check_answer_bool_invalid, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(false, mport_check_answer_bool(""));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("X"));
	ATF_REQUIRE_EQ(false, mport_check_answer_bool("hello"));
static void
create_file(const char *filename, const void *data, size_t len)

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

	(void)tc;
	ATF_REQUIRE_EQ(0, mport_count_spaces(""));
}

ATF_TC_WITHOUT_HEAD(count_spaces_none);
ATF_TC_BODY(count_spaces_none, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(0, mport_count_spaces("hello"));
	ATF_REQUIRE_EQ(0, mport_count_spaces("hello_world-123"));
}

ATF_TC_WITHOUT_HEAD(count_spaces_only);
ATF_TC_BODY(count_spaces_only, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(1, mport_count_spaces(" "));
	ATF_REQUIRE_EQ(3, mport_count_spaces("   "));
	ATF_REQUIRE_EQ(2, mport_count_spaces("\t\n"));
}

ATF_TC_WITHOUT_HEAD(count_spaces_mixed);
ATF_TC_BODY(count_spaces_mixed, tc)
{
	(void)tc;
	ATF_REQUIRE_EQ(1, mport_count_spaces("hello world"));
	ATF_REQUIRE_EQ(2, mport_count_spaces("hello  world"));
	ATF_REQUIRE_EQ(3, mport_count_spaces(" hello world "));
	ATF_REQUIRE_EQ(5, mport_count_spaces(" \thello \nworld "));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_rmdir_empty);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_ignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_noignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_notfound_ignore);
	ATF_TP_ADD_TC(tp, mport_mkdir_success);
	ATF_TP_ADD_TC(tp, mport_mkdir_existing);
	ATF_TP_ADD_TC(tp, count_spaces_empty);
	ATF_TP_ADD_TC(tp, count_spaces_none);
	ATF_TP_ADD_TC(tp, count_spaces_only);
	ATF_TP_ADD_TC(tp, count_spaces_mixed);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_null);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_true);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_false);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_invalid);
	ATF_TP_ADD_TC(tp, is_elf_file_true);
	ATF_TP_ADD_TC(tp, is_elf_file_false_text);
	ATF_TP_ADD_TC(tp, is_elf_file_false_small);
	ATF_TP_ADD_TC(tp, is_elf_file_false_missing);
	ATF_TP_ADD_TC(tp, mport_file_exists_existing_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_nonexistent_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_directory);

	return atf_no_error();
}
