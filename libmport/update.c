/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2026 Lucas Holt
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

static void
free_moved_entries(mportIndexMovedEntry **entries)
{
	mportIndexMovedEntry **entry;

	if (entries == NULL)
		return;

	for (entry = entries; *entry != NULL; entry++)
		free(*entry);
	free(entries);
}

MPORT_PUBLIC_API int
mport_update(mportInstance *mport, const char *packageName)
{
	char *path = NULL;
	mportDependsEntry **depends = NULL;
	mportDependsEntry **depends_orig = NULL;
	mportIndexEntry **indexEntries = NULL;
	mportIndexEntry *indexEntry = NULL;
	mportPackageMeta **packs_meta = NULL;
	/*@null@*/ mportIndexMovedEntry **movedEntries = NULL;
	char *replacement_path = NULL;
	char expiry[32];
	mportAutomatic automatic = MPORT_EXPLICIT;
	int result;
	int ret;

	if (packageName == NULL) {
		return (MPORT_ERR_WARN);
	}

	/* Check for MOVE operations: look up the installed package to get its origin,
	 * then consult the moved table in the index. */
	if (mport_pkgmeta_search_master(mport, &packs_meta, "pkg=%Q", packageName) == MPORT_OK &&
	    packs_meta != NULL && *packs_meta != NULL && (*packs_meta)->origin != NULL) {
		if (mport_moved_lookup(mport, (*packs_meta)->origin, &movedEntries) == MPORT_OK &&
		    movedEntries != NULL && *movedEntries != NULL) {
			if ((*movedEntries)->date[0] != '\0') {
				/* Package is deprecated/expired; save date, then warn and refuse.
				 */
				strlcpy(expiry, (*movedEntries)->date, sizeof(expiry));
				mport_call_msg_cb(mport,
				    "Package %s is deprecated and expired on %s\n", packageName,
				    expiry);
				free_moved_entries(movedEntries);
				mport_pkgmeta_vec_free(packs_meta);
				return SET_ERRORX(MPORT_ERR_WARN,
				    "Package %s is deprecated (expiry: %s)", packageName, expiry);
			}

			if ((*movedEntries)->moved_to_pkgname[0] != '\0') {
				/* Package has been moved to a new name; migrate it. */
				automatic = (*packs_meta)->automatic;
				mport_call_msg_cb(mport,
				    "Package %s has moved to %s. Migrating to %s\n", packageName,
				    (*movedEntries)->moved_to_pkgname,
				    (*movedEntries)->moved_to_pkgname);
				ret = mport_download(mport, (*movedEntries)->moved_to_pkgname,
				    false, false, &replacement_path);
				if (ret != MPORT_OK) {
					free_moved_entries(movedEntries);
					mport_pkgmeta_vec_free(packs_meta);
					return ret;
				}
				if (replacement_path == NULL) {
					free_moved_entries(movedEntries);
					mport_pkgmeta_vec_free(packs_meta);
					return SET_ERROR(
					    MPORT_ERR_FATAL, "Downloaded package path is missing");
				}
				(*packs_meta)->action = MPORT_ACTION_UPGRADE;
				ret = mport_delete_primative(mport, *packs_meta, 1);
				if (ret == MPORT_OK)
					ret = mport_install_primative(
					    mport, replacement_path, NULL, automatic);
				free(replacement_path);
				free_moved_entries(movedEntries);
				mport_pkgmeta_vec_free(packs_meta);
				return ret;
			}
		}
		free_moved_entries(movedEntries);
		movedEntries = NULL;
	}
	mport_pkgmeta_vec_free(packs_meta);
	packs_meta = NULL;

	if (mport_index_select_pkgname(mport, packageName, "Multiple packages match your query.",
		&indexEntries, &indexEntry) != MPORT_OK)
		return mport_err_code();

	result = mport_download(
	    mport, indexEntry == NULL ? packageName : indexEntry->pkgname, false, false, &path);
	if (result != MPORT_OK) {
		mport_index_entry_free_vec(indexEntries);
		return result;
	}

	/* in the event the package is not found in the index, it could be user generated, and we
	   still want to update it if present */
	if (indexEntry == NULL) {
		mport_call_msg_cb(mport, "Package %s not found in the index\n", packageName);
	} else {
		/* get the dependency list and start updating/installing missing entries */
		if (mport_index_depends_list(mport, indexEntry->pkgname, indexEntry->version,
			&depends_orig) != MPORT_OK) {
			mport_call_msg_cb(mport, "Failed to get dependency list for %s: %s\n",
			    packageName, mport_err_string());
			mport_index_entry_free_vec(indexEntries);
			return mport_err_code();
		}

		depends = depends_orig;
		while (*depends != NULL) {
			if (mport_install_depends(mport, (*depends)->d_pkgname,
				(*depends)->d_version, MPORT_AUTOMATIC) != MPORT_OK) {
				mport_call_msg_cb(mport, "%s", mport_err_string());
				mport_index_depends_free_vec(depends);
				mport_index_entry_free_vec(indexEntries);
				if (mport->ignoreMissing) {
					mport_call_msg_cb(mport,
					    "Ignoring missing dependency %s-%s\n",
					    (*depends)->d_pkgname, (*depends)->d_version);
					depends++;
					continue;
				}
				return mport_err_code();
			}
			depends++;
		}

		mport_index_depends_free_vec(depends_orig);
		depends_orig = NULL;
		depends = NULL;
	}

	if (mport_update_primative(mport, path) != MPORT_OK) {
		mport_call_msg_cb(mport, "%s\n", mport_err_string());
		free(path);
		path = NULL;
		mport_index_entry_free_vec(indexEntries);
		return mport_err_code();
	}

	free(path);
	path = NULL;
	mport_index_entry_free_vec(indexEntries);

	return (MPORT_OK);
}
