/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <libutil.h>
#include "mport.h"
#include "mport_private.h"

/*
 * Does an age bracket from agev_get_age_bracket(3), { lowest, highest } with
 * -1 for "no limit" or "unknown", meet a package's required age?  18,-1 is
 * 18 or older and meets any requirement: adults are never refused.  Other
 * brackets pass when the requirement is no higher than the top of the
 * bracket, so an unknown age (-1,-1) never does.
 */
bool
mport_age_bracket_allows(const int *bracket, int required_age)
{

	if (bracket[0] >= 18 && bracket[1] == -1)
		return true;

	return required_age <= bracket[1];
}

bool
mport_is_age_verified(mportInstance *mport, mportPackageMeta *pack)
{

#if defined(__MidnightBSD__) && __MidnightBSD_version >= 400004
	char *age_str = NULL;
	int age = 0;

	/* pack is a stub entry: its annotations are still in the bundle */
	(void)pack;
	if (mport_stub_annotation_get(mport, "age", &age_str) != MPORT_OK) {
		mport_call_msg_cb(
		    mport, "Could not read the age restriction: %s", mport_err_string());
		return false;
	}

	if (age_str == NULL) {
		return true; /* No age annotation means it's appropriate for all ages */
	}

	age = atoi(age_str);
	free(age_str);

	const char *username = getlogin();
	if (username == NULL) {
		mport_call_msg_cb(
		    mport, "Could not determine username to verify age: %s", strerror(errno));
		return false;
	}

	int *user_age_bracket = agev_get_age_bracket(username);
	if (user_age_bracket == NULL) {
		mport_call_msg_cb(mport, "Could not determine user age bracket.");
		return false;
	}

	bool verified = mport_age_bracket_allows(user_age_bracket, age);

	free(user_age_bracket);

	return verified;
#else
	return true;
#endif
}
