#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <atf-c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libmport/mport.h"
#include "../libmport/mport_private.h"

ATF_TC_WITH_CLEANUP(osrelease_from_settings);
ATF_TC_HEAD(osrelease_from_settings, tc)
{
	atf_tc_set_md_var(tc, "descr", "mport_get_osrelease honors MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_from_settings, tc)
{
	mportInstance *mport;
	char *version;

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_init(mport, NULL, "root", false, false));

	ATF_REQUIRE_EQ(MPORT_OK, mport_setting_set(mport, MPORT_SETTING_TARGET_OS, "9.9-TEST"));

	version = mport_get_osrelease(mport);
	ATF_REQUIRE(version != NULL);
	ATF_REQUIRE_STREQ("9.9-TEST", version);

	free(version);
	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_from_settings, tc)
{
	(void)tc;
}

ATF_TC_WITH_CLEANUP(osrelease_null_instance);
ATF_TC_HEAD(osrelease_null_instance, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles NULL instance (falls back to system tools)");
}
ATF_TC_BODY(osrelease_null_instance, tc)
{
	char *version;

	/* On a real MidnightBSD system this will return the OS release,
	 * on Linux/others it might return NULL.
	 * We just ensure it doesn't crash.
	 */
	version = mport_get_osrelease(NULL);
	if (version != NULL) {
		free(version);
	}
}
ATF_TC_CLEANUP(osrelease_null_instance, tc)
{
	(void)tc;
}

ATF_TC_WITH_CLEANUP(osrelease_settings_null);
ATF_TC_HEAD(osrelease_settings_null, tc)
{
	atf_tc_set_md_var(
	    tc, "descr", "mport_get_osrelease handles missing MPORT_SETTING_TARGET_OS");
}
ATF_TC_BODY(osrelease_settings_null, tc)
{
	mportInstance *mport;
	char *version;

	mport = mport_instance_new();
	ATF_REQUIRE(mport != NULL);
	ATF_REQUIRE_EQ(MPORT_OK, mport_instance_init(mport, NULL, "root", false, false));

	// make sure the setting does not exist or we clear it
	mport_db_do(mport->db, "DELETE FROM settings WHERE name=%Q", MPORT_SETTING_TARGET_OS);

	version = mport_get_osrelease(mport);
	// this shouldn't crash, and will try system methods
	if (version != NULL) {
		free(version);
	}

	mport_instance_free(mport);
}
ATF_TC_CLEANUP(osrelease_settings_null, tc)
{
	(void)tc;
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, osrelease_from_settings);
	ATF_TP_ADD_TC(tp, osrelease_null_instance);
	ATF_TP_ADD_TC(tp, osrelease_settings_null);

	return atf_no_error();
}
