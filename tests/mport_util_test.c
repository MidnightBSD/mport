#include <sys/cdefs.h>
#include <sys/param.h>

#include <atf-c.h>
#include <stdbool.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

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
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_null);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_true);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_false);
	ATF_TP_ADD_TC(tp, mport_check_answer_bool_invalid);

	return atf_no_error();
}
