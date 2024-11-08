/**
 * xrdp: A Remote Desktop Protocol server.
 *
 * Copyright (C) Jay Sorg 2004-2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * fonts
 */

/*
  The fv1 files contain
  Font File Header (just one)
    FNT1       4 bytes
    Font Name  32 bytes
    Font Size  2 bytes
    Font Style 2 bytes
    Pad        8 bytes
  Font Data (repeats)
    Width      2 bytes
    Height     2 bytes
    Baseline   2 bytes
    Offset     2 bytes
    Incby      2 bytes
    Pad        6 bytes
    Glyph Data var, see FONT_DATASIZE macro
*/

#if defined(HAVE_CONFIG_H)
#include <config_ac.h>
#endif

#include "xrdp.h"
#include "log.h"

#if 0 /* not used */
static char w_char[] =
{
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x08, 0x20, 0x80,
    0x08, 0x50, 0x80,
    0x04, 0x51, 0x00,
    0x04, 0x51, 0x00,
    0x04, 0x51, 0x00,
    0x02, 0x8a, 0x00,
    0x02, 0x8a, 0x00,
    0x02, 0x8a, 0x00,
    0x01, 0x04, 0x00,
    0x01, 0x04, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};
#endif

// Unicode definitions
#define UNICODE_WHITE_SQUARE 0x25a1

// First character allocated in the 'struct xrdp_font.chars' array
#define FIRST_CHAR ' '

/*****************************************************************************/
struct xrdp_font *
xrdp_font_create(struct xrdp_wm *wm)
{
    struct xrdp_font *self;
    struct stream *s;
    int fd;
    int b;
    int i;
    unsigned int char_count;
    int datasize;
    int file_size;
    struct xrdp_font_char *f;
    char file_path[256];

    LOG_DEVEL(LOG_LEVEL_TRACE, "in xrdp_font_create");
    g_snprintf(file_path, 255, "%s/%s", XRDP_SHARE_PATH, DEFAULT_FONT_NAME);

    if (!g_file_exist(file_path))
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_font_create: error font file [%s] does not exist",
            file_path);
        return 0;
    }

    file_size = g_file_get_size(file_path);

    if (file_size < 1)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_font_create: error reading font from file [%s]",
            file_path);
        return 0;
    }

    self = (struct xrdp_font *)g_malloc(sizeof(struct xrdp_font), 1);
    if (self == NULL)
    {
        LOG(LOG_LEVEL_ERROR, "xrdp_font_create: "
            "Can't allocate memory for font");
        return self;
    }
    self->wm = wm;
    make_stream(s);
    init_stream(s, file_size + 1024);
    fd = g_file_open(file_path);

    if (fd < 0)
    {
        LOG(LOG_LEVEL_ERROR,
            "xrdp_font_create: Can't open %s - %s", file_path,
            g_get_strerror());
        g_free(self);
        self = NULL;
    }
    else
    {
        b = g_file_read(fd, s->data, file_size + 1024);
        g_file_close(fd);

        // Got at least a header?
        if (b < (4 + 32 + 2 + 2 + 8))
        {
            LOG(LOG_LEVEL_ERROR,
                "xrdp_font_create: Font %s is truncated", file_path);
            g_free(self);
            self = NULL;
        }
        else
        {
            s->end = s->data + b;
            in_uint8s(s, 4);
            in_uint8a(s, self->name, 32);
            in_uint16_le(s, self->size);
            in_uint16_le(s, self->style);
            in_uint8s(s, 8);
            char_count = FIRST_CHAR;

            while (!s_check_end(s))
            {
                if (!s_check_rem(s, 16))
                {
                    LOG(LOG_LEVEL_WARNING,
                        "xrdp_font_create: "
                        "Can't parse header for character U+%X", char_count);
                    break;
                }

                if (char_count >= MAX_FONT_CHARS)
                {
                    LOG(LOG_LEVEL_WARNING,
                        "xrdp_font_create: "
                        "Ignoring characters >= U+%x", MAX_FONT_CHARS);
                    break;
                }

                f = self->chars + char_count;
                in_sint16_le(s, i);
                f->width = i;
                in_sint16_le(s, i);
                f->height = i;
                in_sint16_le(s, i);
                f->baseline = i;
                in_sint16_le(s, i);
                f->offset = i;
                in_sint16_le(s, i);
                f->incby = i;
                in_uint8s(s, 6);
                datasize = FONT_DATASIZE(f);

                if (datasize < 0 || datasize > 512)
                {
                    /* shouldn't happen */
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "datasize for U+%x wrong "
                        "width %d, height %d, datasize %d",
                        char_count, f->width, f->height, datasize);
                    break;
                }

                if (!s_check_rem(s, datasize))
                {
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "Not enough data for character U+%X", char_count);
                    break;
                }

                if ((f->data = (char *)g_malloc(datasize, 0)) == NULL)
                {
                    LOG(LOG_LEVEL_ERROR,
                        "xrdp_font_create: "
                        "Allocation error for character U+%X", char_count);
                    break;
                }
                in_uint8a(s, f->data, datasize);

                ++char_count;
            }

            self->char_count = char_count;
            if (char_count <= FIRST_CHAR)
            {
                /* We read no characters from the font */
                xrdp_font_delete(self);
                self = NULL;
            }
            else
            {
                // Find a default glyph
                if (char_count > UNICODE_WHITE_SQUARE)
                {
                    self->default_char = &self->chars[UNICODE_WHITE_SQUARE];
                }
                else if (char_count > '?')
                {
                    self->default_char = &self->chars['?'];
                }
                else
                {
                    self->default_char = &self->chars[FIRST_CHAR];
                }
            }
        }
    }

    free_stream(s);
    /*
      self->font_items[0].offset = -4;
      self->font_items[0].baseline = -16;
      self->font_items[0].width = 24;
      self->font_items[0].height = 16;
      self->font_items[0].data = g_malloc(3 * 16, 0);
      g_memcpy(self->font_items[0].data, w_char, 3 * 16);
    */
    LOG_DEVEL(LOG_LEVEL_TRACE, "out xrdp_font_create");
    return self;
}

/*****************************************************************************/
/* free the font and all the items */
void
xrdp_font_delete(struct xrdp_font *self)
{
    unsigned int i;

    if (self == 0)
    {
        return;
    }

    for (i = FIRST_CHAR; i < self->char_count; i++)
    {
        g_free(self->chars[i].data);
    }

    g_free(self);
}

/*****************************************************************************/
/* compare the two font items returns 1 if they match */
int
xrdp_font_item_compare(struct xrdp_font_char *font1,
                       struct xrdp_font_char *font2)
{
    int datasize;

    if (font1 == 0)
    {
        return 0;
    }

    if (font2 == 0)
    {
        return 0;
    }

    if (font1->offset != font2->offset)
    {
        return 0;
    }

    if (font1->baseline != font2->baseline)
    {
        return 0;
    }

    if (font1->width != font2->width)
    {
        return 0;
    }

    if (font1->height != font2->height)
    {
        return 0;
    }

    datasize = FONT_DATASIZE(font1);

    if (g_memcmp(font1->data, font2->data, datasize) == 0)
    {
        return 1;
    }

    return 0;
}
