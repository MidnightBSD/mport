/*-
 * Copyright (c) 2013-2015, 2021 Lucas Holt
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

#include "mport.h"
#include "mport_private.h"

#include <sys/stat.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <archive_entry.h>
#include <ucl.h>

int
mport_display_pkg_msg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg)
{
    mportPackageMessage packageMessage;
    packageMessage.type = PKG_MESSAGE_ALWAYS;
    if (mport_load_pkg_msg(mport, bundle, pkg, &packageMessage) != MPORT_OK) {
        RETURN_CURRENT_ERROR;
    }

    if (packageMessage.type == PKG_MESSAGE_INSTALL || packageMessage.type == PKG_MESSAGE_ALWAYS) {
        if (packageMessage.str != NULL && packageMessage.str[0] != '\0')
            mport_call_msg_cb(mport, "%s", packageMessage.str);
    }

    free(packageMessage.str);

    return MPORT_OK;
}


int
mport_load_pkg_msg(mportInstance *mport, mportBundleRead *bundle, mportPackageMeta *pkg, mportPackageMessage *packageMessage)
{
    char filename[FILENAME_MAX];
    char *buf;
    struct stat st;
    FILE *file;
    struct ucl_parser *parser;
    ucl_object_t *obj;

    (void) snprintf(filename, FILENAME_MAX, "%s/%s/%s-%s/%s", bundle->tmpdir, MPORT_STUB_INFRA_DIR, pkg->name,
                    pkg->version, MPORT_MESSAGE_FILE);

    if (stat(filename, &st) == -1) {
        /* if we couldn't stat the file, we assume there isn't a pkg-msg */
        return MPORT_OK;
    }

    if ((file = fopen(filename, "re")) == NULL)
        RETURN_ERRORX(MPORT_ERR_FATAL, "Couldn't open %s: %s", filename, strerror(errno));

    if ((buf = (char *) calloc((size_t)(st.st_size + 1), sizeof(char))) == NULL)
        RETURN_ERROR(MPORT_ERR_FATAL, "Out of memory.");

    if (fread(buf, sizeof(char), (size_t) st.st_size, file) != (size_t) st.st_size) {
        free(buf);
        RETURN_ERRORX(MPORT_ERR_FATAL, "Read error: %s", strerror(errno));
    }

    buf[st.st_size] = '\0';

    if (buf[0] == '[') {
        parser = ucl_parser_new(0);
        // remove leading/trailing array entries
        buf[0] = ' ';
        buf[st.st_size-1] = '\0';

        if (ucl_parser_add_chunk(parser, (const unsigned char*)buf, st.st_size)) {
            obj = ucl_parser_get_object(parser);
            ucl_parser_free(parser);
            free(buf);

            packageMessage = mport_pkg_message_from_ucl(mport, obj, packageMessage);
            ucl_object_unref(obj);

            return packageMessage == NULL ? MPORT_ERR_FATAL : MPORT_OK;
        }

        ucl_parser_free (parser);
    } else {
        packageMessage->str = strdup(buf);
        packageMessage->type = PKG_MESSAGE_ALWAYS;
        free(buf);
    }

    return MPORT_OK;
}

mportPackageMessage *
mport_pkg_message_from_ucl(mportInstance *mport, const ucl_object_t *obj, mportPackageMessage *msg)
{
    const ucl_object_t *enhanced;

    if (ucl_object_type(obj) == UCL_STRING) {
        msg->str = strdup(ucl_object_tostring(obj));
    } else if (ucl_object_type(obj) == UCL_OBJECT) {
        /* New format of pkg message */
        enhanced = ucl_object_find_key(obj, "message");
        if (enhanced == NULL || ucl_object_type(enhanced) != UCL_STRING) {
            return NULL;
        }
        msg->str = strdup(ucl_object_tostring(enhanced));

        enhanced = ucl_object_find_key(obj, "minimum_version");
        if (enhanced != NULL && ucl_object_type(enhanced) == UCL_STRING) {
            msg->minimum_version = strdup(ucl_object_tostring(enhanced));
        }
    }

    return msg;
}
