/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, 2025 Lucas Holt
 * Copyright (c) 2009 Chris Reinhardt
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

#include "mport.h"
#include "mport_private.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fetch.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFSIZE 1024 * 8

static int fetch(mportInstance *, const char *, const char *);
static int fetch_bundle_to_dir(mportInstance *, const char *, const char *, const char *);
static int fetch_to_file(mportInstance *, const char *, FILE *, bool);


/* mport_fetch_index(mport)
 *
 * Fetch the index from a remote, or the bootstrap if we don't currently
 * have an index. If the current index is recentish, then don't do
 * anything.
 */
int
mport_fetch_index(mportInstance *mport)
{
	char **mirrors = NULL;
	char **mirrorsPtr = NULL;
	char *url = NULL;
	char *hashUrl = NULL;
	char *osrel;
	int mirrorCount = 0;
	
	MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_index()");
 
	if (mport_index_get_mirror_list(mport, &mirrors, &mirrorCount) != MPORT_OK)
		RETURN_CURRENT_ERROR;

#ifdef DEBUG 
	fprintf(stderr, "Mirror count is %d\n", mirrorCount);
#endif
 
	mirrorsPtr = mirrors;
	osrel = mport_get_osrelease(mport);
	 
	while (mirrorsPtr != NULL) {
		if (*mirrorsPtr == NULL)
			break;
		
		if (asprintf(&url, "%s/%s/%s/%s", *mirrorsPtr,  MPORT_ARCH, osrel, MPORT_INDEX_FILE_SOURCE) == -1 ||
		    asprintf(&hashUrl, "%s/%s/%s/%s", *mirrorsPtr,  MPORT_ARCH, osrel, MPORT_INDEX_FILE_SOURCE ".sha256") == -1) {
			free(url);
			free(hashUrl);
			for (int mi = 0; mi < mirrorCount; mi++)
				free(mirrors[mi]);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");
		}

		if (fetch(mport, url, MPORT_INDEX_FILE_COMPRESSED) == MPORT_OK) {
			if (fetch(mport, hashUrl, MPORT_INDEX_FILE_HASH) == MPORT_OK) {
				char *hash = mport_extract_hash_from_file(MPORT_INDEX_FILE_HASH);
				free(hashUrl);

				if (hash == NULL || mport_verify_hash(MPORT_INDEX_FILE_COMPRESSED, hash) == 0) {
					mport_call_msg_cb(mport, "Index hash failed verification: %s\n", hash);
					free(hash);
					free(url);
					continue;
				} else {
					mport_decompress_zstd(MPORT_INDEX_FILE_COMPRESSED, mport_index_file_path());
					free(url);
					free(hash);
					for (int mi = 0; mi < mirrorCount; mi++)
						free(mirrors[mi]);
					return MPORT_OK;
				}
			} else {
				free(hashUrl);
			}
		}
		free(url);
		mirrorsPtr++;
	}

	free(osrel);

	/* fallback to mport bootstrap site in a pinch */
	if (mport_fetch_bootstrap_index(mport) == MPORT_OK)
		return MPORT_OK;
	
	for (int mi = 0; mi < mirrorCount; mi++) 
		free(mirrors[mi]);

	free(mirrors);
	RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to fetch index file: %s", mport_err_string());
}



/* mport_fetch_bootstrap_index(mportInstance *mport)
 *
 * Fetches the index for the bootstrap site. The index need 
 * not be loaded for this to be used.
 */
int
mport_fetch_bootstrap_index(mportInstance *mport)
{
	int result;
	char *url;
	char *hashUrl;
	char *osrel;

	osrel = mport_get_osrelease(mport);

	if (asprintf(&url, "%s/%s/%s/%s", MPORT_BOOTSTRAP_INDEX_URL, MPORT_ARCH, osrel, MPORT_INDEX_FILE_SOURCE) == -1 ||
	    asprintf(&hashUrl, "%s/%s/%s/%s", MPORT_BOOTSTRAP_INDEX_URL,  MPORT_ARCH, osrel, MPORT_INDEX_FILE_SOURCE ".sha256") == -1) {
		free(url);
		free(hashUrl);
		return MPORT_ERR_FATAL;
	}

	result = fetch(mport, url, MPORT_INDEX_FILE_COMPRESSED);
	if (result == MPORT_OK && fetch(mport, hashUrl, MPORT_INDEX_FILE_HASH) == MPORT_OK) {
		free(hashUrl);
		char *hash = mport_extract_hash_from_file(MPORT_INDEX_FILE_HASH);

		if (hash == NULL || mport_verify_hash(MPORT_INDEX_FILE_COMPRESSED, hash) == 0) {
			mport_call_msg_cb(mport, "Bootstrap index hash failed verification: %s\n", hash);
        } else {
			mport_decompress_zstd(MPORT_INDEX_FILE_COMPRESSED, mport_index_file_path());
		}
		free(hash);
	} else {
		result = MPORT_ERR_FATAL;
	}

	free(hashUrl);
	free(url);
	free(osrel);

	return result;
}

/* mport_fetch_bundle(mport, filename)
 *
 * Fetch a given bundle from a remote.	If there is no loaded index, then
 * an error is thrown.	The file will be downloaded to the 
 * MPORT_FETCH_STAGING_DIR directory.
 */
int
mport_fetch_bundle(mportInstance *mport, const char *directory, const char *filename)
{
	char **mirrors;
	char **mirrorsPtr;
	char *url;
	const char *fetch_dir;
	char *osrel;
	int mirrorCount = 0;

	MPORT_CHECK_FOR_INDEX(mport, "mport_fetch_bundle()");

	if (filename == NULL || filename[0] == '\0' || strchr(filename, '/') != NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid bundle filename");
	}
	
	if (mport_index_get_mirror_list(mport, &mirrors, &mirrorCount) != MPORT_OK)
		RETURN_CURRENT_ERROR;

	fetch_dir = directory == NULL ? MPORT_FETCH_STAGING_DIR : directory;
 
	mirrorsPtr = mirrors;
	osrel = mport_get_osrelease(mport);
 
	while (mirrorsPtr != NULL) {
		if (*mirrorsPtr == NULL)
			break;
		if (asprintf(&url, "%s/%s/%s/%s", *mirrorsPtr,  MPORT_ARCH, osrel, filename) == -1) {
			free(osrel);
			RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory");
		}

		if (fetch_bundle_to_dir(mport, url, fetch_dir, filename) == MPORT_OK) {
			free(url);
			url = NULL;
			for (int mi = 0; mi < mirrorCount; mi++)
				free(mirrors[mi]);
			return MPORT_OK;
		} 
		
		free(url);
		url = NULL;
		mirrorsPtr++;
	}

	free(osrel);
	for (int mi = 0; mi < mirrorCount; mi++)
		free(mirrors[mi]);

	RETURN_CURRENT_ERROR; 
}


char *
mport_fetch_cves(mportInstance *mport, char *cpe)
{
	int result;
	char *url;
	FILE *local;

	char tmpfile2[] = _PATH_TMP "mport.cve.XXXXXXXX";
	int fd;

	if ((fd = mkstemp(tmpfile2)) == -1) {
		SET_ERRORX(MPORT_ERR_FATAL, "Couldn't make tmp file: %s", strerror(errno));
	}
  
	if (asprintf(&url, "%s/api/cpe/partial-match?includeVersion=true&cpe=%s&startDate=2006-02-28", MPORT_SECURITY_URL, cpe) == -1)
		return NULL;

	local = fdopen(fd, "w");
	if (local == NULL) {
		close(fd);
		free(url);
		SET_ERRORX(MPORT_ERR_FATAL, "Unable to open %s: %s", tmpfile2, strerror(errno));
		return NULL;
	}

	result = fetch_to_file(mport, url, local, false);
	if (fclose(local) == EOF && result == MPORT_OK)
		result = SET_ERRORX(MPORT_ERR_FATAL, "Unable to close %s: %s", tmpfile2, strerror(errno));
	free(url);

    if (result!= MPORT_OK) {
       return NULL;
    }
	return strdup(tmpfile2); // return the file path
}

static int
fetch(mportInstance *mport, const char *url, const char *dest) 
{
	FILE *local = NULL;
	
	if ((local = fopen(dest, "w")) == NULL) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s: %s", dest, strerror(errno));
	}

	int result = fetch_to_file(mport, url, local, true);
	if (fclose(local) == EOF && result == MPORT_OK)
		result = SET_ERRORX(MPORT_ERR_FATAL, "Unable to close %s: %s", dest, strerror(errno));
	if (result == MPORT_ERR_FATAL) {
		unlink(dest);
	}

	return result;
}

static int
fetch_bundle_to_dir(mportInstance *mport, const char *url, const char *directory,
    const char *filename)
{
	FILE *local = NULL;
	int dirfd = -1;
	int fd = -1;
	char tmpname[FILENAME_MAX];
	int result;
	struct stat sb;
	int len;

	if (filename == NULL || filename[0] == '\0' || strchr(filename, '/') != NULL) {
		RETURN_ERROR(MPORT_ERR_FATAL, "Invalid bundle filename");
	}

	if (mkdir(directory, S_IRWXU | S_IRWXG) == -1 && errno != EEXIST) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to create %s: %s", directory, strerror(errno));
	}

	dirfd = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (dirfd == -1) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s: %s", directory, strerror(errno));
	}

	for (int i = 0; i < 100; i++) {
		len = snprintf(tmpname, sizeof(tmpname), ".%s.tmp.%08x", filename,
		    arc4random());
		if (len < 0 || (size_t)len >= sizeof(tmpname)) {
			close(dirfd);
			RETURN_ERROR(MPORT_ERR_FATAL, "Bundle filename is too long");
		}

		fd = openat(dirfd, tmpname,
		    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		if (fd != -1)
			break;
		if (errno != EEXIST) {
			close(dirfd);
			RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s/%s: %s", directory, tmpname, strerror(errno));
		}
	}

	if (fd == -1) {
		close(dirfd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to create temporary bundle file in %s", directory);
	}

	if (fstat(fd, &sb) == -1) {
		close(fd);
		unlinkat(dirfd, tmpname, 0);
		close(dirfd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to stat %s/%s: %s", directory, tmpname, strerror(errno));
	}
	if (!S_ISREG(sb.st_mode) || sb.st_nlink != 1) {
		close(fd);
		unlinkat(dirfd, tmpname, 0);
		close(dirfd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Refusing unsafe bundle destination %s/%s", directory, tmpname);
	}

	local = fdopen(fd, "w");
	if (local == NULL) {
		close(fd);
		unlinkat(dirfd, tmpname, 0);
		close(dirfd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to open %s/%s: %s", directory, tmpname, strerror(errno));
	}
	fd = -1;

	result = fetch_to_file(mport, url, local, true);
	if (fclose(local) == EOF && result == MPORT_OK)
		result = SET_ERRORX(MPORT_ERR_FATAL, "Unable to close %s/%s: %s", directory, tmpname, strerror(errno));
	if (result == MPORT_ERR_FATAL) {
		unlinkat(dirfd, tmpname, 0);
		close(dirfd);
		return result;
	}

	if (renameat(dirfd, tmpname, dirfd, filename) == -1) {
		unlinkat(dirfd, tmpname, 0);
		close(dirfd);
		RETURN_ERRORX(MPORT_ERR_FATAL, "Unable to rename %s/%s to %s/%s: %s",
		    directory, tmpname, directory, filename, strerror(errno));
	}

	close(dirfd);
	return result;
}

/* Copies url into local. The caller owns local and must fclose it. */
static int 
fetch_to_file(mportInstance *mport, const char *url, FILE *local, bool progress) 
{
	FILE *remote = NULL;
	struct url_stat ustat;
	char buffer[BUFFSIZE];
	char *ptr = NULL;
	size_t size;																	
	size_t got = 0;
	size_t wrote;
	char msg[1024];

	if (progress)	
		mport_call_progress_init_cb(mport, "Downloading %s", url);
	
	if ((remote = fetchXGetURL(url, &ustat, "p")) == NULL) {
		RETURN_ERRORX(MPORT_ERR_FATAL, "Fetch error: %s: %s", url, fetchLastErrString);
	}

	char pkg[128];
	char *loc = strrchr(url, '/');
	if (loc!=NULL) {
		strlcpy(pkg, loc + 1, 127);
	} else {
		strlcpy(pkg, url, 127);
	}
	double dlpercent = 0.0;
	
	while (1) {
		size = fread(buffer, 1, BUFFSIZE, remote);
		
		if (size < BUFFSIZE) {
			if (ferror(remote)) {
				fclose(remote);
				RETURN_ERRORX(MPORT_ERR_FATAL, "Fetch error: %s: %s", url, fetchLastErrString);	 
			} else if (feof(remote)) {
				/* do nothing */
			} 
		} 
	
		got += size;

		if (progress) {	
			double val = ((double)got / (double) ustat.size) * 100;
			if (val > dlpercent) {
				dlpercent = val;
				snprintf(msg, 1024, "Downloading %s (%.2f%%)", pkg, dlpercent);
			}
			(mport->progress_step_cb)(got, ustat.size, msg);
		}

		for (ptr = buffer; size > 0; ptr += wrote, size -= wrote) {
			wrote = fwrite(ptr, 1, size, local);
			if (wrote < size) {
				fclose(remote);
				RETURN_ERRORX(MPORT_ERR_FATAL, "Write error %s", strerror(errno));
			}
		}

		if (feof(remote))
			break;
	}
	
	fclose(remote);

	if (progress)
		(mport->progress_free_cb)();

	return MPORT_OK;
}

/**
 * Download a package. Top level, public method.
 *
 * @param mport
 * @param packageName name of the package to fetch
 * @param path returns the path of the saved file. Must be freed on success
 */
int
mport_download(mportInstance *mport, const char *packageName, bool all, bool includeDependencies, char **path) {
	mportIndexEntry **indexEntries = NULL;
	mportIndexEntry *indexEntry = NULL;
	mportDependsEntry **depends = NULL;
	mportDependsEntry **depends_orig = NULL;
	bool existed = true;
	int retryCount = 0;

	if (all) {
		mportIndexEntry **ie2_orig;
		mport_index_list(mport, &ie2_orig);
		mportIndexEntry **ie2 = ie2_orig;

		while (*ie2 != NULL) {
			char *dpath;
			if (mport_download(mport, (*ie2)->pkgname, false, false, &dpath) != MPORT_OK) {
				mport_call_msg_cb(mport, "%s", mport_err_string());
				mport_index_entry_free_vec(ie2_orig);
				return mport_err_code();
			}
			free(dpath);
			ie2++;
		}
		mport_index_entry_free_vec(ie2_orig);
		return (MPORT_OK);
	}

	if (mport_index_select_pkgname(mport, packageName, "Multiple packages match your query.", &indexEntries, &indexEntry) !=
	    MPORT_OK) {
		RETURN_CURRENT_ERROR;
	}
	
	if (indexEntry == NULL) {
		SET_ERRORX(1, "Package %s not found in index.\n", packageName);
		mport_index_entry_free_vec(indexEntries);
		RETURN_CURRENT_ERROR;
	}

	if (indexEntry->bundlefile == NULL) {
		SET_ERRORX(1, "Package %s does not contain a bundle file.\n", packageName);
		mport_index_entry_free_vec(indexEntries);
		RETURN_CURRENT_ERROR;
	}
	
	if (asprintf(path, "%s/%s", mport->outputPath, indexEntry->bundlefile) == -1) {
		mport_index_entry_free_vec(indexEntries);
		indexEntries = NULL;
		indexEntry = NULL;
		SET_ERRORX(1, "%s", "Unable to allocate memory for path.");
		RETURN_CURRENT_ERROR;
	}

	if (includeDependencies) {
		mport_index_depends_list(mport, indexEntry->pkgname, indexEntry->version, &depends_orig);
		depends = depends_orig;

		while (*depends != NULL) {
			char *dpath;
			if (mport_download(mport, (*depends)->d_pkgname, false, includeDependencies, &dpath) != MPORT_OK) {
     			mport_call_msg_cb(mport, "%s", mport_err_string());
     			mport_index_depends_free_vec(depends_orig);
				return mport_err_code();
			}
			free(dpath);
			depends++;
		}
		mport_index_depends_free_vec(depends_orig);
	}

getfile:
	if (!mport_file_exists(*path)) {
		if (mport_fetch_bundle(mport, mport->outputPath, indexEntry->bundlefile) != MPORT_OK) {
			mport_call_msg_cb(mport, "Error fetching package %s, %s", packageName, mport_err_string());
			free(*path);
			path = NULL;
			mport_index_entry_free_vec(indexEntries);
			indexEntries = NULL;
			indexEntry = NULL;
			return mport_err_code();
			
		}
		existed = false;
	}

	if (!mport_verify_hash(*path, indexEntry->hash)) {
		if (existed) {
			if (unlink(*path) == 0)	{
				retryCount++;

				if (retryCount < 2)
					goto getfile;
			}
		} else {
			/* the index might be stale. re-fetch it */
			mport_call_msg_cb(mport, "The index might be stale. Attempting to refresh it.\n");
			mport_index_get(mport);
			retryCount++;
			if (retryCount < 2)
				goto getfile;
		}
		free(*path);
		path = NULL;
		mport_index_entry_free_vec(indexEntries);
		indexEntries = NULL;
		indexEntry = NULL;
		SET_ERRORX(1, "Package %s fails hash verification.", packageName);
		RETURN_CURRENT_ERROR;
	}

	if (!existed)
		mport_call_msg_cb(mport, "Package %s saved as %s\n", packageName, *path);
	else
		mport_call_msg_cb(mport, "Package %s exists at %s\n", packageName, *path);

	mport_index_entry_free_vec(indexEntries);
	indexEntries = NULL;
	indexEntry = NULL;

	return (MPORT_OK);
}
