#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

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
	ATF_TP_ADD_TC(tp, is_elf_file_true);
	ATF_TP_ADD_TC(tp, is_elf_file_false_text);
	ATF_TP_ADD_TC(tp, is_elf_file_false_small);
	ATF_TP_ADD_TC(tp, is_elf_file_false_missing);
	ATF_TP_ADD_TC(tp, mport_file_exists_existing_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_nonexistent_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_directory);

	return atf_no_error();
}
