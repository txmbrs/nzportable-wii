/* Mbed TLS's embedded NV-seed entropy path, GPL-2.0-or-later.
 * Uses the library's NV seed update/reseeding, not a custom random generator.
 * https://mbed-tls.readthedocs.io/en/latest/kb/how-to/add-entropy-sources-to-entropy-pool/
 * The host provisioning utility creates a private, unique OS-random seed per
 * installation. No seed is shipped in release/source archives. Missing/invalid
 * seed or persistence failure prevents TLS/ICE startup. */
#include <gccore.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_util.h>
#include <stdio.h>
#include <unistd.h>
#include <ogc/lwp_watchdog.h>

static mbedtls_entropy_context entropy_state;
static mbedtls_ctr_drbg_context random_state;
static mutex_t random_mutex;
static int random_ready;
static int random_initializing;
void WiiOnline_RandomProgress(const char *message);
static void progress(const char *message)
{ if(random_initializing) WiiOnline_RandomProgress(message); }

mbedtls_ms_time_t mbedtls_ms_time(void)
{ return (mbedtls_ms_time_t)ticks_to_millisecs(gettime()); }
static int read_nv_seed(unsigned char *out,size_t length)
{
    progress("TLS: opening private seed for reading");
    FILE *file=fopen("tls-seed.bin","rb");
    if (!file) return -1;
    setbuf(file,NULL);
    progress("TLS: reading private seed");
    size_t read=fread(out,1,length,file);
    int extra=fgetc(file);
    progress("TLS: closing private seed after read");
    int closed=fclose(file);
    progress("TLS: private seed read completed");
    return read==length && extra==EOF && !closed ? (int)length : -1;
}
static int write_nv_seed(unsigned char *seed,size_t length)
{
    progress("TLS: opening private seed for update");
    FILE *file=fopen("tls-seed.bin","wb");
    if (!file) return -1;
    setbuf(file,NULL);
    progress("TLS: writing updated private seed");
    int written=fwrite(seed,1,length,file)==length;
    progress("TLS: flushing updated private seed");
    int flushed=!fflush(file);
    progress("TLS: syncing updated private seed");
    int synced=!fsync(fileno(file));
    progress("TLS: closing updated private seed");
    int closed=!fclose(file);
    progress("TLS: private seed update completed");
    return written && flushed && synced && closed ? (int)length : -1;
}
int WiiOnline_InitRandom(void)
{
    if (random_ready) return 0;
    random_initializing=1;
    progress("TLS: configuring Mbed TLS NV-seed callbacks");
    mbedtls_platform_set_nv_seed(read_nv_seed,write_nv_seed);
    progress("TLS: initializing entropy and DRBG contexts");
    mbedtls_entropy_init(&entropy_state);
    mbedtls_ctr_drbg_init(&random_state);
    progress("TLS: seeding DRBG");
    int result=mbedtls_ctr_drbg_seed(&random_state,mbedtls_entropy_func,&entropy_state,
        (const unsigned char *)"NZP-Wii-TLS-ICE",14);
    if (!result) {
        progress("TLS: DRBG seeded; initializing random mutex");
        result=LWP_MutexInit(&random_mutex,false);
    }
    progress(result ? "TLS: random initialization failed":"TLS: random initialization completed");
    random_initializing=0;
    if (result) {
        mbedtls_ctr_drbg_free(&random_state);
        mbedtls_entropy_free(&entropy_state);
        return -1;
    }
    random_ready=1;
    return 0;
}
int WiiOnline_Random(void *ctx,unsigned char *out,size_t length)
{
    int result;
    (void)ctx;
    if (!random_ready) return MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
    LWP_MutexLock(random_mutex);
    result=mbedtls_ctr_drbg_random(&random_state,out,length);
    LWP_MutexUnlock(random_mutex);
    return result;
}
