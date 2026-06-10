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
#include <zlib.h>

#include "utils.h"
#include "types.h"
#include "borendian.h"
#include "bitmap.h"
#include "screen.h"
#include "packfile.h"
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
// New PNG decoder by Plombo (2019-1-18) -- faster than the old libpng-based one
// 
// Caskey, Damon V., 2026-06-01 - Minor readability updates, clean up for 64-bit 
// Win and drop interlaced support.

#define PNG_MAGIC        0x89504e470d0a1a0aLL
#define PNG_CHUNK_IHDR   0x49484452
#define PNG_CHUNK_PLTE   0x504c5445
#define PNG_CHUNK_IDAT   0x49444154
#define PNG_CHUNK_IEND   0x49454e44

struct png_chunk_header {
    uint32_t chunk_size;
    uint32_t chunk_name;
};

static void closepng()
{
    if (image_load_handle >= 0)
    {
        closepackfile(image_load_handle);
    }
    image_load_handle = HANDLE_UNUSED;
}

/*
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Opens a PNG image from disk or the active pack file and reads
* enough header information to validate the image and populate the
* global image resolution values.
*
* Only 8-bit grayscale and indexed PNG images are supported. RGB,
* RGBA, grayscale-alpha, and non-standard compression or filter
* methods are rejected.
*
* Returns 1 on success, or 0 on error. On success, the pack file
* handle remains open for readpng() and must later be released by
* closepng().
*/
static int openpng(const char *filename, const char *packfilename) {

    uint64_t magic;
    struct png_chunk_header chunk_header;
    unsigned char ihdr_data[13];
    uint32_t width_raw;
    uint32_t height_raw;
    uint32_t width;
    uint32_t height;

#ifdef VERBOSE
    printf("openpng: entering filename='%s' pack='%s'\n",
           filename,
           packfilename ? packfilename : "(null)");
#endif

    /*
    * Close any previous image handle before attempting a 
    * new open. This prevents stale handles from surviving 
    * across failed opens.
    */
    closepng();

    image_load_handle = openpackfile(filename, packfilename);
    if(image_load_handle == HANDLE_UNUSED) {
        goto openpng_abort;
    }

#ifdef VERBOSE
    printf("openpng: openpackfile OK handle=%d\n", image_load_handle);
#endif

    /*
    * Validate the 8 byte PNG signature. SwapMSB64() must use a true
    * 64-bit integer type on all platforms, including Win64.
    */
    if(readpackfile(image_load_handle, &magic, 8) != 8) {
        goto openpng_abort;
    }

    if(magic != SwapMSB64(PNG_MAGIC)) {
        goto openpng_abort;
    }

    /*
    * The first PNG chunk must be IHDR and its payload must be 13 bytes.
    */
    if(readpackfile(image_load_handle, &chunk_header, sizeof(chunk_header)) != sizeof(chunk_header)) {
        goto openpng_abort;
    }

    if(SwapMSB32(chunk_header.chunk_size) != 13 ||
            chunk_header.chunk_name != SwapMSB32(PNG_CHUNK_IHDR)) {
        goto openpng_abort;
    }

    /*
    * Read the IHDR payload. Use memcpy() instead of casting the byte
    * buffer to uint32_t*. Casting can violate alignment and aliasing
    * rules, especially on 64-bit builds.
    */
    if(readpackfile(image_load_handle, ihdr_data, sizeof(ihdr_data)) != sizeof(ihdr_data)) {
        goto openpng_abort;
    }

    memcpy(&width_raw, ihdr_data + 0, sizeof(width_raw));
    memcpy(&height_raw, ihdr_data + 4, sizeof(height_raw));

    width = SwapMSB32(width_raw);
    height = SwapMSB32(height_raw);

    /* 
    * Check for zero dimensions, and also guard against 
    * 32-bit integer overflow when converting to int. 
    */
   
    if(width == 0 || height == 0 || width > INT_MAX || height > INT_MAX) {
        printf("\n\n Error: The image %s has invalid dimensions.\n", filename);
        goto openpng_abort;
    }

    image_res.width = (int)width;
    image_res.height = (int)height;

    /*
    * Bit depth must be 8 for engine use. PNG compression and filter
    * method must be 0 by specification.
    */
    if(ihdr_data[8] != 8 || ihdr_data[10] != 0 || ihdr_data[11] != 0) {
        printf("\n\n Error: The image %s has unsupported bit depth or compression/filter method. Use 8-bit PNG images with standard compression and filter methods.\n", filename);
        goto openpng_abort;
    }

    /*
    * Color type must be grayscale or indexed. Truecolor, alpha, and
    * truecolor-alpha PNG images are not supported by this loader.
    */
    if(ihdr_data[9] != 0 && ihdr_data[9] != 3) {
        printf("\n\n Error: The image %s has unsupported color type. Use indexed PNG images.\n", filename);
        goto openpng_abort;
    }

    /*
    * Interlaced PNGs are not supported. Local packed assets get no
    * practical benefit from Adam7 interlacing, and supporting it adds
    * decoder complexity. Use non-interlaced PNGs.
    */
    if(ihdr_data[12] != 0) {
        printf("\n\n Error: The image %s is interlaced. Use non-interlaced PNG images.\n", filename);
        goto openpng_abort;
    }

    return 1;

openpng_abort:

    /*
    * If the PNG failed after opening a file or pack entry, release the
    * handle before returning failure. If no handle was opened, leave the
    * close path alone.
    */
    if(image_load_handle != HANDLE_UNUSED) {
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
    }

    return 0;
}

// Based on the PaethPredictor pseudocode in the PNG specification.
// a = pixel to the left, b = above, c = upper left
static inline unsigned char png_paeth_predictor(unsigned char a, unsigned char b, unsigned char c)
{
    // initial estimate
    int p = a + b - c;

    // distances to a, b, c
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    
    // return nearest of a,b,c, breaking ties in the order a,b,c
    if (pa <= pb && pa <= pc) return a;
    else if (pb <= pc) return b;
    else return c;
}

// Decodes the image from a decompressed, non-interlaced IDAT stream.
static void png_decode_data(unsigned char *buf, unsigned char *inflated_data, int max_width, int max_height)
{
    int width = image_res.width;
    unsigned int y, x;

    for (y = 0; y < max_height; y++)
    {
        switch (inflated_data[y * (width + 1)])
        {
            case 0: // no filter, the easiest case
            {
                memcpy(buf + (y * max_width), inflated_data + (y * (width + 1)) + 1, max_width);
                break;
            }
            case 1: // Sub filter: Raw(x) = Sub(x) + Raw(pixel to the left of x)
            {
                unsigned char last = 0;
                for (x = 0; x < max_width; x++)
                {
                    last = buf[y * max_width + x] = inflated_data[y * (width + 1) + 1 + x] + last;
                }
                break;
            }
            case 2: // Up filter: Raw(x) = Up(x) + Raw(pixel above x)
            {
                if (y == 0)
                {
                    memcpy(buf + (y * max_width), inflated_data + (y * (width + 1)) + 1, max_width);
                }
                else
                {
                    unsigned int lastline = y - 1;
                    for (x = 0; x < max_width; x++)
                    {
                        buf[y * max_width + x] = inflated_data[y * (width + 1) + 1 + x] + buf[lastline * max_width + x];
                    }
                }
                break;
            }
            case 3: // Average filter: Raw(x) = Average(x) + floor((Raw(pixel above x) + Raw(pixel left of x))/2)
            {
                unsigned char last = 0;
                unsigned int lastline = y - 1;
                for (x = 0; x < max_width; x++)
                {
                    unsigned char a = last;
                    unsigned char b = (y == 0) ? 0 : buf[lastline * max_width + x];
                    last = buf[y * max_width + x] = inflated_data[y * (width + 1) + 1 + x] + ((a + b) / 2);
                }
                break;
            }
            case 4: // Paeth filter: the complicated one
            {
                unsigned char last = 0;
                unsigned int lastline = y - 1;
                for (x = 0; x < max_width; x++)
                {
                    unsigned char a = last;
                    unsigned char b = (y == 0) ? 0 : buf[lastline * max_width + x];
                    unsigned char c = (y == 0 || x == 0) ? 0 : buf[lastline * max_width + x - 1];
                    last = buf[y * max_width + x] = inflated_data[y * (width + 1) + 1 + x] + png_paeth_predictor(a, b, c);
                }
                break;
            }
            default:
            {
                printf("invalid PNG filter %i for line %u\n", inflated_data[y * (width + 1)], y);
                assert(!"invalid PNG filter");
            }
        }
    }
}

/*
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Reads the PNG image data from the active pack 
* file handle, inflates the IDAT chunks, and applies 
* PNG filters. 2026-06-01 rework adds documentation 
* and shores up some edge cases for PNG decoding and
* 64-bit compatibility.
*/
static int readpng(unsigned char *buf, unsigned char *pal, int max_width, int max_height) {

    unsigned char *png_data = NULL;
    unsigned char *png_data_ptr = NULL;
    unsigned char *inflated_data = NULL;
    z_stream zlib_stream = {
        .zalloc = Z_NULL, 
        .zfree = Z_NULL, 
        .opaque = Z_NULL, 
        .avail_in = 0, 
        .next_in = Z_NULL,
        .avail_out = 0, 
        .next_out = Z_NULL
    };

    int width = image_res.width;
    int height = image_res.height;
    int data_start_pos;
    int data_size;
    int z_lib_result = Z_OK;
    int zlib_initialized = 0;

    /*
    * Initialize the zlib stream before reading IDAT chunks.
    * readpng_abort calls inflateEnd(), so any failure after this
    * point can use the common cleanup path. We then set a flag 
    * to indicate that the zlib stream has been initialized for
    * downstream guards.
    */
    if(inflateInit(&zlib_stream) != Z_OK) {
        goto readpng_abort;
    }

    zlib_initialized = 1;

    /*
    * Read the remaining PNG data into memory. openpng() already read
    * the PNG signature, IHDR chunk header, and IHDR payload. The file
    * pointer is currently positioned just before the IHDR CRC, so skip
    * four bytes to move to the next chunk.
    *
    * Loading the rest of the file into one buffer avoids repeated pack
    * file I/O while walking PNG chunks.
    */
    int current_pos;
    int end_pos;

    current_pos = seekpackfile(image_load_handle, 0, SEEK_CUR);
    end_pos = seekpackfile(image_load_handle, 0, SEEK_END);

    if(current_pos < 0 || end_pos < 0) {
        goto readpng_abort;
    }

    data_start_pos = current_pos + 4;

    if(data_start_pos < current_pos || end_pos <= data_start_pos) {
        goto readpng_abort;
    }

    data_size = end_pos - data_start_pos;

    if(data_size <= 0) {
        goto readpng_abort;
    }

    if(seekpackfile(image_load_handle, data_start_pos, SEEK_SET) < 0) {
        goto readpng_abort;
    }

    png_data = malloc(data_size);

    if(!png_data) {
        goto readpng_abort;
    }

    if(readpackfile(image_load_handle, png_data, data_size) != data_size) {
        goto readpng_abort;
    }

    png_data_ptr = png_data;    

    /*
    * If the caller supplied a pixel buffer, allocate space for the
    * decompressed scanlines. Each PNG scanline begins with one filter
    * byte, so the decompressed size is one extra byte per row.
    *
    * If buf is NULL, only palette data is being requested and the IDAT
    * stream does not need to be inflated.
    */

    if(buf) {

        size_t inflated_size;

        inflated_size = ((size_t)width + 1) * (size_t)height;

        if(inflated_size > UINT_MAX) {
            goto readpng_abort;
        }

        inflated_data = malloc(inflated_size);

        if(!inflated_data) {
            goto readpng_abort;
        }

        zlib_stream.avail_out = (uInt)inflated_size;
        zlib_stream.next_out = inflated_data;
    }

    /*
    * Walk the remaining PNG chunks. The loader only needs PLTE for the
    * palette and IDAT for pixel data. Other chunks are skipped.
    *
    * Chunk layout:
    *   4 bytes - data length
    *   4 bytes - chunk name
    *   N bytes - chunk data
    *   4 bytes - CRC
    */

    while(png_data_ptr < (png_data + data_size)) {

        struct png_chunk_header chunk_header;
        uint32_t chunk_size;
        uint32_t chunk_name;
        ptrdiff_t bytes_left;

        bytes_left = (png_data + data_size) - png_data_ptr;

        /*
        * Make sure there is enough data left to read the chunk header.
        * Using memcpy() avoids alignment and aliasing issues on 64-bit
        * builds.
        */
        if(bytes_left < (ptrdiff_t)sizeof(chunk_header)) {
            goto readpng_abort;
        }

        memcpy(&chunk_header, png_data_ptr, sizeof(chunk_header));

        chunk_size = SwapMSB32(chunk_header.chunk_size);
        chunk_name = chunk_header.chunk_name;

        png_data_ptr += sizeof(chunk_header);
        bytes_left = (png_data + data_size) - png_data_ptr;

        /*
        * Make sure the declared chunk payload and CRC are still inside
        * the data buffer before reading or skipping the chunk.
        */
        if(bytes_left < 4 || chunk_size > (uint32_t)(bytes_left - 4)) {
            goto readpng_abort;
        }

        /*
        * IEND marks the formal end of the PNG stream. Stop here so
        * palette-only reads do not keep walking into trailing data.
        */
        if(chunk_name == SwapMSB32(PNG_CHUNK_IEND)) {
            png_data_ptr += chunk_size + 4;
            break;
        }

        /*
        * PLTE contains the image palette. This loader currently writes
        * palette entries in the engine's 32-bit palette format.
        */
        if(pal && chunk_name == SwapMSB32(PNG_CHUNK_PLTE)) {

            unsigned int ncolors = chunk_size / 3;
            unsigned int i;
            int *pal32 = (int*)pal;

            if(chunk_size % 3 != 0 || ncolors > 256) {
                goto readpng_abort;
            }

            for(i = 0; i < ncolors; i++) {
                pal32[i] = colour32(png_data_ptr[0], png_data_ptr[1], png_data_ptr[2]);
                png_data_ptr += 3;
            }

            /*
            * Skip the PLTE CRC after reading the palette payload.
            */
            png_data_ptr += 4;

        } else if(buf && chunk_name == SwapMSB32(PNG_CHUNK_IDAT)) {

            /*
            * IDAT chunks contain one continuous zlib stream split across
            * one or more PNG chunks. Feed each IDAT payload to inflate()
            * in file order.
            */
            zlib_stream.avail_in = chunk_size;
            zlib_stream.next_in = png_data_ptr;

            z_lib_result = inflate(&zlib_stream, Z_SYNC_FLUSH);

            /*
            * Move past the IDAT payload and CRC before checking whether
            * zlib reached the end of the stream.
            */
            png_data_ptr += chunk_size + 4;

            if(z_lib_result == Z_STREAM_END) {
                break;
            }

            if(z_lib_result != Z_OK) {
                printf("inflate failed: %i\n", z_lib_result);
                goto readpng_abort;
            }

        } else {

            /*
            * Unused chunk. Skip payload and CRC.
            */
            png_data_ptr += chunk_size + 4;
        }
    }

    /*
    * If the caller requested pixel data, the zlib stream must have
    * reached its natural end and the output buffer must be filled
    * with exact pixel data. If not, something went wrong.
    */

    if(buf && (z_lib_result != Z_STREAM_END || zlib_stream.avail_out != 0)) {
        goto readpng_abort;
    }

    /*
    * Decode the decompressed scanlines into the destination buffer.
    * PNG filtering is reversed here after all IDAT data has been
    * inflated.
    */

    if(buf) {
        png_decode_data(buf, inflated_data, max_width, max_height);
    }

    if(zlib_initialized) {
        inflateEnd(&zlib_stream);
    }

    free(inflated_data);
    free(png_data);
    return 1;

readpng_abort:

    /*
    * Shared cleanup path for allocation, file read, PNG chunk, and zlib
    * failures.
    */
    
    if(zlib_initialized) {
        inflateEnd(&zlib_stream);
    }

    free(inflated_data);
    free(png_data);
    return 0;
}

// ============================== GIF loading ===============================

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

    if(readpackfile(image_load_handle, &gif_header, sizeof(gifheaderstruct)) != sizeof(gifheaderstruct))
    {
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
        return 0;
    }
    if(gif_header.magic[0] != 'G' || gif_header.magic[1] != 'I' || gif_header.magic[2] != 'F')
    {
        closepackfile(image_load_handle);
        image_load_handle = HANDLE_UNUSED;
        return 0;
    }

    gif_header.screenwidth = SwapLSB16(gif_header.screenwidth);
    gif_header.screenheight = SwapLSB16(gif_header.screenheight);

    image_res.width = gif_header.screenwidth;
    image_res.height = gif_header.screenheight;

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
            if(readpackfile(image_load_handle, &iblock, sizeof(iblock)) != sizeof(iblock))
            {
                return 0;
            }

            iblock.left = SwapLSB16(iblock.left);
            iblock.top = SwapLSB16(iblock.top);
            iblock.width = SwapLSB16(iblock.width);
            iblock.height = SwapLSB16(iblock.height);

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
* Caskey, Damon V.
* Original date and author unknown, reworked 2026-06-01.
*
* Opens a PNG or GIF image from disk or the active pack file and records the
* detected image type for later readimage() and closeimage() calls.
*
* If filename already includes a supported extension, only that exact file is
* tested. This avoids opening a valid image more than once and leaking pack
* file handles.
*
* If filename has no supported extension, .png is tried first, then .gif.
*
* Returns 1 on success, or 0 if no supported image could be opened.
*/
static int openimage(char *filename, char *packfile) {

    char fnam[MAX_BUFFER_LEN];
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

            return 0;
        }

        if(stricmp(ext, ".gif") == 0) {

            if(opengif(filename, packfile)) {
                open_type = OT_GIF;
                return 1;
            }

            return 0;
        }

        printf("\n\n Error: Unsupported image format '%s' for file '%s'. Use PNG or GIF images.\n", ext, filename);
        return 0;
    }

    /*
    * No extension was supplied. Try PNG first, then GIF.
    * Return immediately on success so exactly one image 
    * handle remains active for readimage().
    */

    snprintf(fnam, sizeof(fnam), "%s.png", filename);
    if(openpng(fnam, packfile)) {
        open_type = OT_PNG;
        return 1;
    }

    snprintf(fnam, sizeof(fnam), "%s.gif", filename);
    if(opengif(fnam, packfile)) {
        open_type = OT_GIF;
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
