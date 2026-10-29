/* DTLS 1.2 for FTE ICE/WebRTC data channels. GPL-2.0-or-later.
 * WebRTC certificates are authenticated against the fingerprint supplied by
 * FTE's signalling layer. Non-fingerprint peers require ordinary CA/name trust. */
#include "quakedef.h"
#include "netinc.h"
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <mbedtls/platform_util.h>
#include <time.h>

int WiiOnline_Random(void *ctx,unsigned char *out,size_t length);
typedef struct dtls_packet_s {
    struct dtls_packet_s *next;
    size_t length;
    unsigned char bytes[];
} dtls_packet_t;
typedef struct {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt cert,roots;
    mbedtls_pk_context key;
    dtlspeercred_t_unused_placeholder *unused;
} unused_type_placeholder;
