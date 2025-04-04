/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Lucas Holt
 * Copyright (c) 2007 Chris Reinhardt
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

#include "mport.h"
#include "mport_private.h"
#include <string.h>
#include <stdlib.h>

static int set_prefix_to_installed(mportInstance *, mportPackageMeta *);


MPORT_PUBLIC_API int
mport_update_primative(mportInstance *mport, const char *filename)
{
    mportBundleRead *bundle = NULL;
    mportPackageMeta **pkgs = NULL;
    mportPackageMeta *pkg = NULL;
    mportPackageMeta **packs_start = NULL;
    mportPackageMeta **already_installed = NULL;

    if ((bundle = mport_bundle_read_new()) == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

    if (mport_bundle_read_init(bundle, filename) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (mport_bundle_read_prep_for_install(mport, bundle) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    if (mport_pkgmeta_read_stub(mport, &pkgs) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    packs_start = pkgs;
    for (int i = 0; *(pkgs + i) != NULL; i++) {
        pkg = pkgs[i];
		pkg->install_date = mport_get_time();

        /* retain automatic flag from previous install. */
        if (mport_pkgmeta_search_master(mport, &already_installed, "pkg=%Q", pkg->name) == MPORT_OK) {
            if (already_installed != NULL && already_installed[0] != NULL) {
              pkg->automatic = already_installed[0]->automatic;
              pkg->locked = already_installed[0]->locked;

              mport_pkgmeta_vec_free(already_installed);
            }
        }

        if (mport_lock_islocked(pkg) == MPORT_LOCKED) {
            mport_call_msg_cb(mport, "Unable to update %s-%s: package is locked.", pkg->name, pkg->version);
            mport_set_err(MPORT_OK, NULL);
            continue;
        }

        int flag = MPORT_PRECHECK_CONFLICTS|MPORT_PRECHECK_DEPENDS;
        if (!mport->force)
            flag = flag | MPORT_PRECHECK_UPGRADEABLE;

        if (
            (mport_check_preconditions(mport, pkg, flag) != MPORT_OK) ||
            (set_prefix_to_installed(mport, pkg) != MPORT_OK) ||
            (mport_bundle_read_update_pkg(mport, bundle, pkg) != MPORT_OK)
        ) {
            mport_call_msg_cb(mport, "Unable to update %s-%s: %s", pkg->name, pkg->version, mport_err_string());
            mport_set_err(MPORT_OK, NULL);
        }
    }

    mport_pkgmeta_vec_free(packs_start);

    if (mport_bundle_read_finish(mport, bundle) != MPORT_OK)
        RETURN_CURRENT_ERROR;

    return MPORT_OK;
}

static int
set_prefix_to_installed(mportInstance *mport, mportPackageMeta *pkg)
{
    sqlite3_stmt *stmt;
    int ret = MPORT_OK;
    const char *prefix;

    if (mport_db_prepare(mport->db, &stmt, "SELECT prefix FROM packages WHERE pkg=%Q", pkg->name) != MPORT_OK) {
		sqlite3_finalize(stmt);
		RETURN_CURRENT_ERROR;
	}

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            prefix = sqlite3_column_text(stmt, 0);

            if (strcmp(prefix, pkg->prefix) != 0) {
                free(pkg->prefix);
                if ((pkg->prefix = strdup(prefix)) == NULL) {
                    ret = MPORT_ERR_FATAL;
                }
            }
            break;
        case SQLITE_DONE:
            ret = SET_ERRORX(MPORT_ERR_FATAL, "%s not in master db, after passing precondition check!", pkg->name);
            break;
        default:
            ret = SET_ERROR(MPORT_ERR_FATAL, sqlite3_errmsg(mport->db));
            break;
    }

    sqlite3_finalize(stmt);

    return ret;
}

