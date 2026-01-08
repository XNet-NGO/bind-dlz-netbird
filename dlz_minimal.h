/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef DLZ_MINIMAL_H
#define DLZ_MINIMAL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * This header defines the minimal interface required for a dynamically
 * loadable zone (DLZ) driver for BIND 9.18+.
 */

typedef unsigned int isc_result_t;
typedef uint32_t dns_ttl_t;

#define ISC_R_SUCCESS           0
#define ISC_R_NOMEMORY          1
#define ISC_R_TIMEDOUT          2
#define ISC_R_NOTFOUND          23
#define ISC_R_FAILURE           25
#define ISC_R_NOTIMPLEMENTED    27

#define ISC_TRUE  1
#define ISC_FALSE 0

/*
 * Log levels
 */
#define ISC_LOG_INFO    -1
#define ISC_LOG_NOTICE  -2
#define ISC_LOG_WARNING -3
#define ISC_LOG_ERROR   -4
#define ISC_LOG_CRITICAL -5

/*
 * DLZ Driver Constants
 */
#define DLZ_DLOPEN_VERSION 3
#define DLZ_DLOPEN_AGE     0

/*
 * Opaque types from BIND - we don't need their internals
 */
typedef struct dns_sdlzlookup dns_sdlzlookup_t;
typedef struct dns_sdlzallnodes dns_sdlzallnodes_t;
typedef struct dns_clientinfomethods dns_clientinfomethods_t;
typedef struct dns_clientinfo dns_clientinfo_t;

/*
 * The dns_sdlz_putrr function - provided by BIND, we declare it here
 */
extern isc_result_t dns_sdlz_putrr(dns_sdlzlookup_t *lookup, const char *type,
                                    dns_ttl_t ttl, const char *data);

/*
 * The dns_sdlz_putsoa function - for authority queries
 */
extern isc_result_t dns_sdlz_putsoa(dns_sdlzlookup_t *lookup, const char *mname,
                                     const char *rname, uint32_t serial);

/*
 * Logging helper type (not used in dlopen drivers typically)
 */
typedef void (*dns_dlz_write_log_t)(int level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DLZ_MINIMAL_H */
