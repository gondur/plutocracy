/******************************************************************************\
 Plutocracy - Copyright (C) 2008 - Michael Levin

 This program is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation; either version 2, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
\******************************************************************************/

/* Classes and functions that manipulate strings. */

#include "c_shared.h"

/* This file legitimately uses standard library string functions */
#ifdef PLUTOCRACY_LIBC_ERRORS
#undef strlen
#endif

/* The mask is applied to the hash function as a quick way of limiting the
   hash index to the table range */
#define TRANSLATIONS_MASK 0xff
#define TRANSLATIONS_MAX (TRANSLATIONS_MASK + 1)

/* Translation database type */
typedef struct translation {
        char *value, key[60];
} translation_t;

static translation_t translations[TRANSLATIONS_MAX];
static int translations_len;

/******************************************************************************\
 These functions parse variable argument lists into a static buffer and return
 a pointer to it. You can also pass a pointer to an integer to get the length
 of the output. Not thread safe, but very convenient. Note that after repeated
 calls to these functions, C_vanv will eventually run out of buffers and start
 overwriting old ones. The idea for these functions comes from the Quake 3
 source code.
\******************************************************************************/
char *C_vanv(int *plen, const char *fmt, va_list va)
{
	static char buffer[C_VA_BUFFERS][C_VA_BUFFER_SIZE];
	static int which;
	int len;

	which++;
	if (which >= C_VA_BUFFERS)
	        which = 0;
	len = vsnprintf(buffer[which], sizeof (buffer[which]), fmt, va);
	if (plen)
		*plen = len;
	return buffer[which];
}

char *C_van(int *plen, const char *fmt, ...)
{
	va_list va;
	char *string;

	va_start(va, fmt);
	string = C_vanv(plen, fmt, va);
	va_end(va);
	return string;
}

char *C_va(const char *fmt, ...)
{
	va_list va;
	char *string;

	va_start(va, fmt);
	string = C_vanv(NULL, fmt, va);
	va_end(va);
	return string;
}

/******************************************************************************\
 Skips any space characters in the string.
\******************************************************************************/
char *C_skip_spaces(const char *str)
{
        char ch;

        ch = *str;
        while (C_is_space(ch))
                ch = *(++str);
        return (char *)str;
}

/******************************************************************************\
 Equivalent to the standard library strncpy, but ensures that there is always
 a NUL terminator. Sometimes known as "strncpyz". Can copy overlapped strings.
 Returns the length of the source string.
\******************************************************************************/
int C_strncpy(char *dest, const char *src, int len)
{
        int src_len;

        if (!dest)
                return 0;
        if (!src) {
                if (len > 0)
                        dest[0] = NUL;
                return 0;
        }
        src_len = (int)strlen(src);
        if (src_len > --len) {
                C_debug("dest (%d bytes) too short to hold src (%d bytes)",
                        len, src_len);
                src_len = len;
        }
        memmove(dest, src, src_len);
        dest[src_len] = NUL;
        return src_len;
}

/******************************************************************************\
 Equivalent to the standard library strlen but returns zero if the string is
 NULL.
\******************************************************************************/
int C_strlen(const char *string)
{
        if (!string)
                return 0;
        return (int)strlen(string);
}

/******************************************************************************\
 Allocates new heap memory for and duplicates a string. Doesn't crash if the
 string passed in is NULL.
\******************************************************************************/
char *C_strdup_full(const char *file, int line, const char *function,
                    const char *str)
{
        char *new_str;
        size_t len;

        if (!str)
                return NULL;
        len = strlen(str) + 1;
        new_str = C_realloc_full(file, line, function, NULL, len);
        memcpy(new_str, str, len);
        return new_str;
}

/******************************************************************************\
 Parses an HTML-style hexadecimal color string (#AARRGGBB). It may or may not
 have a '#' in front.
\******************************************************************************/
static struct {
        const char *name;
        unsigned int value;
} colors[] = {
        { "clear", 0x00000000 },

        /* Full-bright colors */
        { "white", 0xffffffff },
        { "black", 0xff000000 },
        { "red", 0xffff0000 },
        { "green", 0xff00ff00 },
        { "blue", 0xff0000ff },
        { "yellow", 0xffffff00 },
        { "cyan", 0xff00ffff },
        { "pink", 0xffff00ff },

        /* Tango colors */
        { "butter1", 0xfffce94f },
        { "butter2", 0xffedd400 },
        { "butter3", 0xffc4a000 },
        { "orange1", 0xfffcaf3e },
        { "orange2", 0xfff57900 },
        { "orange3", 0xffce5c00 },
        { "chocolate1", 0xffe9b96e },
        { "chocolate2", 0xffc17d11 },
        { "chocolate3", 0xff8f5902 },
        { "chameleon1", 0xff8ae234 },
        { "chameleon2", 0xff73d216 },
        { "chameleon3", 0xff4e9a06 },
        { "skyblue1", 0xff729fcf },
        { "skyblue2", 0xff3465a4 },
        { "skyblue3", 0xff204a87 },
        { "plum1", 0xffad7fa8 },
        { "plum2", 0xff75507b },
        { "plum3", 0xff5c3566 },
        { "scarletred1", 0xffef2929 },
        { "scarletred2", 0xffcc0000 },
        { "scarletred3", 0xffa40000 },
        { "aluminium1", 0xffeeeeec },
        { "aluminium2", 0xffd3d7cf },
        { "aluminium3", 0xffbabdb6 },
        { "aluminium4", 0xff888a85 },
        { "aluminium5", 0xff555753 },
        { "aluminium6", 0xff2e3436 },
};

c_color_t C_color_string(const char *string)
{
        int i;

        if (string[0] == '#')
                string++;
        for (i = 0; i < sizeof (colors) / sizeof (*colors); i++)
                if (!strcasecmp(colors[i].name, string))
                        return C_color_32(colors[i].value);
        return C_color_32(strtoul(string, NULL, 16));
}

/******************************************************************************\
 Wraps a string in double-quotes and escapes any special characters. Returns
 a statically allocated string.
\******************************************************************************/
char *C_escape_string(const char *str)
{
        static char buf[4096];
        char *pbuf;

        buf[0] = '"';
        for (pbuf = buf + 1; *str && pbuf <= buf + sizeof (buf) - 3; str++) {
                if (*str == '"' || *str == '\\')
                        *pbuf++ = '\\';
                else if (*str == '\n') {
                        *pbuf++ = '\\';
                        *pbuf = 'n';
                        continue;
                } else if(*str == '\t') {
                        *pbuf++ = '\\';
                        *pbuf = 't';
                        continue;
                } else if (*str == '\r')
                        continue;
                *pbuf++ = *str;
        }
        *pbuf++ = '"';
        *pbuf = NUL;
        return buf;
}

/******************************************************************************\
 Returns the number of bytes in a single UTF-8 character:
 http://www.cl.cam.ac.uk/%7Emgk25/unicode.html#utf-8
\******************************************************************************/
int C_utf8_size(unsigned char first_byte)
{
        /* U-00000000 – U-0000007F (ASCII) */
        if (first_byte < 192)
                return 1;

        /* U-00000080 – U-000007FF */
        else if (first_byte < 224)
                return 2;

        /* U-00000800 – U-0000FFFF */
        else if (first_byte < 240)
                return 3;

        /* U-00010000 – U-001FFFFF */
        else if (first_byte < 248)
                return 4;

        /* U-00200000 – U-03FFFFFF */
        else if (first_byte < 252)
                return 5;

        /* U-04000000 – U-7FFFFFFF */
        return 6;
}

/******************************************************************************\
 Returns the index of the [n]th UTF-8 character in [str].
\******************************************************************************/
int C_utf8_index(char *str, int n)
{
        int i, len;

        for (i = 0; n > 0; n--)
                for (len = C_utf8_size(str[i]); len > 0; len--, i++)
                        if (!str[i])
                                return i;
        return i;
}

/******************************************************************************\
 Equivalent to the standard library strlen but returns zero if the string is
 NULL. If [chars] is not NULL, the number of UTF-8 characters will be output
 to it. Note that if the number of UTF-8 characters is not needed, C_strlen()
 is preferrable to this function.
\******************************************************************************/
int C_utf8_strlen(const char *str, int *chars)
{
        int i, str_len, char_len;

        if (!str || !str[0]) {
                if (chars)
                        *chars = 0;
                return 0;
        }
        char_len = C_utf8_size(*str);
        for (i = 0, str_len = 1; str[i]; char_len--, i++)
                if (char_len < 1) {
                        str_len++;
                        char_len = C_utf8_size(str[i]);
                }
        if (chars)
                *chars = str_len;
        return i;
}

/******************************************************************************\
 One UTF-8 character from [src] will be copied to [dest]. The index of the
 current position in [dest], [dest_i] and the index in [src], [src_i], will
 be advanced accordingly. [dest] will not be allowed to overrun [dest_sz]
 bytes. Returns the size of the UTF-8 character or 0 if there is not enough
 room or if [src] starts with NUL.
\******************************************************************************/
int C_utf8_append(char *dest, int *dest_i, int dest_sz, const char *src)
{
        int len, char_len;

        if (!*src)
                return 0;
        char_len = C_utf8_size(*src);
        if (*dest_i + char_len > dest_sz)
                return 0;
        for (len = char_len; *src && len > 0; len--)
                dest[(*dest_i)++] = *src++;
        return char_len;
}

/******************************************************************************\
 Convert a Unicode token to a UTF-8 character sequence. The length of the
 token in bytes is output to [plen], which can be NULL.
\******************************************************************************/
char *C_utf8_encode(unsigned int unicode, int *plen)
{
        static char buf[7];
        int len;

        /* ASCII is an exception */
        if (unicode <= 0x7f) {
                buf[0] = unicode;
                buf[1] = NUL;
                if (plen)
                        *plen = 1;
                return buf;
        }

        /* Size of the sequence depends on the range */
        if (unicode <= 0xff)
                len = 2;
        else if (unicode <= 0xffff)
                len = 3;
        else if (unicode <= 0x1fffff)
                len = 4;
        else if (unicode <= 0x3FFFFFF)
                len = 5;
        else if (unicode <= 0x7FFFFFF)
                len = 6;
        else {
                C_warning("Invalid Unicode character 0x%x", unicode);
                buf[0] = NUL;
                if (plen)
                        *plen = 0;
                return buf;
        }
        if (plen)
                *plen = len;

        /* The first byte is 0b*110x* and the rest are 0b10xxxxxx */
        buf[0] = 0xfc << (6 - len);
        while (--len > 0) {
                buf[len] = 0x80 + (unicode & 0x3f);
                unicode >>= 6;
        }
        buf[0] += unicode;
        return buf;
}

/******************************************************************************\
 A string hashing function due to Dan Bernstein of comp.lang.c.
 http://www.cse.yorku.ca/~oz/hash.html
\******************************************************************************/
unsigned int C_hash_djb2(const char *str)
{
        unsigned int hash = 5381;
        int c;

        while ((c = *str++))
                hash = (hash << 5) + hash + (unsigned int)c;
        return hash;
}

/******************************************************************************\
 Find the index of where a string would go in the translations table. The
 translation keys are not case-sensitive.
\******************************************************************************/
static int translations_index(const char *key)
{
        int i;

        for (i = (int)(C_hash_djb2(key) & TRANSLATIONS_MASK);
             translations[i].value && strcasecmp(translations[i].key, key);
             i = (i + 1) & TRANSLATIONS_MASK);
        return i;
}

/******************************************************************************\
 Retrieve a translated string from the translation database. If the token is
 not found in the current translation database [fallback] is returned.
\******************************************************************************/
const char *C_str(const char *key, const char *stock)
{
        int index;

        if (translations_len < 1)
                return stock;
        index = translations_index(key);
        if (translations[index].value)
                return translations[index].value;
        return stock;
}

/******************************************************************************\
 Insert a new entry into the translation hash table.
\******************************************************************************/
static int lang_key_value(const char *key, const char *value)
{
        int index;

        if (!value) {
                C_warning("Language file has no value for key '%s'", key);
                return TRUE;
        }

        /* If this is a freshly generated file and there is no translation yet,
           the string may be blank */
        if (!value[0])
                return TRUE;

        /* Always leave one entry in the hash table empty so that the index
           loop will be able to quit if it can't find a match and the table
           is full */
        if (translations_len >= TRANSLATIONS_MAX - 1) {
                C_warning("Maximum translations reached (key '%s')", key);
                return FALSE;
        }

        index = translations_index(key);
        if (translations[index].value) {
                C_warning("Already have a translation for key '%s'", key);
                return TRUE;
        }
        C_strncpy_buf(translations[index].key, key);
        translations[index].value = C_strdup(value);
        translations_len++;
        return TRUE;
}

/******************************************************************************\
 Load the translation database file in lang/[c_lang] file and parse it. Format
 is the same as that of a normal configuration file.
\******************************************************************************/
void C_init_lang(void)
{
        c_token_file_t tf;

        C_var_unlatch(&c_lang);
        if (!c_lang.value.s[0] ||
            !C_token_file_init(&tf, C_va("lang/%s", c_lang.value.s)))
                return;
        C_debug("Using language file '%s'", tf.filename);
        C_token_file_parse_pairs(&tf, lang_key_value);
        C_token_file_cleanup(&tf);
}

/******************************************************************************\
 Free all the strings when exiting to prevent false-alarm leak detections.
\******************************************************************************/
void C_cleanup_lang(void)
{
        int i;

        if (translations_len < 1)
                return;
        C_debug("Cleaning up translation database");
        for (i = 0; i < TRANSLATIONS_MAX; i++)
                C_free(translations[i].value);
}

