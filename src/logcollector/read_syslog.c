/* Copyright (C) 2009 Trend Micro Inc.
 * All right reserved.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

/* Read the syslog */

#include "shared.h"
#include "logcollector.h"


/* Read syslog files */
void *read_syslog(int pos, int *rc, int drop_it)
{
    int __ms = 0;
    int __ms_reported = 0;
    char *p;
    char str[OS_MAXSTR + 1];
    fpos_t fp_pos;
    int lines = 0;

    str[OS_MAXSTR] = '\0';
    *rc = 0;

    /* Get initial file location */
    fgetpos(logff[pos].fp, &fp_pos);

    while (fgets(str, OS_MAXSTR - OS_LOG_HEADER, logff[pos].fp) != NULL && (!maximum_lines || lines < maximum_lines)) {

        lines++;
        /* Get the last occurrence of \n */
        if ((p = strrchr(str, '\n')) != NULL) {
            *p = '\0';
        }

        /* If we didn't get the new line, because the
         * size is large, send what we got so far.
         */
        else if (strlen(str) >= (OS_MAXSTR - OS_LOG_HEADER - 2)) {
            /* Message size > maximum allowed */
            __ms = 1;
        } else {
            /* Message not complete. Return. */
            mdebug1("Message not complete from '%s'. Trying again: '%.*s'%s", logff[pos].file, sample_log_length, str, strlen(str) > (size_t)sample_log_length ? "..." : "");
            fsetpos(logff[pos].fp, &fp_pos);
            break;
        }

#ifdef WIN32
        if ((p = strrchr(str, '\r')) != NULL) {
            *p = '\0';
        }

        /* Look for empty string (only on Windows) */
        if (strlen(str) <= 2) {
            fgetpos(logff[pos].fp, &fp_pos);
            continue;
        }

        /* Windows can have comment on their logs */
        if (str[0] == '#') {
            fgetpos(logff[pos].fp, &fp_pos);
            continue;
        }
#endif

        mdebug2("Reading syslog message: '%.*s'%s", sample_log_length, str, strlen(str) > (size_t)sample_log_length ? "..." : "");

        /* Send message to queue */
        if (drop_it == 0) {
            if (SendMSGtoSCK(logr_queue, str, logff[pos].file,
                        LOCALFILE_MQ, logff[pos].log_target) < 0) {
                merror(QUEUE_SEND);
                if ((logr_queue = StartMQ(DEFAULTQPATH, WRITE)) < 0) {
                    merror_exit(QUEUE_FATAL, DEFAULTQPATH);
                }
            }
        }
        /* Incorrect message size */
        if (__ms) {
            // strlen(str) >= (OS_MAXSTR - OS_LOG_HEADER - 2)

            if (!__ms_reported) {
                merror("Large message size from file '%s' (length = %zu): '%.*s'...", logff[pos].file, strlen(str), sample_log_length, str);
                __ms_reported = 1;
            } else {
                mdebug2("Large message size from file '%s' (length = %zu): '%.*s'...", logff[pos].file, strlen(str), sample_log_length, str);
            }

            while (fgets(str, OS_MAXSTR - 2, logff[pos].fp) != NULL) {
                /* Get the last occurrence of \n */
                if (strrchr(str, '\n') != NULL) {
                    break;
                }
            }
            __ms = 0;
        }
        fgetpos(logff[pos].fp, &fp_pos);
        continue;
    }

    mdebug2("Read %d lines from %s", lines, logff[pos].file);
    return (NULL);
}
