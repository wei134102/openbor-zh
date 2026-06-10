/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c)  OpenBOR Team
 */

/*
* Functions to load PNG and legacy GIF images into
* screens or bitmaps. PNG is preferred for OpenBOR 4.0
* modules; GIF is retained on openbor-zh-wii for older packs.
*
* Last update: openbor-zh-wii branch.
* Now loading to screens or bitmaps,
* creating them on-the-fly if necessary.
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>

#include "utils.h"
#include "types.h"
#include "borendian.h"
#include "bitmap.h"
#include "screen.h"
#include "packfile.h"
#include "png.h"
#include "pngdec.h"

// ============================== Globals ===============================
#define HANDLE_UNUSED -1
static int image_load_handle = HANDLE_UNUSED;

/*
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Resolution of the currently opened image. Image open 
* functions populate this value after validating the 
* file header. loadbitmap() uses it to allocate the 
* destination bitmap before readimage() decodes pixel 
* data.
*/

typedef struct image_resolution {
    int width;
    int height;
} image_resolution;

static image_resolution image_res = {.width = 0, .height = 0};

// ============================== PNG loading ===============================
// libpng loader restored from OpenBOR 3.0 for legacy mod compatibility on Wii.

static int png_height = 0;
static png_structp png_ptr = NULL;
static png_infop info_ptr = NULL;
static png_bytep *row_pointers = NULL;

static void png_read_fn(png_structp pngp, png_bytep outp, png_size_t size)
{
    readpackfile(image_load_handle, outp, (int)size);
}

static void png_read_destroy_all(void)
{
    if(png_ptr)
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    info_ptr = NULL;
    png_ptr = NULL;
}

static void closepng(void)
{
    int y;

    png_read_destroy_all();

    if(row_pointers)
    {
        for(y = 0; y < png_height; y++)
        {
            free(row_pointers[y]);
            row_pointers[y] = NULL;
        }
        free(row_pointers);
        row_pointers = NULL;
    }

    png_height = 0;

    if(image_load_handle >= 0)
    {
        closepackfile(image_load_handle);
    }
    image_load_handle = HANDLE_UNUSED;
}

static int openpng(const char *filename, const char *packfilename)
{
    unsigned char header[8];
    int y;

#ifdef VERBOSE
    printf("openpng: entering filename='%s' pack='%s'\n",
           filename,
           packfilename ? packfilename : "(null)");
#endif

    closepng();

    image_load_handle = openpackfile(filename, packfilename);
    if(image_load_handle == HANDLE_UNUSED)
    {
        goto openpng_abort;
    }

    if(readpackfile(image_load_handle, header, 8) != 8)
    {
        goto openpng_abort;
    }

    if(png_sig_cmp(header, 0, 8))
    {
        goto openpng_abort;
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png_ptr)
    {
        goto openpng_abort;
    }

    png_set_read_fn(png_ptr, &image_load_handle, png_read_fn);

    info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr)
    {
        goto openpng_abort;
    }

    png_set_sig_bytes(png_ptr, 8);
    png_read_info(png_ptr, info_ptr);

    image_res.width = png_get_image_width(png_ptr, info_ptr);
    png_height = image_res.height = png_get_image_height(png_ptr, info_ptr);

    if(png_get_bit_depth(png_ptr, info_ptr) != 8)
    {
        goto openpng_abort;
    }

    if(!image_bytes_valid(image_res.width, image_res.height, 4))
    {
        printf("\n\n Error: The PNG '%s' has invalid dimensions (%d x %d).\n",
               filename, image_res.width, image_res.height);
        goto openpng_abort;
    }

    png_read_update_info(png_ptr, info_ptr);

    row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * png_height);
    if(!row_pointers)
    {
        goto openpng_abort;
    }

    for(y = 0; y < png_height; y++)
    {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png_ptr, info_ptr));
        if(!row_pointers[y])
        {
            goto openpng_abort;
        }
    }

    png_read_image(png_ptr, row_pointers);
    return 1;

openpng_abort:
    closepng();
    return 0;
}

static int readpng(unsigned char *buf, unsigned char *pal, int maxwidth, int maxheight)
{
    int i, j, cw, ch;
    png_colorp png_pal_ptr = 0;
    int png_pal_num = 0;
    int pb = PAL_BYTES;

    cw = image_res.width > maxwidth ? maxwidth : image_res.width;
    ch = image_res.height > maxheight ? maxheight : image_res.height;

    if(buf)
    {
        for(i = 0; i < ch; i++)
        {
            memcpy(buf + (maxwidth * i), row_pointers[i], cw);
        }
    }

    if(pal)
    {
        if(png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY)
        {
            for(i = 0; i < 256; i++)
            {
                pal[i * 3] = pal[i * 3 + 1] = pal[i * 3 + 2] = (unsigned char)i;
            }
            return 1;
        }
        else if(png_get_PLTE(png_ptr, info_ptr, &png_pal_ptr, &png_pal_num) != PNG_INFO_PLTE ||
                png_pal_ptr == NULL)
        {
            return 0;
        }

        png_pal_ptr[0].red = png_pal_ptr[0].green = png_pal_ptr[0].blue = 0;

        if(pb == 512)
        {
            for(i = 0, j = 0; i < 512 && j < png_pal_num; i += 2, j++)
            {
                *(unsigned short *)(pal + i) = colour16(png_pal_ptr[j].red, png_pal_ptr[j].green, png_pal_ptr[j].blue);
            }
        }
        else if(pb == 768)
        {
            for(i = 0; i < png_pal_num; i++)
            {
                pal[i * 3] = png_pal_ptr[i].red;
                pal[i * 3 + 1] = png_pal_ptr[i].green;
                pal[i * 3 + 2] = png_pal_ptr[i].blue;
            }
        }
        else if(pb == 1024)
        {
            for(i = 0, j = 0; i < 1024 && j < png_pal_num; i += 4, j++)
            {
                *(unsigned *)(pal + i) = colour32(png_pal_ptr[j].red, png_pal_ptr[j].green, png_pal_ptr[j].blue);
            }
        }
    }

    return 1;
}

// ============================== GIF loading ===============================
// GIF on-disk fields are little-endian. Parse them from raw bytes so width and
// height are correct on both little- and big-endian hosts (Wii / PowerPC).

#define GIF_MAX_DIMENSION 4096
#define MAX_IMAGE_BYTES (32 * 1024 * 1024)

static int image_bytes_valid(int width, int height, int bpp)
{
    size_t bytes;

    if(width <= 0 || height <= 0 || bpp <= 0)
    {
        return 0;
    }

    if(width > INT_MAX / height)
    {
        return 0;
    }

    if((width * height) > INT_MAX / bpp)
    {
        return 0;
    }

    bytes = (size_t)width * (size_t)height * (size_t)bpp;
    return bytes <= MAX_IMAGE_BYTES;
}

static unsigned short read_le16(const unsigned char *p)
{
    return (unsigned short)(p[0] | (p[1] << 8));
}

typedef struct
{
    char            magic[6];
    unsigned short  screenwidth, screenheight;
    unsigned char   flags;
    unsigned char   background;
    unsigned char   aspect;
} gifheaderstruct;

typedef struct
{
    unsigned short  left, top, width, height;
    unsigned char   flags;
} gifblockstruct;

static int gif_dimensions_valid(int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        return 0;
    }

    if(width > GIF_MAX_DIMENSION || height > GIF_MAX_DIMENSION)
    {
        return 0;
    }

    if(width > INT_MAX / height)
    {
        return 0;
    }

    return 1;
}

static gifheaderstruct gif_header;

static unsigned char readbyte(int handle)
{
    unsigned char c = 0;
    readpackfile(handle, &c, 1);
    return c;
}

#define NO_CODE -1

static int decodegifblock(int handle, unsigned char *buf, int width, int height, unsigned char bits, gifblockstruct *gb)
{
    short bits2;
    short codesize;
    short codesize2;
    short nextcode;
    short thiscode;
    short oldtoken;
    short currentcode;
    short oldcode;
    short bitsleft;
    short blocksize;
    int line = 0;
    int byte = gb->left;
    int pass = 0;

    unsigned char *p;
    unsigned char *u;

    unsigned char *q;
    unsigned char b[255];
    unsigned char *linebuffer;

    static unsigned char firstcodestack[4096];
    static unsigned char lastcodestack[4096];
    static short codestack[4096];

    static short wordmasktable[] = {    0x0000, 0x0001, 0x0003, 0x0007,
                                        0x000f, 0x001f, 0x003f, 0x007f,
                                        0x00ff, 0x01ff, 0x03ff, 0x07ff,
                                        0x0fff, 0x1fff, 0x3fff, 0x7fff
                                   };

    static short inctable[] = { 8, 8, 4, 2, 0 };
    static short startable[] = { 0, 4, 2, 1, 0 };

    p = q = b;
    bitsleft = 8;

    if (bits < 2 || bits > 8)
    {
        return 0;
    }
    bits2 = 1 << bits;
    nextcode = bits2 + 2;
    codesize2 = 1 << (codesize = bits + 1);
    oldcode = oldtoken = NO_CODE;

    linebuffer = buf + (gb->top * width);

    for(;;)
    {
        if(bitsleft == 8)
        {
            if(++p >= q && (((blocksize = (unsigned char)readbyte(handle)) < 1) ||
                            (q = (p = b) + readpackfile(handle, b, blocksize)) < (b + blocksize)))
            {
                return 0;
            }
            bitsleft = 0;
        }
        thiscode = *p;
        if((currentcode = (codesize + bitsleft)) <= 8)
        {
            *p >>= codesize;
            bitsleft = currentcode;
        }
        else
        {
            if(++p >= q && (((blocksize = (unsigned char)readbyte(handle)) < 1) ||
                            (q = (p = b) + readpackfile(handle, b, blocksize)) < (b + blocksize)))
            {
                return 0;
            }

            thiscode |= *p << (8 - bitsleft);
            if(currentcode <= 16)
            {
                *p >>= (bitsleft = currentcode - 8);
            }
            else
            {
                if(++p >= q && (((blocksize = (unsigned char)readbyte(handle)) < 1) ||
                                (q = (p = b) + readpackfile(handle, b, blocksize)) < (b + blocksize)))
                {
                    return 0;
                }

                thiscode |= *p << (16 - bitsleft);
                *p >>= (bitsleft = currentcode - 16);
            }
        }
        thiscode &= wordmasktable[codesize];
        currentcode = thiscode;

        if(thiscode == (bits2 + 1))
        {
            break;
        }
        if(thiscode > nextcode)
        {
            return 0;
        }

        if(thiscode == bits2)
        {
            nextcode = bits2 + 2;
            codesize2 = 1 << (codesize = (bits + 1));
            oldtoken = oldcode = NO_CODE;
            continue;
        }

        u = firstcodestack;

        if(thiscode == nextcode)
        {
            if(oldcode == NO_CODE)
            {
                return 0;
            }
            *u++ = oldtoken;
            thiscode = oldcode;
        }

        while(thiscode >= bits2)
        {
            *u++ = lastcodestack [thiscode];
            thiscode = codestack[thiscode];
        }

        oldtoken = thiscode;
        do
        {
            if(byte < width && line < (height - gb->top))
            {
                linebuffer[byte] = thiscode;
            }
            byte++;
            if(byte >= gb->left + gb->width)
            {
                byte = gb->left;
                if(gb->flags & 0x40)
                {
                    line += inctable[pass];
                    if(line >= gb->height)
                    {
                        line = startable[++pass];
                    }
                }
                else
                {
                    ++line;
                }
                linebuffer = buf + (width * (gb->top + line));
            }
            if (u <= firstcodestack)
            {
                break;
            }
            thiscode = *--u;
        }
        while(1);

        if(nextcode < 4096 && oldcode != NO_CODE)
        {
            codestack[nextcode] = oldcode;
            lastcodestack[nextcode] = oldtoken;
            if(++nextcode >= codesize2 && codesize < 12)
            {
                codesize2 = 1 << ++codesize;
            }
        }
        oldcode = currentcode;
    }
    return 1;
}

static void passgifblock(int handle)
{
    int len;

    len = readbyte(handle);
    while((len = readbyte(handle)) != 0)
    {
        seekpackfile(handle, len, SEEK_CUR);
    }
}

static int opengif(const char *filename, const char *packfilename)
{
    unsigned char hdr[13];

    if(image_load_handle >= 0)
    {
        closepackfile(image_load_handle);
    }
    image_load_handle = HANDLE_UNUSED;

    image_load_handle = openpackfile(filename, packfilename);
    if(image_load_handle == HANDLE_UNUSED)
    {
        return 0;
    }

    if(readpackfile(image_load_handle, hdr, sizeof(hdr)) != sizeof(hdr))
    {
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
        return 0;
    }
    if(hdr[0] != 'G' || hdr[1] != 'I' || hdr[2] != 'F')
    {
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
        return 0;
    }

    memset(&gif_header, 0, sizeof(gif_header));
    memcpy(gif_header.magic, hdr, 6);
    image_res.width = read_le16(hdr + 6);
    image_res.height = read_le16(hdr + 8);
    gif_header.screenwidth = image_res.width;
    gif_header.screenheight = image_res.height;
    gif_header.flags = hdr[10];
    gif_header.background = hdr[11];
    gif_header.aspect = hdr[12];

    if(!gif_dimensions_valid(image_res.width, image_res.height))
    {
        printf("\n\n Error: The GIF '%s' has invalid dimensions (%d x %d).\n",
               filename, image_res.width, image_res.height);
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
        image_res.width = 0;
        image_res.height = 0;
        return 0;
    }

    return 1;
}

static int readgif(unsigned char *buf, unsigned char *pal, int maxwidth, int maxheight)
{
    gifblockstruct iblock;
    int bitdepth;
    int numcolours;
    int i, j;
    int done = 0;
    unsigned char *pbuf;
    unsigned char c;
    int pb = PAL_BYTES;

    bitdepth = (gif_header.flags & 7) + 1;
    numcolours = (1 << bitdepth);

    if(gif_header.flags & 0x80)
    {
        if(pal)
        {
            if(pb == 512)
            {
                pbuf = malloc(768);
                if(!pbuf || readpackfile(image_load_handle, pbuf, numcolours * 3) != numcolours * 3)
                {
                    free(pbuf);
                    return 0;
                }
                for(i = 0, j = 0; i < 512; i += 2, j += 3)
                {
                    *(unsigned short *)(pal + i) = colour16(pbuf[j], pbuf[j + 1], pbuf[j + 2]);
                }
                free(pbuf);
            }
            else if(pb == 768)
            {
                if(readpackfile(image_load_handle, pal, numcolours * 3) != numcolours * 3)
                {
                    return 0;
                }
            }
            else if(pb == 1024)
            {
                pbuf = malloc(768);
                if(!pbuf || readpackfile(image_load_handle, pbuf, numcolours * 3) != numcolours * 3)
                {
                    free(pbuf);
                    return 0;
                }
                for(i = 0, j = 0; i < 1024; i += 4, j += 3)
                {
                    *(unsigned *)(pal + i) = colour32(pbuf[j], pbuf[j + 1], pbuf[j + 2]);
                }
                free(pbuf);
            }
        }
        else
        {
            seekpackfile(image_load_handle, numcolours * 3, SEEK_CUR);
        }
    }

    if(!buf)
    {
        return 1;
    }

    while(!done)
    {
        if(readpackfile(image_load_handle, &c, 1) != 1)
        {
            break;
        }
        switch(c)
        {
        case ',':
        {
            unsigned char block[9];

            if(readpackfile(image_load_handle, block, sizeof(block)) != sizeof(block))
            {
                return 0;
            }

            iblock.left = read_le16(block + 0);
            iblock.top = read_le16(block + 2);
            iblock.width = read_le16(block + 4);
            iblock.height = read_le16(block + 6);
            iblock.flags = block[8];

            if(!gif_dimensions_valid(iblock.width, iblock.height))
            {
                return 0;
            }

            if((iblock.flags & 0x80) && pal)
            {
                i = 3 * (1 << ((iblock.flags & 0x0007) + 1));
                if(readpackfile(image_load_handle, pal, i) != i)
                {
                    return 0;
                }
            }
            else if(iblock.flags & 0x80)
            {
                seekpackfile(image_load_handle, 3 * (1 << ((iblock.flags & 0x0007) + 1)), SEEK_CUR);
            }

            if(readpackfile(image_load_handle, &c, 1) != 1)
            {
                return 0;
            }
            if(c < 2 || c > 8)
            {
                return 0;
            }
            if(!decodegifblock(image_load_handle, buf, maxwidth, maxheight, c, &iblock))
            {
                return 0;
            }
            break;
        }
        case '!':
            passgifblock(image_load_handle);
            break;
        case 0:
            break;
        default:
            done = 1;
        }
    }
    return 1;
}

// ============================== auto loading ===============================

/*
* Image format currently opened by openimage().
* readimage() and closeimage() use this value to
* route follow up work to the correct image loader.
*/
typedef enum open_type_enum {
    OT_NONE = 0,
    OT_GIF,
    OT_PNG
} open_type_enum;
    
static open_type_enum open_type = OT_NONE;

/*
* Caskey, Damon V.
* Added 2026-06-01.
*
* Finds the file extension in an image path. Dots 
* in directory names are ignored, so paths like 
* data/v1.0/sprite still count as extensionless.
*/
static const char *get_image_extension(const char *filename) {
    const char *ext = strrchr(filename, '.');
    const char *slash = strrchr(filename, '/');
    const char *backslash = strrchr(filename, '\\');

    if(!ext) {
        return NULL;
    }

    if(slash && ext < slash) {
        return NULL;
    }

    if(backslash && ext < backslash) {
        return NULL;
    }

    return ext;
}

/*
* Copy filename without a trailing extension into dest.
* Returns 1 if ext was present and stripped, else 0.
*/
static int copy_base_without_ext(char *dest, size_t destsize, const char *filename, const char *ext)
{
    size_t flen = strlen(filename);
    size_t elen = strlen(ext);

    if(flen <= elen || stricmp(filename + flen - elen, ext) != 0)
    {
        return 0;
    }

    snprintf(dest, destsize, "%.*s", (int)(flen - elen), filename);
    return 1;
}

/*
* Append an image extension to base without snprintf truncation warnings.
* Returns 1 on success, or 0 if the combined path would not fit in dest.
*/
static int append_image_ext(char *dest, size_t destsize, const char *base, const char *extension)
{
    size_t baselen = strlen(base);
    size_t extlen = strlen(extension);

    if(baselen == 0 || baselen + extlen >= destsize)
    {
        return 0;
    }

    memcpy(dest, base, baselen);
    memcpy(dest + baselen, extension, extlen);
    dest[baselen + extlen] = '\0';
    return 1;
}

/*
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Opens a PNG or GIF image from disk or the active pack file and records the
* detected image type for later readimage() and closeimage() calls.
*
* Legacy OpenBOR modules often ship GIF assets while model data references
* .png paths (or the reverse). When an explicit extension fails, the same
* basename is tried with the other supported format before giving up.
*
* Extensionless paths try .gif first, then .png, so older Wii modules load
* without pak edits while 4.0 PNG modules still work when no GIF exists.
*
* Returns 1 on success, or 0 if no supported image could be opened.
*/
static int openimage(char *filename, char *packfile) {

    char fnam[MAX_BUFFER_LEN];
    char alt[MAX_BUFFER_LEN];
    const char *ext = NULL;

    /*
    * Reset active image state before attempting a new open.
    * This prevents readimage(), closeimage(), or callers from using
    * stale state after a failed open attempt.
    */
    open_type = OT_NONE;
    image_res.width = 0;
    image_res.height = 0;

#ifdef VERBOSE
    printf("openimage: filename='%s' pack='%s'\n",
        filename,
        packfile ? packfile : "(null)");
#endif

    ext = get_image_extension(filename);

    /*
    * If the caller supplied an explicit extension, 
    * verify that it is a supported format and try 
    * to open it directly.
    */

    if(ext) {

        if(stricmp(ext, ".png") == 0) {

            if(openpng(filename, packfile)) {
                open_type = OT_PNG;
                return 1;
            }

            if(copy_base_without_ext(fnam, sizeof(fnam), filename, ".png")
                && append_image_ext(alt, sizeof(alt), fnam, ".gif"))
            {
                if(opengif(alt, packfile)) {
                    open_type = OT_GIF;
                    return 1;
                }
            }

            return 0;
        }

        if(stricmp(ext, ".gif") == 0) {

            if(opengif(filename, packfile)) {
                open_type = OT_GIF;
                return 1;
            }

            if(copy_base_without_ext(fnam, sizeof(fnam), filename, ".gif")
                && append_image_ext(alt, sizeof(alt), fnam, ".png"))
            {
                if(openpng(alt, packfile)) {
                    open_type = OT_PNG;
                    return 1;
                }
            }

            return 0;
        }

        printf("\n\n Error: Unsupported image format '%s' for file '%s'. Use PNG or GIF images.\n", ext, filename);
        return 0;
    }

    /*
    * No extension was supplied. Prefer GIF for legacy modules, then PNG.
    */

    if(append_image_ext(fnam, sizeof(fnam), filename, ".gif")
        && opengif(fnam, packfile)) {
        open_type = OT_GIF;
        return 1;
    }

    if(append_image_ext(fnam, sizeof(fnam), filename, ".png")
        && openpng(fnam, packfile)) {
        open_type = OT_PNG;
        return 1;
    }

    return 0;
}

static int readimage(unsigned char *buf, unsigned char *pal, int maxwidth, int maxheight)
{
    int result = 0;

    switch(open_type)
    {
    case OT_GIF:
        result = readgif(buf, pal, maxwidth, maxheight);
#ifdef VERBOSE
        printf("calling readimage %p %p %d %d with format %s, result is %d\n",
            buf, pal, maxwidth, maxheight, "GIF", result);
#endif
        break;

    case OT_PNG:
        result = readpng(buf, pal, maxwidth, maxheight);
#ifdef VERBOSE
        printf("calling readimage %p %p %d %d with format %s, result is %d\n",
            buf, pal, maxwidth, maxheight, "PNG", result);
#endif
        break;

    case OT_NONE:
    default:
        assert(!"invalid open_type in readimage");
        break;
    }

    return result;
}

static void closeimage()
{
    if(open_type == OT_PNG)
    {
        closepng();
    }
    else
    {
        /*
        * Safety cleanup for failed or unexpected states. handle 0 is a
        * valid pack handle, so check for >= 0 rather than > 0.
        */
        if(image_load_handle >= 0)
        {
            closepackfile(image_load_handle);
        }

        image_load_handle = HANDLE_UNUSED;
    }

    open_type = OT_NONE;
}

// ============================== Interface ===============================

int loadscreen(char *filename, char *packfile, unsigned char *pal, int format, s_screen **screen) {
    int result;
    unsigned char *p;

    #ifdef VERBOSE
        printf("loadscreen called packfile: %s, filename %s\n", packfile, filename);
    #endif

    if((*screen)) {
        freescreen(screen);
    }

    if(!openimage(filename, packfile)){
        return 0;
    }

    if(!(*screen) 
        || (*screen)->width != image_res.width 
        || (*screen)->height != image_res.height 
        || (*screen)->pixelformat != format) {

        (*screen) = allocscreen(image_res.width, image_res.height, format);
        
        if((*screen) == NULL) {
            closeimage();
            //assert(0);
            return 0;
        }
    }

    if(pal) {
        p = pal;
    
    } else {
        p = (*screen)->palette;
    }

    result = readimage((unsigned char *)(*screen)->data, p, (*screen)->width, (*screen)->height);
    closeimage();
    
    if(!result) {

        freescreen(screen);
        //assert(0);
        return 0;
    }

    return 1;
}

/*
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Loads a PNG image into a screen. The screen is 
* allocated on-the-fly. GIF backgrounds are loaded
* through loadscreen() instead.
*/
int loadscreen32(char *filename, char *packfile, s_screen **screen) {

    void *data;
    int handle, filesize;
    char fnam[MAX_BUFFER_LEN];

    #ifdef VERBOSE
        printf("loadscreen called packfile: %s, filename %s\n", packfile, filename);
    #endif

    if((*screen)) {
        freescreen(screen);
    }

    if((handle = openpackfile(filename, packfile)) == HANDLE_UNUSED) {

        snprintf(fnam, sizeof(fnam), "%s.png", filename);
        
        if((handle = openpackfile(fnam, packfile)) == HANDLE_UNUSED) {
            return 0;
        }
    }

    filesize = seekpackfile(handle, 0, SEEK_END);

    if(filesize <= 0) {
        closepackfile(handle);
        return 0;
    }

    data = malloc(filesize);
    
    if(!data) {
        closepackfile(handle);
        return 0;
    }

    if(seekpackfile(handle, 0, SEEK_SET) != 0) {
        closepackfile(handle);
        free(data);
        return 0;
    }

    if(readpackfile(handle, data, filesize) != filesize) {
        closepackfile(handle);
        free(data);
        return 0;
    }

    closepackfile(handle);

    (*screen) = pngToScreen(data);
    free(data);
    
    if (!(*screen)) { 
        return 0;
    }

    return 1;
}

s_bitmap *loadbitmap(char *filename, char *packfile, int format)
{
    int result;
    s_bitmap *bitmap;
    int maxwidth, maxheight;

    #ifdef VERBOSE
        printf("loadbitmap: file='%s' pack='%s' format=%d\n",
           filename,
           packfile ? packfile : "(null)",
           format);
    #endif

    if(!openimage(filename, packfile))
    {
        #ifdef VERBOSE
            printf("loadbitmap: openimage FAILED for '%s'\n", filename);
        #endif

        closeimage();
        return NULL;
    }

    #ifdef VERBOSE
        printf("loadbitmap: openimage OK, res=%d x %d\n", image_res.width, image_res.height);
    #endif

    maxwidth = image_res.width;
    maxheight = image_res.height;

    bitmap = allocbitmap(maxwidth, maxheight, format);
    if(!bitmap)
    {
        #ifdef VERBOSE
            printf("loadbitmap: allocbitmap FAILED, %d x %d format=%d\n",
                   maxwidth, maxheight, format);
        #endif

        closeimage();
        return NULL;
    }

    #ifdef VERBOSE
        printf("loadbitmap: bitmap=%p data=%p palette=%p\n",
               (void *)bitmap,
               (void *)bitmap->data,
               (void *)bitmap->palette);
    #endif

    result = readimage((unsigned char *)bitmap->data,
                       bitmap->palette,
                       maxwidth,
                       maxheight);

    closeimage();

    if(!result) {

        #ifdef VERBOSE
            printf("loadbitmap: readimage FAILED for '%s'\n", filename);
        #endif

        freebitmap(bitmap);
        return NULL;
    }

    #ifdef VERBOSE
        printf("loadbitmap: readimage OK for '%s'\n", filename);
    #endif

    return bitmap;
}

int loadimagepalette(char *filename, char *packfile, unsigned char *pal)
{
    int result;

    if(!openimage(filename, packfile))
    {
        return 0;
    }

    result = readimage(NULL, pal, 0, 0);
    closeimage();
    return  result;
}
