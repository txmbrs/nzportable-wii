/* TLS over FTE's nonblocking IOS transport; entropy is provisioned separately. */
#include "mbedtls/mbedtls_config.h"
#undef MBEDTLS_NET_C
#undef MBEDTLS_TIMING_C
#undef MBEDTLS_SSL_PROTO_TLS1_3
#define MBEDTLS_NO_PLATFORM_ENTROPY
#define MBEDTLS_ENTROPY_NV_SEED
#define MBEDTLS_PLATFORM_NV_SEED_ALT
#define MBEDTLS_PLATFORM_MS_TIME_ALT

/* WebRTC DTLS resolves its exact trust anchor from the signalled pin. */
#define MBEDTLS_X509_TRUSTED_CERTIFICATE_CALLBACK
