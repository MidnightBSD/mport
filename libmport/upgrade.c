/*-
 * Copyright (c) 2021 Lucas Holt
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
#include "hashmap.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <assert.h>

#define KEY_MAX_LENGTH (256)
#define KEY_COUNT (1024*1024)

typedef struct data_struct_s
{
	char key_string[KEY_MAX_LENGTH];
	bool updated;
} data_struct_t;

static int add_entry(map_t map, char *pkgname);

MPORT_PUBLIC_API int
mport_upgrade(mportInstance *mport) {
	mportPackageMeta **packs, **packs_orig;
	int total = 0;
	int updated = 0;
	map_t map;

	if (mport_pkgmeta_list(mport, &packs_orig) != MPORT_OK) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Couldn't load package list\n");
	}

	if (packs_orig == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "No packages installed");
		mport_call_msg_cb(mport, "No packages installed\n");
		return (MPORT_ERR_FATAL);
	}

fprintf(stderr, "pre hashmap");
	map = hashmap_new();

	packs = packs_orig;
	while (*packs != NULL) {
		if (mport_index_check(mport, *packs)) {
			updated += mport_update_down(mport, *packs, map);
		}
		packs++;
		total++;
	}

fprintf(stderr, "miller time\n");
	packs = packs_orig;
/*	while (*packs != NULL) {
		hashmap_remove(map, (*packs)->name);
	}
*/

	mport_pkgmeta_vec_free(packs_orig);

fprintf(stderr, "free me bitch\n");
	hashmap_free(map);

	mport_call_msg_cb(mport, "Packages updated: %d\nTotal: %d\n", updated, total);
	return (0);
}

int
add_entry(map_t map, char *pkgname) {
	data_struct_t* value = malloc(sizeof(data_struct_t));
	snprintf(value->key_string, KEY_MAX_LENGTH, "%s", pkgname);
	value->updated = true;

fprintf(stderr, "put a pkgname %s\n", pkgname);
	return hashmap_put(map, pkgname, value);
}

int
mport_update_down(mportInstance *mport, mportPackageMeta *pack, map_t map) {
	mportPackageMeta **depends;
	int ret = 0;

fprintf(stderr, "exists time %s\n", pack->name);
	if (hashmap_exists(map, pack->name))
		return (ret);
fprintf(stderr, "post exists\n");

	if (mport_pkgmeta_get_downdepends(mport, pack, &depends) == MPORT_OK) {
		if (depends == NULL) {
			if (mport_index_check(mport, pack) && !hashmap_exists(map, pack->name)) {
				mport_call_msg_cb(mport, "Updating %s\n", pack->name);
				if (mport_update(mport, pack->name) != 0) {
					mport_call_msg_cb(mport, "Error updating %s\n", pack->name);
					ret = 0;
				} else {
					ret = 1;
					add_entry(map, pack->name);
				}
			} else
				ret = 0;
		} else {
			while (*depends != NULL) {
fprintf(stderr, "doing stuff\n");
				ret += mport_update_down(mport, (*depends), map);
				if (mport_index_check(mport, *depends) && !hashmap_exists(map, (*depends)->name)) {
					mport_call_msg_cb(mport, "Updating depends %s\n", (*depends)->name);
					if (mport_update(mport, (*depends)->name) != 0) {
						mport_call_msg_cb(mport, "Error updating %s\n", (*depends)->name);
					} else {
						ret++;
						add_entry(map, (*depends)->name);
					}
				}
				depends++;
			}
			if (mport_index_check(mport, pack) && !hashmap_exists(map, pack->name)) {
				if (mport_update(mport, pack->name) != 0) {
					mport_call_msg_cb(mport, "Error updating %s\n", pack->name);
				} else {
					ret++;
					add_entry(map, pack->name);
				}
			}
		}
		mport_pkgmeta_vec_free(depends);
		depends = NULL;
	}

	return (ret);
}
