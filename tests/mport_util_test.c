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
#include "../libmport/mport_private.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

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

	ATF_REQUIRE_EQ(MPORT_ERR_FATAL, mport_rmdir(test_dir, 0));

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

static void
create_file(const char *filename, const void *data, size_t len)
{
	FILE *f = fopen(filename, "wb");
	ATF_REQUIRE(f != NULL);
	if (len > 0) {
		ATF_REQUIRE_EQ(len, fwrite(data, 1, len, f));
	}
	fclose(f);
}

ATF_TC_WITH_CLEANUP(is_elf_file_true);
ATF_TC_HEAD(is_elf_file_true, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_is_elf_file returns true for an ELF file");
}
ATF_TC_BODY(is_elf_file_true, tc)
{
	const char *filename = "test_elf_file";
	const char magic[] = "\x7F"
			     "ELF"
			     "some other data";
	create_file(filename, magic, sizeof(magic) - 1); // exclude null terminator

	ATF_CHECK(mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_true, tc)
{
	unlink("test_elf_file");
}

ATF_TC_WITH_CLEANUP(is_elf_file_false_text);
ATF_TC_HEAD(is_elf_file_false_text, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_is_elf_file returns false for a plain text file");
}
ATF_TC_BODY(is_elf_file_false_text, tc)
{
	const char *filename = "test_text_file.txt";
	const char content[] = "Hello World!";
	create_file(filename, content, sizeof(content) - 1);

	ATF_CHECK(!mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_false_text, tc)
{
	unlink("test_text_file.txt");
}

ATF_TC_WITH_CLEANUP(is_elf_file_false_small);
ATF_TC_HEAD(is_elf_file_false_small, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_is_elf_file returns false for a file smaller than ELF magic");
}
ATF_TC_BODY(is_elf_file_false_small, tc)
{
	const char *filename = "test_small_file";
	const char content[] = "ELF"; // Only 3 bytes
	create_file(filename, content, sizeof(content) - 1);

	ATF_CHECK(!mport_is_elf_file(filename));
}
ATF_TC_CLEANUP(is_elf_file_false_small, tc)
{
	unlink("test_small_file");
}

ATF_TC_WITH_CLEANUP(is_elf_file_false_missing);
ATF_TC_HEAD(is_elf_file_false_missing, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_is_elf_file returns false for a missing file");
}
ATF_TC_BODY(is_elf_file_false_missing, tc)
{
	ATF_CHECK(!mport_is_elf_file("nonexistent_file_that_should_not_exist"));
}
ATF_TC_CLEANUP(is_elf_file_false_missing, tc)
{
ATF_TC_WITHOUT_HEAD(mport_file_exists_existing_file);
ATF_TC_BODY(mport_file_exists_existing_file, tc)
{
	(void)tc;

	/* Test with a file that is guaranteed to exist */
	ATF_REQUIRE_EQ(1, mport_file_exists("/etc/passwd"));
}

ATF_TC_WITHOUT_HEAD(mport_file_exists_nonexistent_file);
ATF_TC_BODY(mport_file_exists_nonexistent_file, tc)
{
	(void)tc;

	/* Test with a file that is guaranteed not to exist */
	ATF_REQUIRE_EQ(0, mport_file_exists("/this/file/does/not/exist/ever/12345"));
}

ATF_TC_WITHOUT_HEAD(mport_file_exists_directory);
ATF_TC_BODY(mport_file_exists_directory, tc)
{
	(void)tc;

	/* Test with a directory that is guaranteed to exist */
	ATF_REQUIRE_EQ(1, mport_file_exists("/etc"));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_rmdir_empty);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_ignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_nonempty_noignore);
	ATF_TP_ADD_TC(tp, mport_rmdir_notfound_ignore);
	ATF_TP_ADD_TC(tp, is_elf_file_true);
	ATF_TP_ADD_TC(tp, is_elf_file_false_text);
	ATF_TP_ADD_TC(tp, is_elf_file_false_small);
	ATF_TP_ADD_TC(tp, is_elf_file_false_missing);
	ATF_TP_ADD_TC(tp, mport_file_exists_existing_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_nonexistent_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_directory);

	return atf_no_error();
}
