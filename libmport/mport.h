/*-
 * SPDX-License-Identifier: BSD-2-Clause
 * 
 * Copyright (c) 2013, 2014, 2021, 2024 Lucas Holt
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
 *
 */

#ifndef _MPORT_H_
#define _MPORT_H_

#ifdef __linux__
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <archive.h>
#include <paths.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/param.h>
#include "tllist.h"

#ifndef MAXLOGNAME
#define MAXLOGNAME 32
#endif

typedef void (*mport_msg_cb)(const char *);
typedef void (*mport_progress_init_cb)(const char *);
typedef void (*mport_progress_step_cb)(int, int, const char *);
typedef void (*mport_progress_free_cb)(void);
typedef int (*mport_confirm_cb)(const char *, const char *, const char *, int);

typedef tll(char *) stringlist_t;

/* Mport Instance (an installed copy of the mport system) */
#define MPORT_INST_HAVE_INDEX 1
#define MPORT_LOCAL_PKG_PATH "/var/db/mport/downloads"

enum _Verbosity{
    MPORT_VQUIET,
    MPORT_VBRIEF,
    MPORT_VNORMAL,
    MPORT_VVERBOSE
};
typedef enum _Verbosity mportVerbosity;
mportVerbosity mport_verbosity(bool quiet, bool verbose, bool brief);

typedef struct {
  int flags;
  sqlite3 *db;
  char *root;
  int rootfd; /* Root directory file descriptor */
  char *outputPath; /* Download directory */
  bool noIndex; /* Do not fetch mport index */
  bool offline; /* Installing packages from local files, etc. */
  mportVerbosity verbosity;
  bool force;
  mport_msg_cb msg_cb;
  mport_progress_init_cb progress_init_cb;
  mport_progress_step_cb progress_step_cb;
  mport_progress_free_cb progress_free_cb;
  mport_confirm_cb confirm_cb;
} mportInstance;

mportInstance * mport_instance_new(void);
int mport_instance_init(mportInstance *, const char *, const char *, bool noIndex, mportVerbosity verbosity);
int mport_instance_free(mportInstance *);

/* Run the callbacks. will display messages, etc */
int mport_call_msg_cb(mportInstance *, const char *, ...);
int mport_call_progress_init_cb(mportInstance *, const char *, ...);
bool mport_call_confirm_cb(mportInstance *mport, const char *msg, const char *yes, const char *no, int def);

/* setup your custom callbacks */
void mport_set_msg_cb(mportInstance *, mport_msg_cb);
void mport_set_progress_init_cb(mportInstance *, mport_progress_init_cb);
void mport_set_progress_step_cb(mportInstance *, mport_progress_step_cb);
void mport_set_progress_free_cb(mportInstance *, mport_progress_free_cb);
void mport_set_confirm_cb(mportInstance *, mport_confirm_cb);

/* default cbs for terminal use. For graphical apps, you want to make your own */
void mport_default_msg_cb(const char *);
int mport_default_confirm_cb(const char *, const char *, const char *, int);
void mport_default_progress_init_cb(const char *);
void mport_default_progress_step_cb(int, int, const char *);
void mport_default_progress_free_cb(void);

enum _AssetListEntryType {
    ASSET_INVALID, ASSET_FILE, ASSET_CWD, ASSET_CHMOD, ASSET_CHOWN, ASSET_CHGRP,
    ASSET_COMMENT, ASSET_IGNORE, ASSET_NAME, ASSET_EXEC, ASSET_UNEXEC,
    ASSET_SRC, ASSET_DISPLY, ASSET_PKGDEP, ASSET_CONFLICTS, ASSET_MTREE,
    ASSET_DIRRM, ASSET_DIRRMTRY, ASSET_IGNORE_INST, ASSET_OPTION, ASSET_ORIGIN,
    ASSET_DEPORIGIN, ASSET_NOINST, ASSET_DISPLAY, ASSET_DIR,
    ASSET_SAMPLE, ASSET_SHELL,
    ASSET_PREEXEC, ASSET_PREUNEXEC, ASSET_POSTEXEC, ASSET_POSTUNEXEC,
    ASSET_FILE_OWNER_MODE, ASSET_DIR_OWNER_MODE, 
    ASSET_SAMPLE_OWNER_MODE, ASSET_LDCONFIG, ASSET_LDCONFIG_LINUX,
    ASSET_RMEMPTY, ASSET_GLIB_SCHEMAS, ASSET_KLD, ASSET_DESKTOP_FILE_UTILS,
    ASSET_INFO, ASSET_TOUCH
};

typedef enum _AssetListEntryType mportAssetListEntryType;

struct _AssetListEntry {
	mportAssetListEntryType type;
	char checksum[65];
	char owner[MAXLOGNAME];
	char group[MAXLOGNAME * 2]; /* no standard for this, just guess */
	char mode[5];
	char *data;

	STAILQ_ENTRY(_AssetListEntry) next;
};

STAILQ_HEAD(_AssetList, _AssetListEntry);

typedef struct _AssetList mportAssetList;
typedef struct _AssetListEntry mportAssetListEntry;

mportAssetList* mport_assetlist_new(void);
void mport_assetlist_free(mportAssetList *);
int mport_parse_plistfile(FILE *, mportAssetList *);

enum _Automatic{
    MPORT_EXPLICIT, /* explicitly installed */
    MPORT_AUTOMATIC /* Automatically installed dependency */
};
typedef enum _Automatic mportAutomatic;


enum _Action{
    MPORT_ACTION_INSTALL,
    MPORT_ACTION_UPGRADE,
    MPORT_ACTION_UPDATE,
    MPORT_ACTION_DELETE,
    MPORT_ACTION_UNKNOWN
};
typedef enum _Action mportAction;

enum _Type{
    MPORT_TYPE_APP, 
    MPORT_TYPE_SYSTEM
};
typedef enum _Type mportType;

#define MPORT_NUM_LUA_SCRIPTS 5

/* Package Meta-data structure */
typedef struct {
    char *name;
    char *version;
    char *lang;
    char *options;
    char *comment;
    char *desc;
    char *prefix;
    char *origin;
    char **categories;
    size_t categories_count;
    char *os_release;
    char *cpe;
    int locked;
    char *deprecated;
    time_t expiration_date;
    int no_provide_shlib;
    char *flavor;
    mportAutomatic automatic;
    time_t install_date;
    mportAction action; // not populated from package table
    mportType type;
    int64_t flatsize;
    stringlist_t  lua_scripts[MPORT_NUM_LUA_SCRIPTS]; // not populated from package table
    stringlist_t  conflicts; // not populated from package table
    // TODO: conflicts should be a structure
} __attribute__ ((aligned (16)))  mportPackageMeta;

int mport_asset_get_assetlist(mportInstance *, mportPackageMeta *, mportAssetList **);
int mport_asset_get_package_from_file_path(mportInstance *, const char *, mportPackageMeta **);

mportPackageMeta * mport_pkgmeta_new(void);
void mport_pkgmeta_free(mportPackageMeta *);
void mport_pkgmeta_vec_free(mportPackageMeta **);
int mport_pkgmeta_search_master(mportInstance *, mportPackageMeta ***, const char *, ...);
int mport_pkgmeta_list(mportInstance *mport, mportPackageMeta ***ref);
int mport_pkgmeta_list_locked(mportInstance *mport, mportPackageMeta ***ref);
int mport_pkgmeta_get_downdepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);
int mport_pkgmeta_get_updepends(mportInstance *, mportPackageMeta *, mportPackageMeta ***);


/* index */
typedef struct {
  char *pkgname;
  char *version;
  char *comment;
  char *bundlefile;
  char *license;
  char *hash;
  mportType type;
} mportIndexEntry;

typedef struct {
  char port[128];
  char moved_to[128];
  char why[128];
  char date[32];

  char pkgname[128];
  char moved_to_pkgname[128];
} mportIndexMovedEntry;

typedef struct {
  char country[5];
  char url[256];
} mportMirrorEntry;

int mport_index_load(mportInstance *);
int mport_index_get(mportInstance *);
int mport_index_check(mportInstance *, mportPackageMeta *);
int mport_index_list(mportInstance *, mportIndexEntry ***);
int mport_index_lookup_pkgname(mportInstance *, const char *, mportIndexEntry ***);
int mport_index_search(mportInstance *, mportIndexEntry ***, const char *, ...);
int mport_index_search_term(mportInstance *, mportIndexEntry ***, char *);
void mport_index_entry_free_vec(mportIndexEntry **);
void mport_index_entry_free(mportIndexEntry *);

int mport_index_print_mirror_list(mportInstance *);
int mport_index_mirror_list(mportInstance *, mportMirrorEntry ***);
void mport_index_mirror_entry_free_vec(mportMirrorEntry **e);
void mport_index_mirror_entry_free(mportMirrorEntry *);

int mport_moved_lookup(mportInstance *, const char *, mportIndexMovedEntry ***);

/* Index Depends */

typedef struct {
  char *pkgname;
  char *version;
  char *d_pkgname;
  char *d_version;
} mportDependsEntry;

int mport_index_depends_list(mportInstance *, const char *, const char *, mportDependsEntry ***);
void mport_index_depends_free_vec(mportDependsEntry **);
void mport_index_depends_free(mportDependsEntry *);

/* Info */
char * mport_info(mportInstance *mport, const char *packageName);

/* Package creation */

typedef enum {
    PKG_MESSAGE_ALWAYS = 0,
    PKG_MESSAGE_INSTALL,
    PKG_MESSAGE_REMOVE,
    PKG_MESSAGE_UPGRADE,
} pkg_message_t;

typedef struct package_message {
    char			*str;
    char			*minimum_version;
    char			*maximum_version;
    pkg_message_t		 type;
    struct package_message *next, *prev;
} mportPackageMessage;

typedef struct {
  char pkg_filename[FILENAME_MAX];
  char sourcedir[FILENAME_MAX];
  char **depends;
  size_t depends_count;
  char *mtree; 
  stringlist_t conflicts;
  char *pkginstall;
  char *pkgdeinstall;
  char *luapkgpreinstall;
  char *luapkgpredeinstall;
  char *luapkgpostinstall;
  char *luapkgpostdeinstall;
  char *pkgmessage;
  bool is_backup;
} mportCreateExtras;  

mportCreateExtras * mport_createextras_new(void);
void mport_createextras_free(mportCreateExtras *);

int mport_create_primative(mportInstance *, mportAssetList *, mportPackageMeta *, mportCreateExtras *);

/* Merge primative */
int mport_merge_primative(mportInstance *mport, const char **, const char *);

/* Package installation */
int mport_install(mportInstance *, const char *, const char *, const char *, mportAutomatic);
int mport_install_single(mportInstance *mport, const char *pkgname, const char *version, const char *prefix, mportAutomatic automatic);
int mport_install_primative(mportInstance *, const char *, const char *, mportAutomatic);

/* package updating */
int mport_update(mportInstance *, const char *);
int mport_update_primative(mportInstance *, const char *);

/* package upgrade */
int mport_upgrade(mportInstance *);

/* Package deletion */
int mport_delete_primative(mportInstance *, mportPackageMeta *, int);

int mport_autoremove(mportInstance *);

/* package verify */
int mport_verify_package(mportInstance *, mportPackageMeta *);
int mport_recompute_checksums(mportInstance *, mportPackageMeta *);

/* version comparing */
int mport_version_cmp(const char *, const char *);

/* fetch XXX: This should become private */
int mport_fetch_bundle(mportInstance *, const char *, const char *);
int mport_download(mportInstance *, const char *, bool, bool, char **);

/* Auditing for CVEs */
char * mport_audit(mportInstance *, const char *, bool);

/* Errors */
int mport_err_code(void);
const char * mport_err_string(void);


#define MPORT_OK			    0
#define MPORT_ERR_FATAL 		1
#define MPORT_ERR_WARN			2

/* Clean */
int mport_clean_database(mportInstance *);
int mport_clean_oldpackages(mportInstance *);
int mport_clean_oldmtree(mportInstance *);
int mport_clean_tempfiles(mportInstance *);

/* Setting */
char * mport_setting_get(mportInstance *, const char *);
int mport_setting_set(mportInstance *, const char *, const char *);
char ** mport_setting_list(mportInstance *);

/* Utils */
char * mport_purl_uri(mportPackageMeta *packs);
void mport_parselist(char *, char ***, size_t *);
void mport_parselist_tll(char *, stringlist_t *);
int mport_verify_hash(const char *, const char *);
int mport_file_exists(const char *);
char * mport_version(mportInstance *);
char * mport_version_short(mportInstance *);
char * mport_get_osrelease(mportInstance *);
int mport_drop_privileges(void);
char * mport_string_replace(const char *str, const char *old, const char *new);
bool mport_is_elf_file(const char *file);
bool mport_is_statically_linked(const char *file);

/* Locks */
enum _LockState {
	MPORT_UNLOCKED, MPORT_LOCKED
};

typedef enum _LockState mportLockState;

int mport_lock_lock(mportInstance *, mportPackageMeta *);
int mport_lock_unlock(mportInstance *, mportPackageMeta *);
int mport_lock_islocked(mportPackageMeta *);

/* Statistics */
typedef struct {
    unsigned int pkg_installed;
    unsigned int pkg_available;
    int64_t pkg_installed_size;
    /* TODO: int64_t pkg_available_size; */
} mportStats;

int mport_stats(mportInstance *, mportStats **);
int mport_stats_free(mportStats *);
mportStats * mport_stats_new(void);

/* Import/Export */
int mport_import(mportInstance*,  char *);
int mport_export(mportInstance*, char *);

/* Ping */
long ping(char *hostname);

/* List Print */
typedef struct {
	bool verbose;
	bool origin;
	bool update;
	bool locks;
  bool prime;
} mportListPrint;

int mport_list_print(mportInstance *, mportListPrint *);

#endif /* ! defined _MPORT_H */
