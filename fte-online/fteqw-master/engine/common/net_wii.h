/* Wii IOS socket adapter for FTE. Port additions, GPL-2.0-or-later. */
#ifndef FTE_NET_WII_H
#define FTE_NET_WII_H
#include <fcntl.h>
#ifdef INVALID_SOCKET
#undef INVALID_SOCKET
#endif
#include <network.h>
#include <errno.h>
#include <string.h>
/* Pinned libogc network_wii.c uses wii_sockaddr_in without BSD sin_zero:
 * u8 length, u8 family, u16 port, u32 address. IOS requires length 8. */
#define WII_IOS_IPV4_ADDRESS_BYTES 8u

/* IOS returns negative errno values, whereas FTE expects -1 plus errno. */
static inline int WiiSocketResult(int result)
{
    if (result < 0) { errno = -result; return -1; }
    return result;
}
static inline int WiiSocket(int domain, int type, int protocol)
{
    /* IOS chooses TCP/UDP from the socket type. Explicit IPPROTO_TCP (6)
     * is rejected with EPROTONOSUPPORT on the hardware network service. */
    if (domain == AF_INET &&
        ((protocol == IPPROTO_TCP && (type & 0xf) == SOCK_STREAM) ||
         (protocol == IPPROTO_UDP && (type & 0xf) == SOCK_DGRAM)))
        protocol = IPPROTO_IP;
    return WiiSocketResult(net_socket(domain, type, protocol));
}
static inline int WiiCloseSocket(int fd)
{ return WiiSocketResult(net_close(fd)); }
static inline int WiiSocketAddress(struct sockaddr_in *out, const struct sockaddr *in, socklen_t length)
{
    if (!in || length < sizeof(*out) || in->sa_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return 0;
    }
    memcpy(out, in, sizeof(*out));
    out->sin_len = WII_IOS_IPV4_ADDRESS_BYTES;
    return 1;
}
static inline int WiiBind(int fd, const struct sockaddr *address, socklen_t length)
{
    struct sockaddr_in copy;
    int result, original;
    unsigned int attempts=0;
    if (!WiiSocketAddress(&copy, address, length)) {
        Con_Printf("NET BIND: invalid native address; fd %d, length %u, family %d, errno %d\n",
            fd,(unsigned int)length,address?address->sa_family:-1,errno);
        return -1;
    }
    original=result=net_bind(fd, (struct sockaddr *)&copy, sizeof(copy));
    if (result<0)
        Con_Printf("NET BIND: fd %d, port %u, address %08x, IOS result %d\n",
            fd,(unsigned int)ntohs(copy.sin_port),(unsigned int)copy.sin_addr.s_addr,result);
    /* Retain port-zero allocation as the first choice. If IOS rejects that
     * allocation, try a bounded set of unprivileged ephemeral ports instead.
     * Explicit requested ports never change, and unrelated errors propagate. */
    if (!copy.sin_port && (result==-EINVAL || result==-EADDRNOTAVAIL || result==-EADDRINUSE)) {
        int WiiOnline_Random(void *,unsigned char *,size_t);
        while (attempts++<16) {
            unsigned short random_port;
            if (WiiOnline_Random(NULL,(unsigned char *)&random_port,sizeof(random_port))) break;
            copy.sin_port=htons(49152u+(random_port&16383u));
            result=net_bind(fd,(struct sockaddr *)&copy,sizeof(copy));
            if (result>=0) {
                Con_Printf("NET BIND: ephemeral fallback succeeded; fd %d, port %u\n",fd,(unsigned int)ntohs(copy.sin_port));
                break;
            }
            if (result!=-EADDRINUSE && result!=-EINVAL && result!=-EADDRNOTAVAIL) break;
        }
        if (result<0)
            Con_Printf("NET BIND: ephemeral fallback failed; initial %d, final %d\n",original,result);
    }
    if (result>=0) {
        socklen_t size=sizeof(copy);
        memset(&copy,0,sizeof(copy));
        if (!net_getsockname(fd,(struct sockaddr *)&copy,&size))
            Con_Printf("NET BIND: ready; fd %d, local port %u\n",fd,(unsigned int)ntohs(copy.sin_port));
    }
    return WiiSocketResult(result);
}
static inline int WiiConnect(int fd, const struct sockaddr *address, socklen_t length)
{
    struct sockaddr_in copy;
    if (!WiiSocketAddress(&copy, address, length)) return -1;
    return WiiSocketResult(net_connect(fd, (struct sockaddr *)&copy, sizeof(copy)));
}
static inline int WiiSendTo(int fd, const void *bytes, int length, int flags, const struct sockaddr *address, socklen_t addr_length)
{
    struct sockaddr_in copy;
    if (!WiiSocketAddress(&copy, address, addr_length)) return -1;
    /* IOS has no POSIX SIGPIPE delivery. Passing MSG_NOSIGNAL through to
     * IOS sendto is unsupported; suppress it locally rather than changing
     * protocol data, TLS checks or process-wide signal handling. */
#ifdef MSG_NOSIGNAL
    flags &= ~MSG_NOSIGNAL;
#endif
    return WiiSocketResult(net_sendto(fd, bytes, length, flags, (struct sockaddr *)&copy, WII_IOS_IPV4_ADDRESS_BYTES));
}
static inline int WiiRecvFrom(int fd, void *bytes, int length, int flags, struct sockaddr *address, socklen_t *addr_length)
{
    struct sockaddr_in native __attribute__((aligned(32))) = {0};
    socklen_t native_length=WII_IOS_IPV4_ADDRESS_BYTES;
    int result;
    if (address && !addr_length) { errno=EINVAL; return -1; }
    native.sin_len=WII_IOS_IPV4_ADDRESS_BYTES;
    result=net_recvfrom(fd,bytes,length,flags,address?(struct sockaddr *)&native:NULL,address?&native_length:NULL);
    if (result>=0 && address) {
        socklen_t copied=*addr_length<sizeof(native)?*addr_length:sizeof(native);
        native.sin_len=sizeof(native);
        memcpy(address,&native,copied);
        *addr_length=sizeof(native);
    }
    return WiiSocketResult(result);
}
static inline int WiiSend(int fd, const void *bytes, int length, int flags)
{
#ifdef MSG_NOSIGNAL
    flags &= ~MSG_NOSIGNAL;
#endif
    return WiiSocketResult(net_send(fd, bytes, length, flags));
}
static inline int WiiRecv(int fd, void *bytes, int length, int flags)
{ return WiiSocketResult(net_recv(fd, bytes, length, flags)); }
static inline int WiiSelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{ return WiiSocketResult(net_select(nfds, readfds, writefds, exceptfds, timeout)); }
static inline int WiiSocketIoctl(int fd, unsigned long cmd, void *value)
{
    if (cmd == FIONBIO) {
        /* libogc maps FIONBIO to IOS_O_NONBLOCK (0x04). The C library's
         * O_NONBLOCK is a different flag and leaves IOS sockets blocking. */
        return WiiSocketResult(net_ioctl(fd, FIONBIO, value));
    }
    return WiiSocketResult(net_ioctl(fd, cmd, value));
}
static inline int WiiGetSockName(int fd, struct sockaddr *address, socklen_t *length)
{
    struct sockaddr_in native __attribute__((aligned(32))) = {0};
    socklen_t native_length=WII_IOS_IPV4_ADDRESS_BYTES;
    int result;
    if (!address || !length) { errno=EINVAL; return -1; }
    result=net_getsockname(fd,(struct sockaddr *)&native,&native_length);
    if (result>=0) {
        socklen_t copied=*length<sizeof(native)?*length:sizeof(native);
        native.sin_len=sizeof(native);
        memcpy(address,&native,copied);
        *length=sizeof(native);
    }
    return WiiSocketResult(result);
}
static inline int WiiSetSockOpt(int fd, int level, int option, const void *value, socklen_t length)
{
    int result=net_setsockopt(fd, level, option, value, length);
    if (result<0)
        Con_Printf("NET OPT: fd %d, level %d, option %d, length %u, IOS result %d\n",
            fd,level,option,(unsigned int)length,result);
    return WiiSocketResult(result);
}
static inline int WiiGetSockOpt(int fd, int level, int option, void *value, socklen_t *length)
{ return WiiSocketResult(net_getsockopt(fd, level, option, value, length)); }
static inline int WiiListen(int fd, int backlog)
{ return WiiSocketResult(net_listen(fd, backlog)); }
static inline int WiiAccept(int fd, struct sockaddr *address, socklen_t *length)
{ return WiiSocketResult(net_accept(fd, address, length)); }
static inline int WiiGetHostName(char *name, size_t length)
{
    if (!length) return -1;
    strncpy(name, "NZPortable-Wii", length);
    name[length-1] = 0;
    return 0;
}
#define socket WiiSocket
#define bind WiiBind
#define connect WiiConnect
#define sendto WiiSendTo
#define recvfrom WiiRecvFrom
#define send WiiSend
#define recv WiiRecv
#define select WiiSelect
#define getsockname WiiGetSockName
#define getsockopt WiiGetSockOpt
#define setsockopt WiiSetSockOpt
#define listen WiiListen
#define accept WiiAccept
#define gethostname WiiGetHostName
#define gethostbyname net_gethostbyname
#endif
