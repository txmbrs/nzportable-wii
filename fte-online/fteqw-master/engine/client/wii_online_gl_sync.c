/* OpenGX client-array lifetime adapter. GPL-2.0-or-later.
 * FTE reuses CPU geometry immediately after a draw. Make CPU updates visible
 * to GX and finish consumption before those arrays can be overwritten. */
#include <GL/gl.h>
#include <gccore.h>
#include <stdint.h>
#include <string.h>

void WiiOnline_Log(const char *message);

static unsigned int scalar_bytes(GLint type)
{
    switch (type) {
    case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
    case GL_INT: case GL_UNSIGNED_INT: case GL_FLOAT: return 4;
    case GL_DOUBLE: return 8;
    default: return 0;
    }
}

static void flush_array(GLenum enable, GLenum pointer_enum, GLenum size_enum,
                        GLenum type_enum, GLenum stride_enum,
                        unsigned int last, GLint fixed_size)
{
    void *pointer=NULL;
    GLint size=fixed_size, type=0, stride=0;
    uintptr_t address;
    uint64_t bytes, element;
    if (!glIsEnabled(enable)) return;
    glGetPointerv(pointer_enum, &pointer);
    address=(uintptr_t)pointer;
    /* This path deliberately excludes buffer offsets and non-RAM addresses. */
    if (address<0x80000000u || address>=0x94000000u) return;
    if (size_enum) glGetIntegerv(size_enum, &size);
    glGetIntegerv(type_enum, &type);
    glGetIntegerv(stride_enum, &stride);
    if (size<1 || size>4 || stride<0) return;
    element=(unsigned int)size*scalar_bytes(type);
    if (!element) return;
    bytes=(uint64_t)last*(stride ? (unsigned int)stride : element)+element;
    if (bytes>0x94000000u-address) return;
    DCFlushRange(pointer, (unsigned int)bytes);
}

static void prepare_arrays(unsigned int last)
{
    GLint unit=GL_TEXTURE0, units=1;
    int i;
    static int announced;
    if (!announced) {
        WiiOnline_Log("GFX: OpenGX client arrays: cache flush and GPU completion enabled\n");
        announced=1;
    }
    flush_array(GL_VERTEX_ARRAY, GL_VERTEX_ARRAY_POINTER, GL_VERTEX_ARRAY_SIZE,
                GL_VERTEX_ARRAY_TYPE, GL_VERTEX_ARRAY_STRIDE, last, 0);
    flush_array(GL_COLOR_ARRAY, GL_COLOR_ARRAY_POINTER, GL_COLOR_ARRAY_SIZE,
                GL_COLOR_ARRAY_TYPE, GL_COLOR_ARRAY_STRIDE, last, 0);
    flush_array(GL_NORMAL_ARRAY, GL_NORMAL_ARRAY_POINTER, 0,
                GL_NORMAL_ARRAY_TYPE, GL_NORMAL_ARRAY_STRIDE, last, 3);
    glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE, &unit);
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
    if (units<1) units=1;
    if (units>8) units=8;
    for (i=0; i<units; i++) {
        glClientActiveTexture(GL_TEXTURE0+i);
        flush_array(GL_TEXTURE_COORD_ARRAY, GL_TEXTURE_COORD_ARRAY_POINTER,
                    GL_TEXTURE_COORD_ARRAY_SIZE, GL_TEXTURE_COORD_ARRAY_TYPE,
                    GL_TEXTURE_COORD_ARRAY_STRIDE, last, 0);
    }
    glClientActiveTexture(unit);
    GX_InvVtxCache();
}

static void sync_draw_elements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    unsigned int last=0;
    GLsizei i;
    if (count<=0) return;
    /* The Wii renderer disables VBOs, so indices must be CPU data. */
    if (!indices) return;
    for (i=0; i<count; i++) {
        unsigned int index;
        if (type==GL_UNSIGNED_SHORT) {
            uint16_t value;
            memcpy(&value, (const unsigned char *)indices+i*2, 2);
            index=value;
        } else if (type==GL_UNSIGNED_INT) {
            uint32_t value;
            memcpy(&value, (const unsigned char *)indices+i*4, 4);
            index=value;
        } else if (type==GL_UNSIGNED_BYTE)
            index=((const unsigned char *)indices)[i];
        else return;
        if (index>last) last=index;
    }
    prepare_arrays(last);
    glDrawElements(mode, count, type, indices);
    GX_DrawDone();
}

static void sync_draw_range(GLenum mode, GLuint start, GLuint end, GLsizei count,
                            GLenum type, const void *indices)
{
    (void)start; (void)end;
    sync_draw_elements(mode, count, type, indices);
}

static void sync_draw_arrays(GLenum mode, GLint first, GLsizei count)
{
    if (first<0 || count<=0 || (uint64_t)(unsigned int)first+(unsigned int)count>UINT32_MAX)
        return;
    prepare_arrays((unsigned int)first+(unsigned int)count-1);
    glDrawArrays(mode, first, count);
    GX_DrawDone();
}

static void sync_multi_draw(GLenum mode, const GLsizei *counts, GLenum type,
                            const void *const *indices, GLsizei draws)
{
    GLsizei i;
    for (i=0; i<draws; i++) sync_draw_elements(mode, counts[i], type, indices[i]);
}

void *WiiOnline_GLDrawFunction(const char *name)
{
    if (!strcmp(name,"glDrawElements")) return (void *)sync_draw_elements;
    if (!strcmp(name,"glDrawArrays")) return (void *)sync_draw_arrays;
    if (!strcmp(name,"glDrawRangeElements") || !strcmp(name,"glDrawRangeElementsEXT"))
        return (void *)sync_draw_range;
    if (!strcmp(name,"glMultiDrawElements") || !strcmp(name,"glMultiDrawElementsEXT"))
        return (void *)sync_multi_draw;
    return NULL;
}
