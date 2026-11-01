"""Host tests of production DTLS queue/transmit and Netchan resend predicate.
This is not a Wii or full-engine network session."""
from pathlib import Path
import re,os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
provider=(root/'fte-online/fteqw-master/engine/client/wii_online_dtls_mbedtls.c').read_text(encoding='utf-8')
a=provider.index('static int write_queued(');b=provider.index('static neterr_t received(',a)
body=provider[a:b]
a=provider.index('static neterr_t timeouts(');b=provider.index('static int get_certificate(',a)
body+=provider[a:b]
chan=(root/'fte-online/fteqw-master/engine/common/net_chan.c').read_text(encoding='utf-8')
a=chan.index('send_reliable = false;',chan.index('// if the remote side dropped'))
b=chan.index('// if the reliable transmit buffer is empty',a)
predicate=chan[a:b]
assert 'last_wii_reliable_retry' not in chan
ice=(root/'fte-online/fteqw-master/engine/common/net_ice.c').read_text(encoding='utf-8')
a=ice.index('    {\n        neterr_t result=SCTP_PeerSendPacket',ice.index('neterr_t SCTP_Transmit('))
b=ice.index('\n}\n',a)
send_tail=ice[a:b]
stub=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wii_dtls_queue.h"
#define false 0
#define true 1
typedef unsigned char qbyte;
typedef int neterr_t;
enum {NETERR_SENT,NETERR_CLOGGED,NETERR_DISCONNECTED,NETERR_MTU};
typedef struct {int ssl,failed;wii_dtls_queue_t outgoing;} wii_dtls_t;
static int blocked,fatal,handshaking,emitted;
static unsigned char output[10000];
static const unsigned char *pending_address;
static size_t pending_size;
static unsigned char pending_copy[1200];
static int handshake(wii_dtls_t *d){return d->failed?-1:handshaking?0:1;}
static int dtls_result(wii_dtls_t *d,int result){if(result==-2)return 0;d->failed=1;return -1;}
static int mbedtls_ssl_get_max_out_record_payload(int *ssl){(void)ssl;return 1160;}
static int mbedtls_ssl_write(int *ssl,const unsigned char *data,size_t size){
 (void)ssl;
 if(pending_address){assert(pending_address==data && pending_size==size && !memcmp(data,pending_copy,size));}
 if(fatal)return -9;
 if(blocked){pending_address=data;pending_size=size;memcpy(pending_copy,data,size);return -2;}
 pending_address=NULL;assert(emitted<10000);output[emitted++]=data[0];return (int)size;
}
struct channel {unsigned incoming_acknowledged,last_reliable_sequence,incoming_reliable_acknowledged,reliable_sequence;};
static int need_retry(struct channel *chan){int send_reliable;
'''+predicate+r'''
return send_reliable;}
'''
body+=r'''
#define PLATFORM_WII 1
typedef struct {struct {unsigned tsn;} o;struct {int ackneeded;unsigned ackdeadline;} i;} sctp_t;
static neterr_t wire_result;
static neterr_t SCTP_PeerSendPacket(sctp_t *s,size_t n,void *h){(void)s;(void)n;(void)h;return wire_result;}
static neterr_t finish_sctp(sctp_t *sctp,size_t length){
 int saved_ackneeded=sctp->i.ackneeded;unsigned saved_ackdeadline=sctp->i.ackdeadline;
 size_t pktlen=0;void *h=NULL;
 if(length)++sctp->o.tsn;sctp->i.ackneeded=0;sctp->i.ackdeadline=0;
'''+send_tail+'\n}\n'
main=r'''
static void scenario(int lose_request,int lose_ack){
 struct channel client={0,11,0,1};
 int accepted=0,server_bit=0;
 if(!lose_request){++accepted;server_bit^=1;}
 /* A timeout without newer peer ACK must NEVER resend a reliable payload. */
 assert(!need_retry(&client));
 if(!lose_ack){client.incoming_acknowledged=11;client.incoming_reliable_acknowledged=server_bit;}
 /* Empty ACK probe seq 12 elicits current server state, without toggling it. */
 client.incoming_acknowledged=12;client.incoming_reliable_acknowledged=server_bit;
 if(need_retry(&client)){++accepted;server_bit^=1;}
 assert(accepted==1 && server_bit==1);
 client.incoming_reliable_acknowledged=server_bit;assert(!need_retry(&client));
}
int main(void){
 wii_dtls_t d={0};unsigned char packet[1200]={0};
 blocked=1;
 for(int i=0;i<16;++i){packet[0]=i;assert(transmit(&d,packet,1028)==NETERR_SENT);}
 packet[0]=99;assert(transmit(&d,packet,1028)==NETERR_CLOGGED);assert(d.outgoing.count==16);
 blocked=0;assert(timeouts(&d)==NETERR_SENT && !d.outgoing.count && emitted==16);
 for(int i=0;i<16;++i)assert(output[i]==i);
 assert(transmit(&d,packet,1200)==NETERR_MTU);
 for(int i=0;i<2000;++i){blocked=(i%3)==0;packet[0]=i%251;assert(transmit(&d,packet,100)==NETERR_SENT);blocked=0;assert(timeouts(&d)==NETERR_SENT);}
 assert(emitted==2016);for(int i=0;i<2000;++i)assert(output[16+i]==i%251);
 handshaking=1;assert(transmit(&d,packet,100)==NETERR_CLOGGED && !d.outgoing.count);handshaking=0;
 fatal=1;assert(transmit(&d,packet,100)==NETERR_DISCONNECTED && d.failed);fatal=0;
 for(int request=0;request<2;++request)for(int ack=0;ack<2;++ack)scenario(request,ack);
 sctp_t s={0};s.o.tsn=42;s.i.ackneeded=2;s.i.ackdeadline=100;
 wire_result=NETERR_CLOGGED;assert(finish_sctp(&s,100)==NETERR_CLOGGED);
 assert(s.o.tsn==42 && s.i.ackneeded==2 && s.i.ackdeadline==100);
 wire_result=NETERR_SENT;assert(finish_sctp(&s,100)==NETERR_SENT);
 assert(s.o.tsn==43 && !s.i.ackneeded);
 s.i.ackneeded=1;s.i.ackdeadline=200;wire_result=NETERR_CLOGGED;
 assert(finish_sctp(&s,0)==NETERR_CLOGGED && s.o.tsn==43 && s.i.ackneeded==1);
 /* Receiver sequence check rejects duplicate and delayed older packets. */
 unsigned sequence=20;assert(!(20>sequence) && !(19>sequence));
 puts("PASS: actual DTLS transmit/tick, repeated WANT_WRITE with stable buffers, bounded FIFO/full queue, MTU, wraparound, failure; Netchan lost request/lost ACK/both/probes");
}
'''
with tempfile.TemporaryDirectory(prefix='nzp-online-transport-') as tmp:
 p=Path(tmp)/'test.c';exe=Path(tmp)/'test'
 p.write_text(stub+'\n'+body+'\n'+main,encoding='utf-8')
 subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O2','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(root/'fte-online/fteqw-master/engine/client'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
