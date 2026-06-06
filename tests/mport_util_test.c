#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

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
	ATF_TP_ADD_TC(tp, mport_file_exists_existing_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_nonexistent_file);
	ATF_TP_ADD_TC(tp, mport_file_exists_directory);

	return atf_no_error();
}
