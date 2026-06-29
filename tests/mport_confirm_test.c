#include <sys/cdefs.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../libmport/mport.h"

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-boundsread -boundswrite -compdef -compdestroy -dependenttrans -fullinitblock@*/
/*@-mustfreefresh -noeffect -nullderef -nullpass -nullret -nullstate@*/
/*@-retvalint -retvalother -type -unrecog@*/

/*
 * Force stdin to a non-terminal so the confirmation/selection callbacks take
 * their non-interactive path deterministically (ATF runs each test case in its
 * own process, so reopening stdin here does not leak into other cases).
 */
static void
make_stdin_non_tty(void)
{
	ATF_REQUIRE(freopen("/dev/null", "r", stdin) != NULL);
	ATF_REQUIRE(!isatty(fileno(stdin)));
}

ATF_TC(confirm_assume_always_yes);
ATF_TC_HEAD(confirm_assume_always_yes, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_confirm_cb returns MPORT_OK when ASSUME_ALWAYS_YES is set");
}
ATF_TC_BODY(confirm_assume_always_yes, tc)
{
	(void)tc;

	(void)unsetenv("MAGUS");
	ATF_REQUIRE_EQ(0, setenv("ASSUME_ALWAYS_YES", "1", 1));
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("ASSUME_ALWAYS_YES");
}

ATF_TC(confirm_magus);
ATF_TC_HEAD(confirm_magus, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_confirm_cb returns MPORT_OK when MAGUS is set");
}
ATF_TC_BODY(confirm_magus, tc)
{
	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	ATF_REQUIRE_EQ(0, setenv("MAGUS", "1", 1));
	ATF_REQUIRE_EQ(MPORT_OK, mport_default_confirm_cb("Proceed?", "Yes", "No", 0));
	(void)unsetenv("MAGUS");
}

ATF_TC(confirm_non_tty_aborts);
ATF_TC_HEAD(confirm_non_tty_aborts, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "mport_default_confirm_cb refuses (non-OK) on a non-terminal without assume-yes");
}
ATF_TC_BODY(confirm_non_tty_aborts, tc)
{
	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	(void)unsetenv("MAGUS");
	make_stdin_non_tty();

	/* Must not block on input and must not report confirmation. */
	ATF_REQUIRE(mport_default_confirm_cb("Proceed?", "Yes", "No", 0) != MPORT_OK);
	/* Default of 1 ("yes") must not flip a non-terminal into a confirmation. */
	ATF_REQUIRE(mport_default_confirm_cb("Proceed?", "Yes", "No", 1) != MPORT_OK);
}

ATF_TC(select_non_tty_aborts);
ATF_TC_HEAD(select_non_tty_aborts, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_default_select_cb returns -1 on a non-terminal without assume-yes");
}
ATF_TC_BODY(select_non_tty_aborts, tc)
{
	/*
	 * On the non-terminal path the callback returns before any choice is
	 * dereferenced, so a single non-NULL sentinel terminated by NULL is
	 * enough to get past the empty-list guard.
	 */
	mportIndexEntry *choices[] = { (mportIndexEntry *)(void *)&choices, NULL };

	(void)tc;

	(void)unsetenv("ASSUME_ALWAYS_YES");
	(void)unsetenv("MAGUS");
	make_stdin_non_tty();

	ATF_REQUIRE_EQ(-1, mport_default_select_cb("Pick one", choices, 0));
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, confirm_assume_always_yes);
	ATF_TP_ADD_TC(tp, confirm_magus);
	ATF_TP_ADD_TC(tp, confirm_non_tty_aborts);
	ATF_TP_ADD_TC(tp, select_non_tty_aborts);

	return atf_no_error();
}
