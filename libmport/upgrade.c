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

/* Cache structure for index check results */
typedef struct {
	int result;  /* 0 = no update, 1 = update available, 2 = origin match */
	bool cached;
} index_check_cache_t;

/* Cache structure for moved lookup results */
typedef struct {
	mportIndexMovedEntry **entries;
	bool cached;
} moved_lookup_cache_t;

MPORT_PUBLIC_API int
mport_upgrade(mportInstance *mport) {
	mportPackageMeta **packs, **packs_orig = NULL;
	int total = 0;
	int updated = 0;
#if defined(__MidnightBSD__)
	struct ohash_info info = { 0, NULL, ecalloc, efree, NULL };
	struct ohash h;  /* Tracks processed packages */
	struct ohash_info cache_info = { 0, NULL, ecalloc, efree, NULL };
	struct ohash index_cache;  /* Caches index_check results */
	struct ohash moved_cache;  /* Caches moved_lookup results */
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
	ohash_init(&index_cache, 6, &cache_info);
	ohash_init(&moved_cache, 6, &cache_info);
	#endif

	// check for moved/expired packages first
	packs = packs_orig;
	while (*packs != NULL) {
		mportIndexMovedEntry **movedEntries = NULL;
		bool use_cached_moved = false;

		#if defined(__MidnightBSD__)
		slot = ohash_qlookup(&h, (*packs)->name);
		key = ohash_find(&h, slot);
		if (key != NULL) {
			packs++;
			continue;
		}

		/* Check cache for moved lookup */
		slot = ohash_qlookup(&moved_cache, (*packs)->origin);
		moved_lookup_cache_t *cached_moved = (moved_lookup_cache_t *)ohash_find(&moved_cache, slot);
		if (cached_moved != NULL && cached_moved->cached) {
			movedEntries = cached_moved->entries;
			use_cached_moved = true;
		}
		#endif

		if (!use_cached_moved) {
			if (mport_moved_lookup(mport, (*packs)->origin, &movedEntries) != MPORT_OK ||
			    movedEntries == NULL || *movedEntries == NULL) {
				#if defined(__MidnightBSD__)
				/* Cache negative result to avoid repeated queries */
				moved_lookup_cache_t *cache_entry = (moved_lookup_cache_t *)ecalloc(sizeof(moved_lookup_cache_t), NULL);
				cache_entry->entries = NULL;
				cache_entry->cached = true;
				ohash_insert(&moved_cache, slot, cache_entry);
				#endif
				packs++;
				continue;
			}
			#if defined(__MidnightBSD__)
			/* Cache positive result - entries are managed by mport_moved_lookup */
			moved_lookup_cache_t *cache_entry = (moved_lookup_cache_t *)ecalloc(sizeof(moved_lookup_cache_t), NULL);
			cache_entry->entries = movedEntries;
			cache_entry->cached = true;
			ohash_insert(&moved_cache, slot, cache_entry);
			#endif
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

		if ((*movedEntries)->moved_to_pkgname[0] != '\0') {   
			mport_call_msg_cb(mport, "Package %s has moved to %s. Migrating %s\n", (*packs)->name, (*movedEntries)->moved_to_pkgname,  (*movedEntries)->moved_to_pkgname);
			(*packs)->action = MPORT_ACTION_UPGRADE;
			mport_delete_primative(mport, (*packs), true);
			// TODO: how to mark this action as an update?
			mport_install_single(mport, (*movedEntries)->moved_to_pkgname,  NULL, NULL, (*packs)->automatic);
#if defined(__MidnightBSD__)
			ohash_insert(&h, slot, (*packs)->name);
			slot = ohash_qlookup(&h, (*movedEntries)->moved_to_pkgname);
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
			int match;
			bool use_cached_index = false;

			#if defined(__MidnightBSD__)
			/* Check cache for index_check result */
			slot = ohash_qlookup(&index_cache, (*packs)->name);
			index_check_cache_t *cached_index = (index_check_cache_t *)ohash_find(&index_cache, slot);
			if (cached_index != NULL && cached_index->cached) {
				match = cached_index->result;
				use_cached_index = true;
			} else {
				match = mport_index_check(mport, *packs);
				/* Cache the result */
				index_check_cache_t *cache_entry = (index_check_cache_t *)ecalloc(sizeof(index_check_cache_t), NULL);
				cache_entry->result = match;
				cache_entry->cached = true;
				ohash_insert(&index_cache, slot, cache_entry);
			}
			#else
			match = mport_index_check(mport, *packs);
			#endif

			if (match == 1) {
				(*packs)->action = MPORT_ACTION_UPGRADE;
				#if defined(__MidnightBSD__)
				updated += mport_update_down(mport, *packs, &info, &h, &index_cache);
				#else
				updated += mport_update_down(mport, *packs, NULL, NULL, NULL);
				#endif
			} else if (match == 2) {
				mportIndexEntry **ieUpdateMe;
				if (mport_index_lookup_pkgname(mport, (*packs)->origin, &ieUpdateMe) != MPORT_OK) {
					SET_ERRORX(MPORT_ERR_WARN, "Error Looking up package origin %s", (*packs)->origin);
					return (0);
				}

				if (ieUpdateMe == NULL || *ieUpdateMe == NULL) {
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
					#if defined(__MidnightBSD__)
					slot = ohash_qlookup(&h, (*ieUpdateMe)->pkgname);
					ohash_insert(&h, slot, (*ieUpdateMe)->pkgname);
					#endif
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
	/* Free cached moved entries - only free the cache structures, not the actual entries */
	/* The actual moved entries are managed elsewhere and may still be in use */
	for (slot = ohash_first(&moved_cache, &key); key != NULL; slot = ohash_next(&moved_cache, &slot)) {
		moved_lookup_cache_t *cache_entry = (moved_lookup_cache_t *)ohash_find(&moved_cache, slot);
		if (cache_entry != NULL) {
			efree(cache_entry, sizeof(moved_lookup_cache_t), NULL);
		}
	}
	ohash_delete(&moved_cache);
	/* Free index cache entries */
	for (slot = ohash_first(&index_cache, &key); key != NULL; slot = ohash_next(&index_cache, &slot)) {
		index_check_cache_t *cache_entry = (index_check_cache_t *)ohash_find(&index_cache, slot);
		if (cache_entry != NULL) {
			efree(cache_entry, sizeof(index_check_cache_t), NULL);
		}
	}
	ohash_delete(&index_cache);
	ohash_delete(&h);
#endif

	mport_call_msg_cb(mport, "Packages updated: %d\nTotal: %d\n", updated, total);
	return (MPORT_OK);
}

int
mport_update_down(mportInstance *mport, mportPackageMeta *pack, struct ohash_info *info, struct ohash *h, struct ohash *index_cache) {
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
				int match;
				bool use_cached = false;

				#if defined(__MidnightBSD__)
				if (index_cache != NULL) {
					slot = ohash_qlookup(index_cache, pack->name);
					index_check_cache_t *cached = (index_check_cache_t *)ohash_find(index_cache, slot);
					if (cached != NULL && cached->cached) {
						match = cached->result;
						use_cached = true;
					}
				}
				#endif

				if (!use_cached) {
					match = mport_index_check(mport, pack);
					#if defined(__MidnightBSD__)
					if (index_cache != NULL) {
						/* Cache the result */
						index_check_cache_t *cache_entry = (index_check_cache_t *)ecalloc(sizeof(index_check_cache_t), NULL);
						cache_entry->result = match;
						cache_entry->cached = true;
						ohash_insert(index_cache, slot, cache_entry);
					}
					#endif
				}

				if (match) {
					mport_call_msg_cb(mport, "Updating %s\n", pack->name);
					pack->action = MPORT_ACTION_UPGRADE;
					if (mport_update(mport, pack->name) !=0) {
						mport_call_msg_cb(mport, "Error updating %s\n", pack->name);
						ret = 0;
					} else {
						ret = 1;
#if defined(__MidnightBSD__)
						slot = ohash_qlookup(h, pack->name);
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
					ret += mport_update_down(mport, (*depends), info, h, index_cache);
					
					int match;
					bool use_cached = false;

					#if defined(__MidnightBSD__)
					if (index_cache != NULL) {
						slot = ohash_qlookup(index_cache, (*depends)->name);
						index_check_cache_t *cached = (index_check_cache_t *)ohash_find(index_cache, slot);
						if (cached != NULL && cached->cached) {
							match = cached->result;
							use_cached = true;
						}
					}
					#endif

					if (!use_cached) {
						match = mport_index_check(mport, *depends);
						#if defined(__MidnightBSD__)
						if (index_cache != NULL) {
							/* Cache the result */
							index_check_cache_t *cache_entry = (index_check_cache_t *)ecalloc(sizeof(index_check_cache_t), NULL);
							cache_entry->result = match;
							cache_entry->cached = true;
							ohash_insert(index_cache, slot, cache_entry);
						}
						#endif
					}

					if (match) {
						mport_call_msg_cb(mport, "Updating depends %s\n", (*depends)->name);
						(*depends)->action = MPORT_ACTION_UPGRADE;
						if (mport_update(mport, (*depends)->name) != 0) {
							mport_call_msg_cb(mport, "Error updating %s\n", (*depends)->name);
						} else {
							ret++;
#if defined(__MidnightBSD__)
							slot = ohash_qlookup(h, (*depends)->name);
							ohash_insert(h, slot, (*depends)->name);
#endif
						}
					}
				}
				depends++;
			}
			
			int match;
			bool use_cached = false;

			#if defined(__MidnightBSD__)
			if (index_cache != NULL) {
				slot = ohash_qlookup(index_cache, pack->name);
				index_check_cache_t *cached = (index_check_cache_t *)ohash_find(index_cache, slot);
				if (cached != NULL && cached->cached) {
					match = cached->result;
					use_cached = true;
				}
			}
			#endif

			if (!use_cached) {
				match = mport_index_check(mport, pack);
				#if defined(__MidnightBSD__)
				if (index_cache != NULL) {
					/* Cache the result */
					index_check_cache_t *cache_entry = (index_check_cache_t *)ecalloc(sizeof(index_check_cache_t), NULL);
					cache_entry->result = match;
					cache_entry->cached = true;
					ohash_insert(index_cache, slot, cache_entry);
				}
				#endif
			}

			if (match) {
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
