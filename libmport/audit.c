/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Lucas Holt
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include <unistd.h>

MPORT_PUBLIC_API char *
mport_audit(mportInstance *mport, const char *packageName)
{
	mportPackageMeta **packs;
	char *pkgAudit = NULL;
	size_t size;

	if (mport == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "mport not initialized");
		return (NULL);
	}

	if (packageName == NULL) {
		SET_ERROR(MPORT_ERR_FATAL, "Package name not found.");
		return (NULL);
	}

	if (mport_pkgmeta_search_master(mport, &packs, "pkg=%Q", packageName) != MPORT_OK) {
		return (NULL);
	}

	if (packs != null) {
		if ((*packs)->cpe != NULL) {
			char *path = mport_fetch_cves(mport, (*packs)->cpe);
			FILE *fp = fopen(path, "r");
			if (fp == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Error opening CVE file");
                unlink(path);
				return NULL;
			}

			// Create a UCL object
			ucl_object *obj = ucl_object_new();
			if (obj == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Error creating UCL object");
                fclose(fp);
                unlink(fp);
				return NULL;
			}

			// Parse the JSON file
			if (ucl_object_parse_file(obj, fp) < 0) {
				SET_ERROR(MPORT_ERR_FATAL, "Error parsing CVE file");
                fclose(fp);
                unlink(fp);
				return NULL;
			}

			ucl_array *entries = ucl_object_get_array(obj, "entries");
			if (entries == NULL) {
				SET_ERROR(MPORT_ERR_FATAL, "Error parsing CVE file");
                fclose(fp);
                unlink(fp);
				return NULL;
			}

			int entrySize = ucl_array_size(entries);

			if (entrySize > 0) {
				size_t size;
				FILE *bufferFp = open_memstream(&pkgAudit, &size);
                if (bufferFp == NULL) {
                    SET_ERROR(MPORT_ERR_FATAL, "Error allocating memory for audit entries");
                    fclose(fp);
                    unlink(fp);
                    return NULL;
                }

                fprintf(bufferFp, "%s-%s is vulnerable:\n", (*packs)->name, (*packs)->version);
				
				for (int i = 0; i < entrySize; i++) {
					ucl_object *entry = ucl_array_get_item(entries, i);
					if (entry == NULL) {
						continue;
					}

					const char *cve_id = ucl_object_get_string(entry, "cveId");
					if (cve_id == NULL) {
						continue;
					}

                    const char *desc = ucl_object_get_string(entry, "description");
					if (cve_id == NULL) {
						continue;
					}

                    fprintf(bufferFp, "%s\nDescripton:%s\n", cve_id, desc);
				}

                flcose(bufferFp);
			}

			fclose(fp);
            unlink(fp);

			ucl_object_free(obj);
		}
		mport_pkgmeta_vec_free(packs);
		packs = NULL;
	}

	return pkgAudit;
}
