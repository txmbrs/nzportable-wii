"""Instrument the exact libogc 3.1.0 network module; preserve its state machine."""
from pathlib import Path
from zipfile import ZipFile
import hashlib
import re

root=Path(__file__).resolve().parent
archive=root/'dependencies/libogc-3.1.0.zip'
if hashlib.sha256(archive.read_bytes()).hexdigest()!='55c891caf66f5a4ffc304363b1465c9fd4d87a8e190faf19fdc0a60965d3915b':
    raise SystemExit('Unexpected libogc source archive.')
with ZipFile(archive) as z:
    original=z.read('libogc-3.1.0/libogc/network_wii.c')
    soc=z.read('libogc-3.1.0/libogc/ogc_sockets/soc.h')
if hashlib.sha256(original).hexdigest()!='69b616eafcdc3d0914d85cbe38e564e4337428ff9e4408206f2ecef28be7e8ed':
    raise SystemExit('Unexpected SDK network source.')
source=original.decode()
start=source.index('static s32 net_init_chain(s32 result, void *usrdata) {')
end=source.index('\ns32 net_init_async(',start)
chain=source[start:end]
marker='\tdebug_printf("net_init chain entered: %u %d\\n", data->state, result);'
assert chain.count(marker)==1
entry='''
    /* Wii port diagnostic addition: memory only, including in IPC callbacks. */
    s32 trace_payload=0;
    if(data->state==7 && result==0 && data->buf)
        memcpy(&trace_payload,data->buf,sizeof(trace_payload));
    WiiNetTrace_Record(0,data->state,result,data->prevres,data->result,
                       data->retries,trace_payload,0);
'''
chain=chain.replace(marker,marker+entry)
operation={'IOS_OpenAsync':1,'IOS_CloseAsync':2,'IOS_IoctlAsync':3,'IOS_IoctlvFormatAsync':4,'SYS_SetAlarm':5}
pattern=r'(?m)^([ \t]*)(data->result = (IOS_OpenAsync|IOS_CloseAsync|IOS_IoctlAsync|IOS_IoctlvFormatAsync|SYS_SetAlarm)\([^\n]+;)'
count=0
def inject(m):
    global count
    count+=1
    indent,statement,name=m.groups()
    return (indent+'WiiNetTrace_Record(1,data->state,0,data->prevres,data->result,data->retries,0,'+
            str(operation[name])+');\n'+indent+statement)
chain=re.sub(pattern,inject,chain)
assert count==12, count
cleanup_pattern=r'(?m)^([ \t]*)(if \(IOS_CloseAsync\([^\n]+)'
def cleanup(m):
    indent,statement=m.groups()
    return indent+'WiiNetTrace_Record(1,data->state,0,data->prevres,data->result,data->retries,0,2);\n'+indent+statement
chain,cleanup_count=re.subn(cleanup_pattern,cleanup,chain)
assert cleanup_count==4, cleanup_count
done='\t_last_init_result = data->result;'
assert chain.count(done)==1
chain=chain.replace(done,'\tWiiNetTrace_Record(2,data->state,data->result,data->prevres,data->result,data->retries,0,0);\n'+done)
source=source[:start]+chain+source[end:]
source='/* Altered Wii port build: callback/request tracing only; upstream license follows. */\n'+source
# Removing only our observation code must reproduce the exact upstream text.
reviewed=source.split('\n',1)[1].replace(entry,'',1)
reviewed=re.sub(r'(?m)^[ \t]*WiiNetTrace_Record\([^;]+;\n','',reviewed)
assert reviewed==original.decode(), 'Instrumentation unexpectedly changed SDK logic.'
out=root.parent/'build/wii-nettrace'
(out/'ogc_sockets').mkdir(parents=True,exist_ok=True)
(out/'network_wii.c').write_text(source,encoding='utf-8')
(out/'network_wii.original.c').write_bytes(original)
(out/'ogc_sockets/soc.h').write_bytes(soc)
print('libogc 3.1.0 trace: chain entry, 16 requests/alarms and final result instrumented.')
