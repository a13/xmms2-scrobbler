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
#include "submission.h"

Submission *
submission_new (StrBuf *sb, SubmissionType type)
{
	Submission *submission;

	submission = malloc (sizeof (Submission));
	submission->sb = sb;
	submission->type = type;

	return submission;
}

Submission *
now_playing_submission_new (xmmsv_t *dict)
{
	StrBuf *sb;
	const char *artist, *title, *val_s;
	int32_t val_i;
	int s;

	/* artist is required */
	s = xmmsv_dict_entry_get_string (dict, "artist", &artist);
	if (!s)
		return NULL;

	/* title is required */
	s = xmmsv_dict_entry_get_string (dict, "title", &title);
	if (!s)
		return NULL;

	sb = strbuf_new ();

	/* note that the session id isn't written here yet, we'll do
	 * that just before we submit the data.
	 */

	/* artist */
	strbuf_append (sb, "a=");
	strbuf_append_encoded (sb, (const uint8_t *) artist);

	/* title */
	strbuf_append (sb, "&t=");
	strbuf_append_encoded (sb, (const uint8_t *) title);

	/* album */
	strbuf_append (sb, "&b=");

	s = xmmsv_dict_entry_get_string (dict, "album", &val_s);
	if (s)
		strbuf_append_encoded (sb, (const uint8_t *) val_s);

	/* duration in seconds */
	strbuf_append (sb, "&l=");

	s = xmmsv_dict_entry_get_int (dict, "duration", &val_i);
	if (s) {
		char buf32[32];

		sprintf (buf32, "%i", val_i / 1000);
		strbuf_append (sb, buf32);
	}

	/* position of the track on the album.
	 * xmms2 doesn't enforce any format for this field, so we're not
	 * submitting it at all.
	 */
	strbuf_append (sb, "&n=");

	/* musicbrainz track id */
	strbuf_append (sb, "&m=");

	s = xmmsv_dict_entry_get_string (dict, "track_id", &val_s);
	if (s)
		strbuf_append (sb, val_s);

	return submission_new (sb, SUBMISSION_TYPE_NOW_PLAYING);
}

Submission *
profile_submission_new (xmmsv_t *dict, uint32_t seconds_played,
                        time_t started_playing)
{
	StrBuf *sb;
	const char *artist, *title, *val_s;
	char buf32[32];
	int32_t val_i;
	int s;

	/* duration in seconds is required */
	s = xmmsv_dict_entry_get_int (dict, "duration", &val_i);
	if (!s)
		return NULL;

	if (seconds_played < 240 && seconds_played < (val_i / 2000)) {
		fprintf (stderr, "seconds_played FAIL: %u\n", seconds_played);
		return NULL;
	}

	/* artist is required */
	s = xmmsv_dict_entry_get_string (dict, "artist", &artist);
	if (!s)
		return NULL;

	/* title is required */
	s = xmmsv_dict_entry_get_string (dict, "title", &title);
	if (!s)
		return NULL;

	sb = strbuf_new ();

	/* note that the session id isn't written here yet, we'll do
	 * that just before we submit the data.
	 */

	/* artist */
	strbuf_append (sb, "a[0]=");
	strbuf_append_encoded (sb, (const uint8_t *) artist);

	/* title */
	strbuf_append (sb, "&t[0]=");
	strbuf_append_encoded (sb, (const uint8_t *) title);

	/* timestamp */
	sprintf (buf32, "%lu", started_playing);
	strbuf_append (sb, "&i[0]=");
	strbuf_append (sb, buf32);

	/* source: chosen by user */
	strbuf_append (sb, "&o[0]=P");

	/* rating: unknown */
	strbuf_append (sb, "&r[0]=");

	/* duration in seconds */
	sprintf (buf32, "%i", val_i / 1000);

	strbuf_append (sb, "&l[0]=");
	strbuf_append (sb, buf32);

	/* album */
	strbuf_append (sb, "&b[0]=");

	s = xmmsv_dict_entry_get_string (dict, "album", &val_s);
	if (s) {
		strbuf_append_encoded (sb, (const uint8_t *) val_s);
	}

	/* position of the track on the album.
	 * xmms2 doesn't enforce any format for this field, so we're not
	 * submitting it at all.
	 */
	strbuf_append (sb, "&n[0]=");

	/* musicbrainz track id */
	strbuf_append (sb, "&m[0]=");

	s = xmmsv_dict_entry_get_string (dict, "track_id", &val_s);
	if (s)
		strbuf_append (sb, val_s);

	return submission_new (sb, SUBMISSION_TYPE_PROFILE);
}

void
submission_free (Submission *s)
{
	strbuf_free (s->sb);
	free (s);
}
