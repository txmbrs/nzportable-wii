#include "../../nzportable_def.h"
#include "sdl_local.h"
#ifdef PLATFORM_WII
#include <SDL_mixer.h>
#endif

#define SDL_DMA_SAMPLES 32768
#ifdef PLATFORM_WII
static qboolean mixer_open;
#else
static SDL_AudioDeviceID audio_device;
#endif
static unsigned int audio_position;

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int length)
{
	int bytes = shm->samples * (shm->samplebits / 8);
	int offset = (audio_position * (shm->samplebits / 8)) % bytes;
	int first = length < bytes - offset ? length : bytes - offset;
	(void)userdata;
#ifdef PLATFORM_WII
    /* SDL_mixer has already decoded music into stream. Add game PCM to it. */
    SDL_MixAudioFormat(stream,shm->buffer+offset,AUDIO_S16SYS,first,SDL_MIX_MAXVOLUME);
    if(first<length)SDL_MixAudioFormat(stream+first,shm->buffer,AUDIO_S16SYS,length-first,SDL_MIX_MAXVOLUME);
#else
	memcpy(stream, shm->buffer + offset, first);
	if (first < length) memcpy(stream + first, shm->buffer, length - first);
#endif
	audio_position = (audio_position + length / (shm->samplebits / 8)) % shm->samples;
}

qboolean SNDDMA_Init(void)
{
	SDL_AudioSpec wanted, obtained;
	memset(&wanted, 0, sizeof(wanted));
	wanted.freq = 44100;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	wanted.callback = AudioCallback;
#ifdef PLATFORM_WII
    if(Mix_OpenAudio(wanted.freq,wanted.format,wanted.channels,wanted.samples)<0){
        Sys_Printf("Wii shared audio initialization failed: %s\n",Mix_GetError());return false;
    }
    mixer_open=true;
    int actual_channels=0;
    if(!Mix_QuerySpec(&obtained.freq,&obtained.format,&actual_channels)||obtained.format!=AUDIO_S16SYS||actual_channels!=2){
        Sys_Printf("Wii shared audio requires native 16-bit stereo\n");
        Mix_CloseAudio();mixer_open=false;return false;
    }
    obtained.channels=(Uint8)actual_channels;
#else
	audio_device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
	if (!audio_device) { Con_Printf("SDL audio disabled: %s\n", SDL_GetError()); return false; }
#endif
	shm = &sn;
	shm->splitbuffer = 0;
	shm->samplebits = 16;
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = SDL_DMA_SAMPLES;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = calloc(SDL_DMA_SAMPLES * (shm->samplebits / 8), sizeof(*shm->buffer));
    audio_position=0;
#ifdef PLATFORM_WII
    if(!shm->buffer){Mix_CloseAudio();mixer_open=false;return false;}
    Mix_SetPostMix(AudioCallback,NULL);
    Sys_Printf("Wii shared audio: %d Hz, native 16-bit stereo\n",obtained.freq);
#else
	if (!shm->buffer) { SDL_CloseAudioDevice(audio_device); audio_device = 0; return false; }
	SDL_PauseAudioDevice(audio_device, 0);
#endif
	return true;
}

int SNDDMA_GetDMAPos(void) { return (int)audio_position; }
void SNDDMA_Submit(void) {}
void SNDDMA_Shutdown(void) {
#ifdef PLATFORM_WII
    if(mixer_open){Mix_SetPostMix(NULL,NULL);Mix_CloseAudio();mixer_open=false;}
#else
    if(audio_device)SDL_CloseAudioDevice(audio_device);audio_device=0;
#endif
    if(shm){free(shm->buffer);shm->buffer=NULL;}
}
