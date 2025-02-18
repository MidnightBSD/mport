/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007-2009 Chris Reinhardt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>
#include "mport.h"
#include "mport_private.h"

struct version {
	char *version;
	int revision;
	int epoch;
};

static void parse_version(const char *, struct version *);
static int cmp_versions(char *, char *);
static int cmp_ints(int, int);

/* mport_version_cmp(version1, version2)
 *
 * Compare two given version strings.  Returns 0 if the versions
 * are the same, -1 if version1 is less than version2, 1 otherwise.
 */
MPORT_PUBLIC_API int
mport_version_cmp(const char *astr, const char *bstr)
{
	struct version a;
	struct version b;
	int result;
  
	parse_version(astr, &a);
	parse_version(bstr, &b);

	/* remember that a.version/b.version are useless after calling
	   cmp_versions (but astr and bstr are unchanged.) */
	if ((result = cmp_ints(a.epoch, b.epoch)) == 0) {
		if ((result = cmp_versions(a.version, b.version)) == 0) {
			result = cmp_ints(a.revision, b.revision);
		}
	}

#ifdef DEBUG
	printf("Version a %s, Version b %s, result %d\n", a.version, b.version, result);
#endif

	free(a.version);
	a.version = NULL;
	free(b.version);
	b.version = NULL;
  
	return (result);
}


/* version of mport_version_cmp() that is bound to the sqlite3 database. */
void
mport_version_cmp_sqlite(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    char *a = NULL;
	char *b = NULL;

    assert(argc == 2);

    a = strdup(sqlite3_value_text(argv[0]));
    b = strdup(sqlite3_value_text(argv[1]));

    assert(a != NULL);
    assert(b != NULL);

    sqlite3_result_int(context, mport_version_cmp(a, b));

    free(a);
	a = NULL;
    free(b);
	b = NULL;
}  


/* Returns 0 if baseline meets the given requirement, -1 if the requirement
 * was not met, and a value greater than 0 on error.  some examples:
 *
 * mport_version_require_check("0.2.1", ">=2.0") == 0
 * mport_version_require_check("4.1.2", ">5.1")  == -1
 * mport_version_require_check("3.1.4", "|")     > 0
 * multi example:
 * mport_version_require_check("0.2.1", ">=1.4.0<1.5")
 */
int
mport_version_require_check(const char *baseline, const char *require)
{
    int ret = 0;

    bool multi = false;
    int greater[2] = {-1,-1} ;
    int less[2] = {-1,-1};
    int equal[2] = {-1,-1};
    size_t version_size = strlen(require);

    if (version_size < 2) {
    	return 1; // impossible to validate
    }

	for (size_t i = 0; i < version_size; i++) {
		if (require[i] == '>') {
			if (greater[0] == -1) {
				greater[0] = i;
			} else {
				greater[1] = i;
			}
		} else if (require[i] == '<') {
			if (less[0] == -1) {
				less[0] = i;
			} else {
				less[1] = i;
			}
		} else if (require[i] == '=') {
			if (equal[0] == -1) {
				equal[0] = i;
			} else {
				equal[1] = i;
			}
		}
	}

#ifdef DEBUG
	printf(" g0 %d g1 %d l0 %d l1 %d eq0 %d\n", greater[0], greater[1], less[0], less[1], equal[0]);
#endif

	if (greater[0] == -1 && less[0] == -1 && equal[0] == -1) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Malformed version requirement: %s", require);
	}

	if (greater[1] > -1 || less[1] > -1 || equal[1] > -1 || (greater[0] > -1 && less[0] > -1)) {
		multi = true;
#ifdef DEBUG
		printf("multi enabled\n");
#endif
	}

	if (!multi) {
		if (require[0] == '<') {
			if (require[1] == '=') {
				ret = (mport_version_cmp(baseline, &require[2]) <= 0) ? 0 : -1;
			} else {
				ret = (mport_version_cmp(baseline, &require[1]) < 0) ? 0 : -1;
			}
		} else if (require[0] == '>') {
			if (require[1] == '=') {
				ret = (mport_version_cmp(baseline, &require[2]) >= 0) ? 0 : -1;
			} else {
				ret = (mport_version_cmp(baseline, &require[1]) > 0) ? 0 : -1;
			}
		} else {
			RETURN_ERRORX(MPORT_ERR_FATAL, "Malformed version requirement: %s", require);
		}
	} else {
		char *s = strdup(require);
		if (s == NULL) {
			RETURN_ERROR(MPORT_ERR_FATAL, "Memory allocation failed");
		}

		if (less[1] > 0) {
			// second one is less than e.g. <1.5
			s[less[1]] = '\0';
			if (s[less[1] + 1] == '=') {
				ret = (mport_version_cmp(baseline, &s[less[1] + 2]) <= 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			} else {
				ret = (mport_version_cmp(baseline, &s[less[1] + 1]) < 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			}
		} else if (less[0] > greater[0]) {
			// second one is less than e.g. <1.5
			s[less[0]] = '\0';
			if (s[less[0] + 1] == '=') {
				ret = (mport_version_cmp(baseline, &s[less[0] + 2]) <= 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			} else {
				ret = (mport_version_cmp(baseline, &s[less[0] + 1]) < 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			}
		} else if (greater[1] > 0) {
			// second one is greater
			s[greater[1]] = '\0';
			if (s[greater[1] + 1] == '=') {
				ret = (mport_version_cmp(baseline, &s[greater[1] + 2]) >= 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			} else {
				ret = (mport_version_cmp(baseline, &s[greater[1] + 1]) > 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			}
		} else if (greater[0] > less[0]) {
			// second one is greater
			s[greater[0]] = '\0';
			if (s[greater[0] + 1] == '=') {
				ret = (mport_version_cmp(baseline, &s[greater[0] + 2]) >= 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			} else {
				ret = (mport_version_cmp(baseline, &s[greater[0] + 1]) > 0) ? 0 : -1;
				if (ret == -1) {
					free(s);
					s = NULL;
					return (ret);
				}
			}
		}

		if (greater[0] > 0) {
			s[greater[0]] = '\0';
			ret = (mport_version_cmp(baseline, &s[greater[0] + 1]) > 0) ? 0 : -1;
			if (ret == -1) {
				free(s);
				s = NULL;
				return (ret);
			}
		} else if (less[0] > 0) {
			s[less[0]] = '\0';
			ret = (mport_version_cmp(baseline, &s[less[0] + 1]) < 0) ? 0 : -1;
			if (ret == -1) {
				free(s);
				s = NULL;
				return (ret);
			}
		}

		free(s);
		s = NULL;
	}

    return (ret);
}

static void
parse_version(const char *in, struct version *v) 
{
    char *s = strdup(in);
    char *underscore;
    char *comma;
    // greater than and less than prevent multiversion strings from getting parsed incorrectly
    // so 2.0<1.5 would just be 2.0. Ideally we need to catch this upstream and do the right check.
    char *lessthan;
    char *greaterthan;

    underscore = rindex(s, '_');
    comma = rindex(s, ',');
	lessthan = rindex(s, '<');
	greaterthan = rindex(s, '>');

	if (lessthan != NULL) {
		*lessthan = '\0';
	}

	if (greaterthan != NULL) {
		*greaterthan = '\0';
	}

    if (comma == NULL) {
        v->epoch = 0;
    } else {
        *comma = '\0';
        v->epoch = (int) strtol(comma + 1, NULL, 10);
    }

    if (underscore == NULL) {
        v->revision = 0;
    } else {
        *underscore = '\0';
        v->revision = (int) strtol(underscore + 1, NULL, 10);
    }

    v->version = s;
}

static int
cmp_ints(int a, int b) 
{

    if (a == b)
        return 0;
    if (a < b)
        return -1;

    return 1;
}

static int
cmp_versions(char *a, char *b)
{
    int a_sub, b_sub, result = 0;

    while (*a || *b) {
        if (*a) {
            while (*a == '.' || *a == '+')
                a++;

            if (isdigit(*a)) {
                a_sub = (int) strtol(a, &a, 10);
            } else {
                a_sub = (int) *a;
                a++;
            }
        } else {
            a_sub = 0;
        }

        if (*b) {
            while (*b == '.' || *b == '+')
                b++;

            if (isdigit(*b)) {
                b_sub = (int) strtol(b, &b, 10);
            } else {
                b_sub = (int) *b;
                b++;
            }
        } else {
            b_sub = 0;
        }

        result = cmp_ints(a_sub, b_sub);

        if (result != 0)
            break;
    }

    return (result);
}    
