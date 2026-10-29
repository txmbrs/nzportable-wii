/* mbed TLS provider for FTE's nonblocking TLS/WSS streams. GPL-2.0-or-later. */
#include "quakedef.h"
#include "netinc.h"
#include <mbedtls/ssl.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>

int WiiOnline_Random(void *ctx,unsigned char *out,size_t length);
typedef struct {
    vfsfile_t vfs;
    vfsfile_t *source;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_x509_crt roots;
    unsigned char pending[16384];
    size_t pending_length;
    qboolean established,failed;
    char peer[256];
} wii_tls_t;
static int tls_send(void *ctx,const unsigned char *data,size_t length)
{
    wii_tls_t *tls=ctx;
    int result=VFS_WRITE(tls->source,data,(int)length);
    if (!result) return MBEDTLS_ERR_SSL_WANT_WRITE;
    return result<0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : result;
}
static int tls_receive(void *ctx,unsigned char *data,size_t length)
{
    wii_tls_t *tls=ctx;
    int result=VFS_READ(tls->source,data,(int)length);
    if (!result) return MBEDTLS_ERR_SSL_WANT_READ;
    if (result==VFS_ERROR_EOF) return 0;
    return result<0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : result;
}
static int tls_error(wii_tls_t *tls,int result)
{
    char error[160];
    if (result==MBEDTLS_ERR_SSL_WANT_READ || result==MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
    if (result==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || !result) return VFS_ERROR_EOF;
    if (!tls->failed) {
        mbedtls_strerror(result,error,sizeof(error));
        Con_Printf("TLS %s: %s\n",tls->peer,error);
    }
    tls->failed=true;
    return result==MBEDTLS_ERR_X509_CERT_VERIFY_FAILED ? VFS_ERROR_UNTRUSTED : VFS_ERROR_UNSPECIFIED;
}
static int tls_handshake(wii_tls_t *tls)
{
    if (tls->failed) return VFS_ERROR_UNSPECIFIED;
    if (tls->established) return 1;
    int result=mbedtls_ssl_handshake(&tls->ssl);
    if (result) return tls_error(tls,result);
    if (mbedtls_ssl_get_verify_result(&tls->ssl)) return tls_error(tls,MBEDTLS_ERR_X509_CERT_VERIFY_FAILED);
    tls->established=true;
    Con_Printf("TLS: authenticated %s (%s)\n",tls->peer,mbedtls_ssl_get_version(&tls->ssl));
    return 1;
}
static int QDECL tls_read(vfsfile_t *file,void *buffer,int length)
{
    wii_tls_t *tls=(wii_tls_t *)file;
    int result=tls_handshake(tls);
    if (result<=0) return result;
    if (length<=0) return 0;
    result=mbedtls_ssl_read(&tls->ssl,buffer,(size_t)length);
    return result>0 ? result : tls_error(tls,result);
}
static int QDECL tls_write(vfsfile_t *file,const void *buffer,int length)
{
    wii_tls_t *tls=(wii_tls_t *)file;
    int result=tls_handshake(tls);
    if (result<=0) return result;
    if (length<=0) return 0;
    /* mbed TLS requires the same buffer/length on WANT_WRITE retries. FTE can
     * append to its outgoing buffer meanwhile, so retain a stable record copy. */
    if (!tls->pending_length) {
        tls->pending_length=(size_t)length>sizeof(tls->pending) ? sizeof(tls->pending):(size_t)length;
        memcpy(tls->pending,buffer,tls->pending_length);
    } else if ((size_t)length<tls->pending_length || memcmp(buffer,tls->pending,tls->pending_length))
        return tls_error(tls,MBEDTLS_ERR_SSL_BAD_INPUT_DATA);
    result=mbedtls_ssl_write(&tls->ssl,tls->pending,tls->pending_length);
    if (result>0) { tls->pending_length=0; return result; }
    return tls_error(tls,result);
}
static qboolean QDECL tls_seek(vfsfile_t *file,qofs_t position)
{ (void)file; (void)position; return false; }
static qofs_t QDECL tls_tell(vfsfile_t *file) { (void)file; return 0; }
static qboolean QDECL tls_close(vfsfile_t *file)
{
    wii_tls_t *tls=(wii_tls_t *)file;
    if (tls->established) mbedtls_ssl_close_notify(&tls->ssl);
    mbedtls_ssl_free(&tls->ssl); mbedtls_ssl_config_free(&tls->config); mbedtls_x509_crt_free(&tls->roots);
    if (tls->source) VFS_CLOSE(tls->source);
    BZ_Free(tls);
    return true;
}
static vfsfile_t *tls_open(const char *hostname,vfsfile_t *source,qboolean server)
{
    int result;
    wii_tls_t *tls;
    if (server || !hostname || !*hostname || !source) return NULL;
    tls=BZ_Malloc(sizeof(*tls)); memset(tls,0,sizeof(*tls));
    tls->vfs.ReadBytes=tls_read; tls->vfs.WriteBytes=tls_write;
    tls->vfs.Seek=tls_seek; tls->vfs.Tell=tls_tell; tls->vfs.GetLen=tls_tell;
    tls->vfs.Close=tls_close; tls->vfs.seekstyle=SS_UNSEEKABLE;
    Q_strncpyz(tls->peer,hostname,sizeof(tls->peer));
    mbedtls_ssl_init(&tls->ssl); mbedtls_ssl_config_init(&tls->config); mbedtls_x509_crt_init(&tls->roots);
    result=mbedtls_x509_crt_parse_file(&tls->roots,"cacert.pem");
    if (result) goto failed;
    result=mbedtls_ssl_config_defaults(&tls->config,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
    if (result) goto failed;
    mbedtls_ssl_conf_authmode(&tls->config,MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&tls->config,&tls->roots,NULL);
    mbedtls_ssl_conf_rng(&tls->config,WiiOnline_Random,NULL);
    result=mbedtls_ssl_setup(&tls->ssl,&tls->config);
    if (result) goto failed;
    result=mbedtls_ssl_set_hostname(&tls->ssl,hostname);
    if (result) goto failed;
    tls->source=source;
    mbedtls_ssl_set_bio(&tls->ssl,tls,tls_send,tls_receive,NULL);
    return &tls->vfs;
failed:
    tls_error(tls,result);
    /* On failure FTE still owns source and closes it itself. */
    tls_close(&tls->vfs);
    return NULL;
}
const dtlsfuncs_t *WiiOnline_DTLS(void);
ftecrypto_t crypto_wii_mbedtls={"Wii-mbedTLS",tls_open,NULL,WiiOnline_DTLS,WiiOnline_DTLS,NULL,NULL};
