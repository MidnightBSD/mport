#include <sys/cdefs.h>

#include <atf-c.h>
#include <stdlib.h>
#include <string.h>

/* Splint does not understand ATF's generated test-case wrappers. */
/*@-fullinitblock -mustfreefresh -noeffect -nullpass -retvalother -unrecog@*/

char *mport_fetch_force_http_url(const char *);

static void
check_effective_url(const char *input, const char *expected)
{
	char *actual;

	actual = mport_fetch_force_http_url(input);
	ATF_REQUIRE(actual != NULL);
	ATF_REQUIRE_STREQ(expected, actual);
	free(actual);
}

ATF_TC_WITH_CLEANUP(force_http_unset_keeps_https);
ATF_TC_HEAD(force_http_unset_keeps_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "HTTPS URLs are unchanged unless MPORT_FORCE_HTTP is set");
}
ATF_TC_BODY(force_http_unset_keeps_https, tc)
{
	(void)tc;

	unsetenv("MPORT_FORCE_HTTP");
	check_effective_url("https://example.invalid/amd64/4.0/pkg.mport",
	    "https://example.invalid/amd64/4.0/pkg.mport");
}
ATF_TC_CLEANUP(force_http_unset_keeps_https, tc)
{
	(void)tc;

	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_empty_keeps_https);
ATF_TC_HEAD(force_http_empty_keeps_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "Empty MPORT_FORCE_HTTP values do not force HTTP");
}
ATF_TC_BODY(force_http_empty_keeps_https, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, setenv("MPORT_FORCE_HTTP", "", 1));
	check_effective_url("https://example.invalid/amd64/4.0/pkg.mport",
	    "https://example.invalid/amd64/4.0/pkg.mport");
}
ATF_TC_CLEANUP(force_http_empty_keeps_https, tc)
{
	(void)tc;

	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_set_rewrites_https);
ATF_TC_HEAD(force_http_set_rewrites_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "MPORT_FORCE_HTTP rewrites HTTPS package URLs to HTTP");
}
ATF_TC_BODY(force_http_set_rewrites_https, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, setenv("MPORT_FORCE_HTTP", "1", 1));
	check_effective_url("https://example.invalid/amd64/4.0/pkg.mport",
	    "http://example.invalid/amd64/4.0/pkg.mport");
}
ATF_TC_CLEANUP(force_http_set_rewrites_https, tc)
{
	(void)tc;

	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC_WITH_CLEANUP(force_http_set_keeps_non_https);
ATF_TC_HEAD(force_http_set_keeps_non_https, tc)
{
	atf_tc_set_md_var(tc, "descr", "MPORT_FORCE_HTTP leaves non-HTTPS URLs unchanged");
}
ATF_TC_BODY(force_http_set_keeps_non_https, tc)
{
	(void)tc;

	ATF_REQUIRE_EQ(0, setenv("MPORT_FORCE_HTTP", "1", 1));
	check_effective_url("http://example.invalid/amd64/4.0/pkg.mport",
	    "http://example.invalid/amd64/4.0/pkg.mport");
}
ATF_TC_CLEANUP(force_http_set_keeps_non_https, tc)
{
	(void)tc;

	unsetenv("MPORT_FORCE_HTTP");
}

ATF_TC(force_http_null_is_null);
ATF_TC_HEAD(force_http_null_is_null, tc)
{
	atf_tc_set_md_var(tc, "descr", "NULL URLs remain NULL");
}
ATF_TC_BODY(force_http_null_is_null, tc)
{
	(void)tc;

	ATF_REQUIRE(mport_fetch_force_http_url(NULL) == NULL);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, force_http_unset_keeps_https);
	ATF_TP_ADD_TC(tp, force_http_empty_keeps_https);
	ATF_TP_ADD_TC(tp, force_http_set_rewrites_https);
	ATF_TP_ADD_TC(tp, force_http_set_keeps_non_https);
	ATF_TP_ADD_TC(tp, force_http_null_is_null);

	return atf_no_error();
}
