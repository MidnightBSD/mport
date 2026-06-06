#define __amd64__ 1
#define __linux__ 1
#define __MidnightBSD_version 300000

#include <sys/cdefs.h>
#include <sys/param.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

ATF_TC_WITHOUT_HEAD(count_spaces_empty);
ATF_TC_BODY(count_spaces_empty, tc)
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
	ATF_TP_ADD_TC(tp, count_spaces_empty);
	ATF_TP_ADD_TC(tp, count_spaces_none);
	ATF_TP_ADD_TC(tp, count_spaces_only);
	ATF_TP_ADD_TC(tp, count_spaces_mixed);

	return atf_no_error();
}
