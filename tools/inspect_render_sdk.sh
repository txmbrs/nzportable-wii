#!/bin/bash
/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objdump -d /opt/devkitpro/portlibs/wii/lib/libopengx.a | sed -n '/<glDepthRange>:/,+10p'
grep -nE 'CONF_GetAspectRatio|CONF_ASPECT|VI_MAX_WIDTH' /opt/devkitpro/libogc/include/ogc/conf.h /opt/devkitpro/libogc/include/ogc/video.h
