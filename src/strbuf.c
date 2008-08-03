/*
 * Copyright (c) 2008 Tilman Sauerbeck (tilman at xmms org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "strbuf.h"

#define GOODCHAR(a) ((((a) >= 'a') && ((a) <= 'z')) || \
                     (((a) >= 'A') && ((a) <= 'Z')) || \
                     (((a) >= '0') && ((a) <= '9')) || \
                     ((a) == ':') || \
                     ((a) == '/') || \
                     ((a) == '-') || \
                     ((a) == '.') || \
                     ((a) == '_'))

StrBuf *
strbuf_new (void)
{
	StrBuf *sb;

	sb = malloc (sizeof (StrBuf));

	sb->allocated = 256;
	sb->buf = malloc (sb->allocated);

	sb->length = 0;
	sb->buf[sb->length] = 0;

	return sb;
}

void
strbuf_free (StrBuf *sb)
{
	free (sb->buf);
	free (sb);
}

static void
resize (StrBuf *sb, size_t len)
{
	int needed = sb->length + len + 1;

	if (needed > sb->allocated) {
		int alloc;

		for (alloc = sb->allocated; needed > alloc; alloc *= 2)
			;

		sb->buf = realloc (sb->buf, alloc);
		sb->allocated = alloc;
	}
}

void
strbuf_append (StrBuf *sb, const char *other)
{
	size_t len;

	len = strlen (other);

	resize (sb, len);

	memcpy (sb->buf + sb->length, other, len + 1);
	sb->length += len;
}

void
strbuf_append_encoded (StrBuf *sb, const uint8_t *other)
{
	static const char hex[16] = "0123456789abcdef";
	char *dest = sb->buf + sb->length;
	int len = 0;

	for (const uint8_t *src = other; *src; src++) {
		if (GOODCHAR (*src))
			len++;
		else if (*src == ' ')
			len++;
		else
			len += 3;
	}

	resize (sb, len);

	for (const uint8_t *src = other; *src; src++) {
		if (GOODCHAR (*src)) {
			*dest++ = *src;
		} else if (*src == ' ') {
			*dest++ = '+';
		} else {
			*dest++ = '%';
			*dest++ = hex[((*src & 0xf0) >> 4)];
			*dest++ = hex[(*src & 0x0f)];
		}
	}

	*dest++ = 0;

	sb->length += len;
}

void
strbuf_truncate (StrBuf *sb, int length)
{
	sb->length = length;
	sb->buf[sb->length] = 0;
}
