/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Lucas Holt
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

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <err.h>

#if defined(__MidnightBSD__)
#include <ohash.h>
#endif

static void * ecalloc(size_t, void *);
static void efree(void *, size_t, void *);

static void *
ecalloc(size_t s1, void *data) {
	void *p;

	if (!(p = malloc(s1)))
		err(1, "malloc");
	memset(p, 0, s1);
	return p;
}

static void
efree(void *p, size_t s1, void *data){
	free(p);
}

MPORT_PUBLIC_API int
mport_upgrade(mportInstance *mport) {
	mportPackageMeta **packs, **packs_orig = NULL;
	int total = 0;
	int updated = 0;
#if defined(__MidnightBSD__)
	struct ohash_info info = { 0, NULL, ecalloc, efree, NULL };
	struct ohash h;
#endif
	unsigned int slot;
	char *key = NULL;
	char *msg;

	if (mport == NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "mport not initialized\n");
	}

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't load package list\n");
	}

	if (packs_orig == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "No packages installed");
		mport_call_msg_cb(mport, "No packages installed\n");
		return (MPORT_ERR_FATAL);
	}

#if defined(__MidnightBSD__)
	ohash_init(&h, 6, &info);
	#endif

	// check for moved/expired packages first
	packs = packs_orig;
	while (*packs != NULL) {
		mportIndexMovedEntry **movedEntries;

		#if defined(__MidnightBSD__)
		slot = ohash_qlookup(&h, (*packs)->name);
		key = ohash_find(&h, slot);
		if (key != NULL) {
			packs++;
			continue;
		}
		#endif

		if (mport_moved_lookup(mport, (*packs)->origin, &movedEntries) != MPORT_OK ||
		    movedEntries == NULL || *movedEntries == NULL) {
			packs++;
			continue;
		}

		if ((*movedEntries)->date[0] != '\0') {
			asprintf(&msg, "Package %s is deprecated with expiration date %s. Do you want to remove it?", (*packs)->name, (*movedEntries)->date);
			if ((mport->confirm_cb)(msg, "Delete", "Don't delete", 1) == MPORT_OK) {
				(*packs)->action = MPORT_ACTION_DELETE;
				mport_delete_primative(mport, (*packs), true);
				#if defined(__MidnightBSD__)
				ohash_insert(&h, slot, (*packs)->name);
				#endif
			}	

			packs++;
			continue;
		}		

		if ((*movedEntries)->moved_to_pkgname != NULL && (*movedEntries)->moved_to_pkgname[0]!= '\0') {   
			mport_call_msg_cb(mport, "Package %s has moved to %s. Migrating %s\n", (*packs)->name, (*movedEntries)->moved_to_pkgname,  (*movedEntries)->moved_to_pkgname);
			(*packs)->action = MPORT_ACTION_UPGRADE;
			mport_delete_primative(mport, (*packs), true);
			// TODO: how to mark this action as an update?
			mport_install_single(mport, (*movedEntries)->moved_to_pkgname,  NULL, NULL, (*packs)->automatic);
#if defined(__MidnightBSD__)
			ohash_insert(&h, slot, (*packs)->name);
			ohash_insert(&h, slot, (*movedEntries)->moved_to_pkgname);
#endif
		}
		packs++;
	}

    // update packages that haven't moved already
	packs = packs_orig;
	while (*packs != NULL) {
#if defined(__MidnightBSD__)
		slot = ohash_qlookup(&h, (*packs)->name);
		key = ohash_find(&h, slot);
		if (key == NULL) {
		#endif
			int match = mport_index_check(mport, *packs);
			if (match == 1) {
				(*packs)->action = MPORT_ACTION_UPGRADE;
				#if defined(__MidnightBSD__)
				updated += mport_update_down(mport, *packs, &info, &h);
				#else
				updated += mport_update_down(mport, *packs, NULL, NULL);
				#endif
			} else if (match == 2) {
				mportIndexEntry **ieUpdateMe;
				if (mport_index_lookup_pkgname(mport, (*packs)->origin, &ieUpdateMe) != MPORT_OK) {
					SET_ERRORX(MPORT_ERR_WARN, "Error Looking up package origin %s", (*packs)->origin);
					return (0);
				}

				if (ieUpdateMe == NULL) {
					packs++;
					continue;
				}

				char *msg = NULL;
				(void)asprintf(
					&msg, "The package you have installed %s appears to have been replaced by %s. Do you want to update?",
					 (*packs)->name, (*ieUpdateMe)->pkgname);
				if ((mport->confirm_cb)(msg, "Update", "Don't Update", 0) != MPORT_OK) {
					(*packs)->action = MPORT_ACTION_UPGRADE;
					mport_delete_primative(mport, (*packs), true);
					// TODO: how to mark this action as an update?
					mport_install_single(mport, (*ieUpdateMe)->pkgname,  NULL, NULL, (*packs)->automatic);
					updated++;
				}
				free(msg);
			}
#if defined(__MidnightBSD__)
		}
#endif
		packs++;
		total++;
	}
	mport_pkgmeta_vec_free(packs_orig);
	packs_orig = NULL;
	packs = NULL;
#if defined(__MidnightBSD__)
	ohash_delete(&h);
#endif

	mport_call_msg_cb(mport, "Packages updated: %d\nTotal: %d\n", updated, total);
	return (MPORT_OK);
}

int
mport_update_down(mportInstance *mport, mportPackageMeta *pack, struct ohash_info *info, struct ohash *h) {
	mportPackageMeta **depends, **depends_orig;
	int ret = 0;
	unsigned int slot;
	char *key = NULL;

	if (mport_pkgmeta_get_downdepends(mport, pack, &depends_orig) == MPORT_OK) {
		if (depends_orig == NULL) {
			
			#if defined(__MidnightBSD__)
			slot = ohash_qlookup(h, pack->name);
			key = ohash_find(h, slot);
			#endif
			if (key == NULL) {
				if (mport_index_check(mport, pack)) {
					mport_call_msg_cb(mport, "Updating %s\n", pack->name);
					pack->action = MPORT_ACTION_UPGRADE;
					if (mport_update(mport, pack->name) !=0) {
						mport_call_msg_cb(mport, "Error updating %s\n", pack->name);
						ret = 0;
					} else {
						ret = 1;
#if defined(__MidnightBSD__)
						ohash_insert(h, slot, pack->name);
#endif
					}
				} else
					ret = 0;
			} else {
				ret = 0;
			}
		} else {
			depends = depends_orig;
			while (*depends != NULL) {
#if defined(__MidnightBSD__)
				slot = ohash_qlookup(h, (*depends)->name);
				key = ohash_find(h, slot);
#endif
				if (key == NULL) {
					ret += mport_update_down(mport, (*depends), info, h);
					if (mport_index_check(mport, *depends)) {
						mport_call_msg_cb(mport, "Updating depends %s\n", (*depends)->name);
						(*depends)->action = MPORT_ACTION_UPGRADE;
						if (mport_update(mport, (*depends)->name) != 0) {
							mport_call_msg_cb(mport, "Error updating %s\n", (*depends)->name);
						} else {
							ret++;
#if defined(__MidnightBSD__)
							ohash_insert(h, slot, (*depends)->name);
#endif
						}
					}
				}
				depends++;
			}
			if (mport_index_check(mport, pack)) {
				if (mport_update(mport, pack->name) != 0) {
					mport_call_msg_cb(mport, "Error updating %s\n", pack->name);
				} else {
					ret++;

#if defined(__MidnightBSD__)
					slot = ohash_qlookup(h, pack->name);
					key = ohash_find(h, slot);
					if (key == NULL)
						ohash_insert(h, slot, pack->name);
#endif
				}
			}
		}
		mport_pkgmeta_vec_free(depends_orig);
		depends_orig = NULL;
		depends = NULL;
	}

	return (ret);
}
