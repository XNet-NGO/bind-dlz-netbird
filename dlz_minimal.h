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

/*
 * This header defines the minimal interface required for a dynamically
 * loadable zone (DLZ) driver.
 */

typedef unsigned int isc_result_t;
typedef int isc_boolean_t;
typedef unsigned int isc_uint32_t;

#define ISC_R_SUCCESS           0
#define ISC_R_NOMEMORY          1
#define ISC_R_TIMEDOUT          2
#define ISC_R_NOTFOUND          23
#define ISC_R_FAILURE           25
#define ISC_R_NOTIMPLEMENTED    27

#define ISC_TRUE  1
#define ISC_FALSE 0

/*
 * Method pointers used by the DLZ module to interact with BIND.
 */
typedef isc_result_t (*dns_sdlz_putrr_t)(void *driverarg, const char *type,
                                         isc_uint32_t ttl, const char *data);

typedef isc_result_t (*dns_sdlz_putnamedrr_t)(void *driverarg, const char *name,
                                                const char *type, isc_uint32_t ttl,
                                                const char *data);

typedef void (*dns_dlz_write_log_t)(int level, const char *fmt, ...);

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

/*
 * The initialization function type.
 */
typedef isc_result_t (*dlz_dlopen_init_t)(isc_uint32_t *version);

/*
 * The creation function type.
 */
typedef isc_result_t (*dlz_create_t)(const char *dlzname, unsigned int argc,
                                     char *argv[], void **dbdata,
                                     const char **helper_name);

/*
 * The destruction function type.
 */
typedef void (*dlz_destroy_t)(void *dbdata);

/*
 * The findzonedb function type.
 */
typedef isc_result_t (*dlz_findzonedb_t)(void *dbdata, const char *name,
                                         void **user_data, void **driver_data);

/*
 * The lookup function type.
 */
typedef isc_result_t (*dlz_lookup_t)(const char *zone, const char *name,
                                     void *driver_data,
                                     dns_sdlz_putrr_t putrr,
                                     dns_sdlz_putnamedrr_t putnamedrr,
                                     void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* DLZ_MINIMAL_H */
