/* DTLS 1.2 for FTE ICE/WebRTC data channels. GPL-2.0-or-later.
 * WebRTC certificates are authenticated against the fingerprint supplied by
 * FTE's signalling layer. Non-fingerprint peers require ordinary CA/name trust. */
#include "quakedef.h"
#include "netinc.h"
#include <mbedtls/ssl.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/error.h>
#include <mbedtls/sha256.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/platform.h>
#include <time.h>
#include "wii_dtls_queue.h"

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
    const hashfunc_t *peer_hash;
    unsigned char peer_digest[DIGEST_MAXSIZE];
    void *push_context;
    neterr_t (*push)(void *,const qbyte *,size_t);
    dtls_packet_t *head,*tail;
    unsigned packets,ingress_dropped;
    unsigned trace_rx,trace_plain,trace_tx,trace_tx_errors,trace_last,trace_input_time;
    mbedtls_ms_time_t timer_start;
    uint32_t timer_intermediate,timer_final;
    wii_dtls_queue_t outgoing;
    qboolean established,failed,peer_matched;
} wii_dtls_t;

static void set_timer(void *ctx,uint32_t intermediate,uint32_t final)
{
    wii_dtls_t *d=ctx;
    d->timer_start=mbedtls_ms_time();
    d->timer_intermediate=intermediate; d->timer_final=final;
}
static int get_timer(void *ctx)
{
    wii_dtls_t *d=ctx;
    if (!d->timer_final) return -1;
    mbedtls_ms_time_t elapsed=mbedtls_ms_time()-d->timer_start;
    return elapsed>=d->timer_final ? 2:elapsed>=d->timer_intermediate ? 1:0;
}
static int push_packet(void *ctx,const unsigned char *data,size_t size)
{
    wii_dtls_t *d=ctx;
    neterr_t result=d->push(d->push_context,data,size);
    ++d->trace_tx;
    if (result!=NETERR_SENT) ++d->trace_tx_errors;
    if (result==NETERR_SENT) return (int)size;
    if (result==NETERR_CLOGGED) return MBEDTLS_ERR_SSL_WANT_WRITE;
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}
static int pull_packet(void *ctx,unsigned char *data,size_t capacity)
{
    wii_dtls_t *d=ctx;
    dtls_packet_t *packet=d->head;
    if (!packet) return MBEDTLS_ERR_SSL_WANT_READ;
    d->head=packet->next; if (!d->head) d->tail=NULL;
    --d->packets;
    int length=(int)packet->length;
    if (packet->length>capacity) length=MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
    else memcpy(data,packet->bytes,packet->length);
    BZ_Free(packet);
    return length;
}
static qboolean matches_peer_pin(wii_dtls_t *d,const mbedtls_x509_crt *certificate)
{
    unsigned char digest[DIGEST_MAXSIZE];
    size_t count;
    if (!d->peer_hash || !certificate || !certificate->raw.p) return false;
    count=CalcHash(d->peer_hash,digest,sizeof(digest),certificate->raw.p,certificate->raw.len);
    return count==d->peer_hash->digestsize && !memcmp(digest,d->peer_digest,count);
}
/* Only the exact certificate named by authenticated signalling may become
 * a trust anchor. mbedTLS owns and frees the returned parsed copy. This keeps
 * VERIFY_REQUIRED, including its missing-client-certificate rejection. */
static int pinned_trust_anchor(void *ctx,const mbedtls_x509_crt *child,mbedtls_x509_crt **anchors)
{
    wii_dtls_t *d=ctx;
    mbedtls_x509_crt *copy;
    int result;
    *anchors=NULL;
    if (!matches_peer_pin(d,child)) return 0;
    copy=mbedtls_calloc(1,sizeof(*copy));
    if (!copy) return MBEDTLS_ERR_X509_ALLOC_FAILED;
    mbedtls_x509_crt_init(copy);
    result=mbedtls_x509_crt_parse_der(copy,child->raw.p,child->raw.len);
    if (result) {
        mbedtls_x509_crt_free(copy); mbedtls_free(copy);
        return result;
    }
    *anchors=copy;
    return 0;
}
static int verify_fingerprint(void *ctx,mbedtls_x509_crt *certificate,int depth,uint32_t *flags)
{
    wii_dtls_t *d=ctx;
    if (!d->peer_hash) return 0; /* Preserve normal CA/name validation flags. */
    if (depth) { *flags=0; return 0; }
    d->peer_matched=matches_peer_pin(d,certificate);
    if (!d->peer_matched) {
        *flags=MBEDTLS_X509_BADCERT_OTHER;
        Con_Printf("DTLS: peer fingerprint mismatch; rejecting certificate\n");
        return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    }
    /* The pinned DER is the identity for WebRTC, including self-signed or
     * expired X.509 wrappers. DTLS still proves possession of its private key. */
    *flags=0;
    Con_Printf("DTLS: peer fingerprint verified\n");
    return 0;
}
/* Only the public WebRTC application label is selected: c-webrtc carries
 * additional media confidentiality promises which this client does not make.
 * This static array must outlive every mbedTLS configuration using it. */
static const char *webrtc_protocols[]={"webrtc",NULL};

static int dtls_result(wii_dtls_t *d,int result)
{
    char description[160];
    if (result==MBEDTLS_ERR_SSL_WANT_READ || result==MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
    if (!d->failed) {
        mbedtls_strerror(result,description,sizeof(description));
        Con_Printf("DTLS: %s; handshake state %d, verification %08x\n",description,
            d->ssl.MBEDTLS_PRIVATE(state),(unsigned)mbedtls_ssl_get_verify_result(&d->ssl));
        /* mbedTLS has already decoded/decrypted this record. Do not log key
         * material or raw handshake packets, only the two-byte TLS alert. */
        if (result==MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE &&
            d->ssl.MBEDTLS_PRIVATE(in_msgtype)==MBEDTLS_SSL_MSG_ALERT &&
            d->ssl.MBEDTLS_PRIVATE(in_msglen)==2 && d->ssl.MBEDTLS_PRIVATE(in_msg))
            Con_Printf("DTLS: peer alert level %u, description %u\n",
                d->ssl.MBEDTLS_PRIVATE(in_msg)[0],d->ssl.MBEDTLS_PRIVATE(in_msg)[1]);
    }
    d->failed=true;
    return -1;
}
static int handshake(wii_dtls_t *d)
{
    if (d->failed) return -1;
    if (d->established) return 1;
    int result=mbedtls_ssl_handshake(&d->ssl);
    if (result) return dtls_result(d,result);
    if (mbedtls_ssl_get_verify_result(&d->ssl)) return dtls_result(d,MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
    if (d->peer_hash && (!d->peer_matched || !matches_peer_pin(d,mbedtls_ssl_get_peer_cert(&d->ssl))))
        return dtls_result(d,MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
    d->established=true;
    Con_Printf("DTLS: authenticated peer (%s); cipher %s; ALPN %s\n",
        d->peer_hash ? "signalled fingerprint":"CA/name",mbedtls_ssl_get_ciphersuite(&d->ssl),
        mbedtls_ssl_get_alpn_protocol(&d->ssl) ? mbedtls_ssl_get_alpn_protocol(&d->ssl):"none");
    return 1;
}
static void destroy_context(void *ctx)
{
    wii_dtls_t *d=ctx;
    if (!d) return;
    while (d->head) { dtls_packet_t *next=d->head->next; BZ_Free(d->head); d->head=next; }
    mbedtls_ssl_free(&d->ssl); mbedtls_ssl_config_free(&d->config);
    mbedtls_x509_crt_free(&d->cert); mbedtls_x509_crt_free(&d->roots); mbedtls_pk_free(&d->key);
    mbedtls_platform_zeroize(d,sizeof(*d)); BZ_Free(d);
}
static void *create_context(const dtlscred_t *credentials,void *cbctx,
    neterr_t (*push)(void *,const qbyte *,size_t),qboolean server)
{
    int result;
    if (!credentials || !push || !credentials->local.cert || !credentials->local.key) return NULL;
    if (!credentials->peer.hash && (!credentials->peer.name || !*credentials->peer.name)) return NULL;
    if (credentials->peer.hash && (!credentials->peer.hash->digestsize || credentials->peer.hash->digestsize>DIGEST_MAXSIZE)) return NULL;
    wii_dtls_t *d=BZ_Malloc(sizeof(*d)); memset(d,0,sizeof(*d));
    mbedtls_ssl_init(&d->ssl); mbedtls_ssl_config_init(&d->config);
    mbedtls_x509_crt_init(&d->cert); mbedtls_x509_crt_init(&d->roots); mbedtls_pk_init(&d->key);
    d->push_context=cbctx; d->push=push;
    d->peer_hash=credentials->peer.hash;
    if (d->peer_hash) memcpy(d->peer_digest,credentials->peer.digest,d->peer_hash->digestsize);
    result=mbedtls_x509_crt_parse_der(&d->cert,credentials->local.cert,credentials->local.certsize);
    if (result) goto failed;
    /* Signalling and the DTLS record must identify the very same DER cert.
     * Independently check the engine's SHA-256 before accepting credentials;
     * a broken endian path must fail locally, not advertise a false identity. */
    {
        unsigned char engine_digest[32],tls_digest[32];
        result=mbedtls_sha256(d->cert.raw.p,d->cert.raw.len,tls_digest,0);
        if (result) goto failed;
        if (CalcHash(&hash_sha2_256,engine_digest,sizeof(engine_digest),
                credentials->local.cert,credentials->local.certsize)!=sizeof(engine_digest) ||
            memcmp(engine_digest,tls_digest,sizeof(engine_digest))) {
            Con_Printf("DTLS: local certificate fingerprint mismatch; refusing handshake\n");
            result=MBEDTLS_ERR_SSL_BAD_CERTIFICATE;
            goto failed;
        }
        Con_Printf("DTLS: local SHA-256 fingerprint matches mbedTLS\n");
    }
    result=mbedtls_pk_parse_key(&d->key,credentials->local.key,credentials->local.keysize,NULL,0,WiiOnline_Random,NULL);
    if (result) goto failed;
    result=mbedtls_ssl_config_defaults(&d->config,server ? MBEDTLS_SSL_IS_SERVER:MBEDTLS_SSL_IS_CLIENT,
        MBEDTLS_SSL_TRANSPORT_DATAGRAM,MBEDTLS_SSL_PRESET_DEFAULT);
    if (result) goto failed;
    if (d->peer_hash) {
        result=mbedtls_ssl_conf_alpn_protocols(&d->config,webrtc_protocols);
        if (result) goto failed;
        mbedtls_ssl_conf_ca_cb(&d->config,pinned_trust_anchor,d);
    }
    mbedtls_ssl_conf_rng(&d->config,WiiOnline_Random,NULL);
    mbedtls_ssl_conf_authmode(&d->config,MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_verify(&d->config,verify_fingerprint,d);
    mbedtls_ssl_conf_handshake_timeout(&d->config,1000,60000);
    /* FTE ICE has already authenticated the selected peer/address using STUN.
     * This provider creates peer-bound sessions, not a public DTLS listener. */
    if (server) mbedtls_ssl_conf_dtls_cookies(&d->config,NULL,NULL,NULL);
    if (!d->peer_hash) {
        result=mbedtls_x509_crt_parse_file(&d->roots,"cacert.pem");
        if (result) goto failed;
        mbedtls_ssl_conf_ca_chain(&d->config,&d->roots,NULL);
    }
    result=mbedtls_ssl_conf_own_cert(&d->config,&d->cert,&d->key);
    if (result) goto failed;
    result=mbedtls_ssl_setup(&d->ssl,&d->config);
    if (result) goto failed;
    /* Explicit NULL declares a non-DNS identity to mbedTLS 3.6.7. The
     * pinned certificate remains mandatory; ordinary TLS keeps its name. */
    result=mbedtls_ssl_set_hostname(&d->ssl,d->peer_hash ? NULL:credentials->peer.name);
    if (result) goto failed;
    mbedtls_ssl_set_bio(&d->ssl,d,push_packet,pull_packet,NULL);
    mbedtls_ssl_set_timer_cb(&d->ssl,d,set_timer,get_timer);
    mbedtls_ssl_set_mtu(&d->ssl,1200);
    Con_Printf("DTLS: starting %s; peer authentication %s; ALPN %s\n",
        server ? "server/passive":"client/active",
        d->peer_hash ? "signalled fingerprint":"CA/name",d->peer_hash ? "webrtc":"none");
    return d;
failed:
    dtls_result(d,result); destroy_context(d); return NULL;
}
static int write_queued(void *ctx,const unsigned char *data,size_t length)
{
    wii_dtls_t *d=ctx;
    int result=mbedtls_ssl_write(&d->ssl,data,length);
    if (result>0) return result;
    return dtls_result(d,result)<0 ? -1:0;
}
static int flush_outgoing(wii_dtls_t *d)
{
    int result=Wii_DTLSQueueDrain(&d->outgoing,d,write_queued);
    if (result<0) d->failed=true;
    return result;
}
static neterr_t transmit(void *ctx,const qbyte *data,size_t length)
{
    wii_dtls_t *d=ctx;
    int result=handshake(d);
    if (result<0) return NETERR_DISCONNECTED;
    if (!result) return NETERR_CLOGGED;
    result=flush_outgoing(d);
    if (result<0) return NETERR_DISCONNECTED;
    if (!length) return NETERR_SENT;
    result=mbedtls_ssl_get_max_out_record_payload(&d->ssl);
    if (result<0 || length>(size_t)result) return NETERR_MTU;
    result=Wii_DTLSQueuePush(&d->outgoing,data,length);
    if (result<0) return NETERR_MTU;
    if (!result) return NETERR_CLOGGED;
    /* SENT means the immutable datagram is now owned by the transport. Tick
     * and receive also drain it; a blocked socket cannot discard later DATA. */
    if (flush_outgoing(d)<0) return NETERR_DISCONNECTED;
    return NETERR_SENT;
}
static neterr_t received(void *ctx,sizebuf_t *message)
{
    wii_dtls_t *d=ctx;
    if (d->failed) return NETERR_DISCONNECTED;
    if (d->established && cls.state<ca_active && Sys_Milliseconds()-d->trace_last>=5000) {
        d->trace_last=Sys_Milliseconds();
        Con_Printf("NET DTLS TOTAL: t=%u ms, UDP in %u, plaintext %u, UDP out %u, TX errors %u, input age %u ms, queued %u, dropped %u\n",
            d->trace_last,d->trace_rx,d->trace_plain,d->trace_tx,d->trace_tx_errors,
            d->trace_last-d->trace_input_time,d->packets,d->ingress_dropped);
    }
    if (message->cursize>0) {
        ++d->trace_rx; d->trace_input_time=Sys_Milliseconds();
        dtls_packet_t *packet=NULL;
        if (message->cursize<=18432 && d->packets<16)
            packet=BZF_Malloc(sizeof(*packet)+message->cursize);
        if (packet) {
            packet->next=NULL; packet->length=message->cursize;
            memcpy(packet->bytes,message->data,packet->length);
            if (d->tail) d->tail->next=packet; else d->head=packet;
            d->tail=packet; ++d->packets;
        } else if (++d->ingress_dropped<=4) {
            Con_Printf("NET DTLS: ingress dropped; queue %u/16, bytes %d; continuing decrypt drain\n",
                d->packets,message->cursize);
        }
        /* A full queue MUST still reach ssl_read below. Buffered records may
         * be returned without pull_packet, so rejecting here is absorbing:
         * the only function able to dequeue would never be called again. */
    }
    message->cursize=0;
    int result=handshake(d);
    if (result<0) return NETERR_DISCONNECTED;
    if (!result) return NETERR_CLOGGED;
    if (flush_outgoing(d)<0) return NETERR_DISCONNECTED;
    result=mbedtls_ssl_read(&d->ssl,message->data,message->maxsize);
    if (result>0) { ++d->trace_plain; message->cursize=result; return NETERR_SENT; }
    return dtls_result(d,result)<0 ? NETERR_DISCONNECTED:NETERR_CLOGGED;
}
static neterr_t timeouts(void *ctx)
{
    int result=handshake(ctx);
    if (result>0 && flush_outgoing(ctx)<0) return NETERR_DISCONNECTED;
    return result<0 ? NETERR_DISCONNECTED:result ? NETERR_SENT:NETERR_CLOGGED;
}
static int get_certificate(void *ctx,enum certprops_e property,char *out,size_t capacity)
{
    wii_dtls_t *d=ctx;
    if (!d) return -1;
    const mbedtls_x509_crt *cert=NULL;
    if (property==QCERT_LOCALCERTIFICATE) cert=&d->cert;
    else if (!d->established) return -1;
    else if (property==QCERT_ISENCRYPTED) return 0;
    else if (property==QCERT_PEERCERTIFICATE || property==QCERT_PEERSUBJECT) cert=mbedtls_ssl_get_peer_cert(&d->ssl);
    if (!cert) return -1;
    if (property==QCERT_PEERSUBJECT) return mbedtls_x509_dn_gets(out,capacity,&cert->subject);
    if (cert->raw.len>capacity) return -1;
    memcpy(out,cert->raw.p,cert->raw.len);
    return (int)cert->raw.len;
}
static qboolean generate_certificate(const char *subject,struct dtlslocalcred_s *credentials)
{
    mbedtls_pk_context key;
    mbedtls_x509write_cert cert;
    unsigned char der[2048],private_der[512],serial[16];
    char before[16],after[16];
    int result,certsize=0,keysize=0;
    qboolean ok=false;
    (void)subject;
    mbedtls_pk_init(&key); mbedtls_x509write_crt_init(&cert);
    if (credentials->key || credentials->cert) goto finish;
    result=mbedtls_pk_setup(&key,mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (result) goto finish;
    result=mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,mbedtls_pk_ec(key),WiiOnline_Random,NULL);
    if (result || WiiOnline_Random(NULL,serial,sizeof(serial))) goto finish;
    serial[0]=(serial[0]&0x7f)|1;
    mbedtls_x509write_crt_set_version(&cert,MBEDTLS_X509_CRT_VERSION_3);
    mbedtls_x509write_crt_set_md_alg(&cert,MBEDTLS_MD_SHA256);
    mbedtls_x509write_crt_set_subject_key(&cert,&key); mbedtls_x509write_crt_set_issuer_key(&cert,&key);
    if (mbedtls_x509write_crt_set_subject_name(&cert,"CN=NZPortable Wii") ||
        mbedtls_x509write_crt_set_issuer_name(&cert,"CN=NZPortable Wii") ||
        mbedtls_x509write_crt_set_serial_raw(&cert,serial,sizeof(serial))) goto finish;
    time_t start=time(NULL)-86400, end=start+30*86400;
    struct tm utc;
    if (!gmtime_r(&start,&utc) || !strftime(before,sizeof(before),"%Y%m%d%H%M%S",&utc) ||
        !gmtime_r(&end,&utc) || !strftime(after,sizeof(after),"%Y%m%d%H%M%S",&utc)) goto finish;
    if (mbedtls_x509write_crt_set_validity(&cert,before,after) ||
        mbedtls_x509write_crt_set_basic_constraints(&cert,0,-1) ||
        mbedtls_x509write_crt_set_key_usage(&cert,MBEDTLS_X509_KU_DIGITAL_SIGNATURE)) goto finish;
    certsize=mbedtls_x509write_crt_der(&cert,der,sizeof(der),WiiOnline_Random,NULL);
    keysize=mbedtls_pk_write_key_der(&key,private_der,sizeof(private_der));
    if (certsize<=0 || keysize<=0) goto finish;
    credentials->cert=BZ_Malloc(certsize); credentials->certsize=certsize;
    credentials->key=BZ_Malloc(keysize); credentials->keysize=keysize;
    memcpy(credentials->cert,der+sizeof(der)-certsize,certsize);
    memcpy(credentials->key,private_der+sizeof(private_der)-keysize,keysize);
    ok=true;
finish:
    mbedtls_platform_zeroize(private_der,sizeof(private_der));
    mbedtls_pk_free(&key); mbedtls_x509write_crt_free(&cert);
    return ok;
}
static const dtlsfuncs_t dtls_functions={create_context,NULL,destroy_context,transmit,received,timeouts,get_certificate,generate_certificate};
const dtlsfuncs_t *WiiOnline_DTLS(void) { return &dtls_functions; }
