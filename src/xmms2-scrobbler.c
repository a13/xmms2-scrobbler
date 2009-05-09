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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <poll.h>
#include <xmmsclient/xmmsclient.h>
#include <pthread.h>
#include <curl/curl.h>
#include <signal.h>
#include "queue.h"
#include "submission.h"
#include "md5.h"

#define PROTOCOL "1.2"
#define CLIENT_ID "xm2"

#define VERSION "0.2.9"

#define INVALID_MEDIA_ID -1

static xmmsc_connection_t *conn;
static int32_t current_id = INVALID_MEDIA_ID;
static uint32_t seconds_played;
static time_t started_playing, last_unpause;
static int hard_failure_count;

static char user[64], hashed_password[33], proxy_host[128];
static int proxy_port;
static char session_id[256], np_url[256], subm_url[256];

static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static Queue submissions;
static pthread_mutex_t submissions_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool need_handshake = true;
static bool shutdown_thread = false;
static bool keep_running = true;

static struct sigaction sig;

static void
signal_handler (int sig)
{
	if (sig == SIGINT)
		keep_running = false;
}

static size_t
handle_handshake_reponse (void *rawptr, size_t size, size_t nmemb,
                          void *data)
{
	size_t total = size * nmemb, left = total;
	char *ptr = rawptr, *newline;
	int len;

	newline = memchr (ptr, '\n', left);
	if (!newline) {
		fprintf (stderr, "no newline (1)\n");
		return total;
	}

	*newline = 0;

	if (strcmp (ptr, "OK")) {
		fprintf (stderr, "handshake failed\n");
		return total;
	}

	len = newline - ptr + 1;

	left -= len;
	ptr += len;

	newline = memchr (ptr, '\n', left);
	if (!newline) {
		fprintf (stderr, "no newline (1)\n");
		return total;
	}

	*newline = 0;

	len = newline - ptr + 1;

	if (len > 255) {
		fprintf (stderr, "session ID is too long (%i characters)\n", len);
		return total;
	}

	strcpy (session_id, ptr);

	left -= len;
	ptr += len;

	/* now playing URL */
	newline = memchr (ptr, '\n', left);
	if (!newline) {
		fprintf (stderr, "no newline (2)\n");
		return total;
	}

	*newline = 0;

	len = newline - ptr + 1;

	if (len > 255) {
		fprintf (stderr, "now_playing URL is too long "
		         "(%i characters)\n", len);
		return total;
	}

	strcpy (np_url, ptr);

	left -= len;
	ptr += len;

	/* submission URL */
	newline = memchr (ptr, '\n', left);
	if (!newline) {
		printf("no newline (3)\n");
		return total;
	}

	*newline = 0;

	len = newline - ptr + 1;

	if (len > 255) {
		fprintf (stderr, "submission URL is too long "
		         "(%i characters)\n", len);
		return total;
	}

	strcpy (subm_url, ptr);

	fprintf (stderr, "got:\n'%s' '%s' '%s'\n",
	         session_id, np_url, subm_url);

	need_handshake = false;
	hard_failure_count = 0;

	return total;
}

static size_t
handle_submission_reponse (void *ptr, size_t size, size_t nmemb,
                           void *data)
{
	size_t total = size * nmemb;
	bool *is_success = data;
	char *newline;

	newline = memchr (ptr, '\n', total);
	if (!newline) {
		fprintf (stderr, "no newline\n");
		return total;
	}

	*newline = 0;
	fprintf (stderr, "response: '%s'\n", (char *) ptr);

	if (!strcmp (ptr, "BADSESSION")) {
		/* need to perform handshake again */
		need_handshake = true;
		fprintf (stderr, "BADSESSION\n");
	} else if (!strcmp (ptr, "OK")) {
		/* submission succeeded */
		fprintf (stderr, "success \\o/\n");
		*is_success = true;
	} else if (total >= strlen ("FAILED ")) {
		fprintf (stderr, "couldn't submit: '%s'\n", (char *) ptr);
	}

	return total;
}

static void
set_proxy (CURL *curl)
{
	if (!*proxy_host)
		return;

	curl_easy_setopt (curl, CURLOPT_PROXY, proxy_host);

	if (proxy_port)
		curl_easy_setopt (curl, CURLOPT_PROXYPORT, (long) proxy_port);
}

/* perform the handshake and return true on success, false otherwise. */
static bool
do_handshake (void)
{
	CURL *curl;
	char hashed[64], post_data[512];
	time_t timestamp;
	int pos;

	timestamp = time (NULL);

	/* copy over the hashed password */
	memcpy (hashed, hashed_password, 32);

	/* append the timestamp */
	snprintf (&hashed[32], sizeof (hashed) - 32, "%lu", timestamp);

	pos = snprintf (post_data, sizeof (post_data),
	                "http://post.audioscrobbler.com/"
	                "?hs=true&p=" PROTOCOL "&c=" CLIENT_ID
	                "&v=" VERSION "&u=%s&t=%lu&a=",
	                user, timestamp);

	/* hash the hashed password and timestamp and append the hex string
	 * to 'post_data'.
	 * FIXME: check for buffer overflow.
	 */
	md5 (hashed, &post_data[pos]);

	curl = curl_easy_init ();

	set_proxy (curl);

	curl_easy_setopt (curl, CURLOPT_URL, post_data);
    curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION,
	                  handle_handshake_reponse);
    curl_easy_setopt (curl, CURLOPT_WRITEDATA, NULL);
	curl_easy_perform (curl);
	curl_easy_cleanup (curl);

	return !need_handshake;
}

static bool
handshake_if_needed ()
{
	int delay = 30;

	while (need_handshake) {
		struct timespec ts;
		bool shutdown;

		if (do_handshake ())
			return true;

		delay *= 2;

		/* there's a maximum delay of two hours */
		if (delay > 7200)
			delay = 7200;

#ifdef CLOCK_REALTIME
		clock_gettime (CLOCK_REALTIME, &ts);
#else
		struct timeval tv;

		gettimeofday (&tv, NULL);

		ts.tv_sec = tv.tv_sec;
		ts.tv_nsec = tv.tv_usec * 1000;
#endif

		ts.tv_sec += delay;

		int e;

		do {
			pthread_mutex_lock (&submissions_mutex);
			e = pthread_cond_timedwait (&cond, &submissions_mutex, &ts);
			shutdown = shutdown_thread;
			pthread_mutex_unlock (&submissions_mutex);

			if (shutdown)
				return false;
		} while (e != ETIMEDOUT);
	}

	return true;
}

static void *
curl_thread (void *arg)
{
	CURL *curl;

	pthread_mutex_lock (&submissions_mutex);

	while (!shutdown_thread) {
		/* check whether there's data waiting to be
		 * submitted.
		 */
		if (!queue_peek (&submissions)) {
			pthread_cond_wait (&cond, &submissions_mutex);
			continue;
		}

		pthread_mutex_unlock (&submissions_mutex);

		curl = curl_easy_init ();

		set_proxy (curl);

		curl_easy_setopt (curl, CURLOPT_POST, 1);
		curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION,
		                  handle_submission_reponse);

		while (true) {
			Submission *submission;
			bool shutdown, is_success = false;
			int orig_length;

			pthread_mutex_lock (&submissions_mutex);
			submission = queue_peek (&submissions);
			shutdown = shutdown_thread;
			pthread_mutex_unlock (&submissions_mutex);

			if (!submission || shutdown)
				break;

			if (!handshake_if_needed ())
				break;

			/* append the current session id, but remember
			 * the current length of the string first.
			 */
			orig_length = submission->sb->length;

			strbuf_append (submission->sb, "&s=");
			strbuf_append (submission->sb, session_id);

			fprintf (stderr, "submitting '%s'\n", submission->sb->buf);

			if (submission->type == SUBMISSION_TYPE_NOW_PLAYING)
				curl_easy_setopt (curl, CURLOPT_URL, np_url);
			else
				curl_easy_setopt (curl, CURLOPT_URL, subm_url);

			curl_easy_setopt (curl, CURLOPT_POSTFIELDS,
			                  submission->sb->buf);
			curl_easy_setopt (curl, CURLOPT_WRITEDATA, &is_success);
			curl_easy_perform (curl);

			if (!is_success && !need_handshake &&
			    ++hard_failure_count == 3)
				need_handshake = true;

			if (is_success ||
			    submission->type == SUBMISSION_TYPE_NOW_PLAYING) {
				/* if the submission was successful, or if it was a
				 * now-playing submission, remove the item from the
				 * queue.
				 */
				pthread_mutex_lock (&submissions_mutex);
				queue_pop (&submissions);
				pthread_mutex_unlock (&submissions_mutex);

				submission_free (submission);
			} else if (!is_success) {
				/* if the (profile) submission wasn't successful,
				 * remove the session id from the string again.
				 */
				strbuf_truncate (submission->sb, orig_length);
			}
		}

		curl_easy_cleanup (curl);

		pthread_mutex_lock (&submissions_mutex);
	}

	return NULL;
}

static void
enqueue (Submission *submission)
{
	pthread_mutex_lock (&submissions_mutex);
	queue_push (&submissions, submission);
	pthread_cond_signal (&cond);
	pthread_mutex_unlock (&submissions_mutex);
}

static void
submit_now_playing (xmmsv_t *val)
{
	Submission *submission;
	xmmsv_t *dict;

	dict = xmmsv_propdict_to_dict (val, NULL);
	submission = now_playing_submission_new (dict);
	xmmsv_unref (dict);

	if (submission)
		enqueue (submission);
}

static bool
submit_to_profile (xmmsv_t *val)
{
	Submission *submission;
	xmmsv_t *dict;

	dict = xmmsv_propdict_to_dict (val, NULL);
	submission = profile_submission_new (dict, seconds_played,
	                                     started_playing);
	xmmsv_unref (dict);

	if (submission)
		enqueue (submission);

	return !!submission;
}

static int
on_medialib_get_info2 (xmmsv_t *val, void *udata)
{
	bool reset_current_id = XPOINTER_TO_INT (udata);

	seconds_played += time (NULL) - last_unpause;
	fprintf (stderr, "submitting: seconds_played %i\n", seconds_played);

	/* if we could submit this song we might have to reset
	 * 'current_id', so we don't submit it again.
	 */
	if (submit_to_profile (val) && reset_current_id)
		current_id = INVALID_MEDIA_ID;

	return 0;
}

static int
on_medialib_get_info (xmmsv_t *val, void *udata)
{
	submit_now_playing (val);

	fprintf (stderr, "resetting seconds_played\n");
	last_unpause = started_playing = time (NULL);
	seconds_played = 0;

	return 0;
}

static void
maybe_submit_to_profile (bool reset_current_id)
{
	xmmsc_result_t *mediainfo_result;

	/* check whether we're interesting in this track at all */
	if (current_id == INVALID_MEDIA_ID)
		return;

	mediainfo_result = xmmsc_medialib_get_info (conn, current_id);
	xmmsc_result_notifier_set (mediainfo_result,
	                           on_medialib_get_info2,
	                           XINT_TO_POINTER (reset_current_id));
	xmmsc_result_unref (mediainfo_result);
}

static int
on_playback_current_id (xmmsv_t *val, void *udata)
{
	xmmsc_result_t *mediainfo_result;
	int32_t id = INVALID_MEDIA_ID;

	/* if the submission works, we must NOT reset current_id
	 * because we set it a couple of lines below (to the new
	 * song's ID). we must not overwrite that value.
	 */
	maybe_submit_to_profile (false);

	/* get the new song's medialib id. */
	xmmsv_get_int (val, &id);

	fprintf (stderr, "now playing %u\n", id);

	current_id = id;

	/* request information about this song. */
	mediainfo_result = xmmsc_medialib_get_info (conn, id);
	xmmsc_result_notifier_set (mediainfo_result,
	                           on_medialib_get_info, NULL);
	xmmsc_result_unref (mediainfo_result);

	return 1;
}

static int
on_playback_status (xmmsv_t *val, void *udata)
{
	int s, status;

	s = xmmsv_get_int (val, &status);
	if (!s)
		return 1;

	switch (status) {
		case XMMS_PLAYBACK_STATUS_STOP:
		case XMMS_PLAYBACK_STATUS_PAUSE:
			/* if we could submit this song we need to reset
			 * 'current_id', so we don't submit it again.
			 */
			maybe_submit_to_profile (true);
			break;
		case XMMS_PLAYBACK_STATUS_PLAY:
			last_unpause = time (NULL);
			break;
	}

	return 1;
}

static int
on_quit (xmmsv_t *val, void *udata)
{
	keep_running = false;

	return 0;
}

static void
on_disconnect (void *udata)
{
	keep_running = false;
}

static void
for_each_line (FILE *fp,
               void (*callback) (const char *line, void *user_data),
               void *user_data)
{
	char buf[4096];

	while (fgets (buf, sizeof (buf), fp)) {
		size_t length = strlen (buf);

		if (length < 2)
			continue;

		/* cut the trailing newline */
		buf[length - 1] = 0;

		callback (buf, user_data);
	}
}

static void
handle_config_line (const char *line, void *user_data)
{
	if (!strncmp (line, "user: ", 6)) {
		strncpy (user, &line[6], sizeof (user));
		user[sizeof (user) - 1] = 0;
	} else if (!strncmp (line, "password: ", 10)) {
		/* we only ever need the hashed password :) */
		md5 (&line[10], hashed_password);
	} else if (!strncmp (line, "proxy: ", 7)) {
		strncpy (proxy_host, &line[7], sizeof (proxy_host));
		proxy_host[sizeof (proxy_host) - 1] = 0;
	} else if (!strncmp (line, "proxy_port: ", 12))
		proxy_port = atoi (&line[12]);
}

static bool
load_config ()
{
	FILE *fp;
	const char *dir;
	char buf[XMMS_PATH_MAX];

	dir = xmmsc_userconfdir_get (buf, sizeof (buf));

	if (!dir) {
		fprintf (stderr, "cannot get userconfdir\n");
		return false;
	}

	chdir (dir);

	fp = fopen ("clients/xmms2-scrobbler/config", "r");

	if (!fp) {
		fprintf (stderr, "cannot open config file\n");
		return false;
	}

	for_each_line (fp, handle_config_line, NULL);

	fclose (fp);

	return true;
}

static void
handle_queue_line (const char *line, void *user_data)
{
	StrBuf *sb;

	sb = strbuf_new ();
	strbuf_append (sb, line);

	queue_push (&submissions,
	            submission_new (sb, SUBMISSION_TYPE_PROFILE));
}

static void
load_profile_submissions_queue ()
{
	FILE *fp;
	const char *dir;
	char buf[XMMS_PATH_MAX];

	dir = xmmsc_userconfdir_get (buf, sizeof (buf));

	if (!dir) {
		fprintf (stderr, "cannot get userconfdir\n");
		return;
	}

	chdir (dir);

	fp = fopen ("clients/xmms2-scrobbler/queue", "r");

	if (!fp) {
		fprintf (stderr, "cannot open queue for reading\n");
		return;
	}

	for_each_line (fp, handle_queue_line, NULL);

	fclose (fp);
}

static void
save_profile_submissions_queue ()
{
	FILE *fp;
	const char *dir;
	char buf[XMMS_PATH_MAX];

	dir = xmmsc_userconfdir_get (buf, sizeof (buf));

	if (!dir) {
		fprintf (stderr, "cannot get userconfdir\n");
		return;
	}

	chdir (dir);

	fp = fopen ("clients/xmms2-scrobbler/queue", "w");

	if (!fp) {
		fprintf (stderr, "cannot open queue for writing\n");
		return;
	}

	while (true) {
		Submission *s;

		s = queue_pop (&submissions);
		if (!s)
			break;

		if (s->type == SUBMISSION_TYPE_PROFILE)
			fprintf (fp, "%s\n", s->sb->buf);

		submission_free (s);
	}

	fclose (fp);
}

static void
main_loop ()
{
	struct pollfd fds;

	fds.fd = xmmsc_io_fd_get (conn);

	while (keep_running) {
		fds.events = POLLIN | POLLHUP | POLLERR;
		fds.revents = 0;

		if (xmmsc_io_want_out (conn))
			fds.events |= POLLOUT;

		int e = poll (&fds, 1, -1);

		if (e == -1)
			xmmsc_io_disconnect (conn);
		else if ((fds.revents & POLLERR) == POLLERR)
			xmmsc_io_disconnect (conn);
		else if ((fds.revents & POLLHUP) == POLLHUP)
			xmmsc_io_disconnect (conn);
		else {
			if ((fds.revents & POLLOUT) == POLLOUT)
				xmmsc_io_out_handle (conn);

			if ((fds.revents & POLLIN) == POLLIN)
				xmmsc_io_in_handle (conn);
		}
	}
}

static void
start_logging ()
{
	int fd;
	const char *dir;
	char buf[XMMS_PATH_MAX];

	dir = xmmsc_userconfdir_get (buf, sizeof (buf));

	if (!dir) {
		fprintf (stderr, "cannot get userconfdir\n");
		return;
	}

	chdir (dir);

	fd = creat ("clients/xmms2-scrobbler/logfile.log", 0640);

	if (fd > -1) {
		/* redirect stderr to the log file. */
		dup2 (fd, 2);
	}
}

int
main (int argc, char **argv)
{
	xmmsc_result_t *current_id_broadcast;
	xmmsc_result_t *playback_status_broadcast;
	xmmsc_result_t *quit_broadcast;
	pthread_t thread;
	int s;

	sig.sa_handler = &signal_handler;
	sigaction (SIGINT, &sig, 0);

	start_logging ();

	if (!load_config ())
		return EXIT_FAILURE;

	conn = xmmsc_init ("XMMS2-Scrobbler");

	if (!conn) {
		fprintf (stderr, "OOM\n");

		return EXIT_FAILURE;
	}

	s = xmmsc_connect (conn, NULL);

	if (!s) {
		fprintf (stderr, "cannot connect to xmms2d\n");

		xmmsc_unref (conn);

		return EXIT_FAILURE;
	}

	curl_global_init (CURL_GLOBAL_NOTHING);

	queue_init (&submissions);

	load_profile_submissions_queue ();

	pthread_create (&thread, NULL, curl_thread, NULL);

	/* register the various broadcasts that we're interested in */
	current_id_broadcast = xmmsc_broadcast_playback_current_id (conn);
	xmmsc_result_notifier_set (current_id_broadcast,
	                           on_playback_current_id, NULL);
	xmmsc_result_unref (current_id_broadcast);

	playback_status_broadcast = xmmsc_broadcast_playback_status (conn);
	xmmsc_result_notifier_set (playback_status_broadcast,
	                           on_playback_status, NULL);
	xmmsc_result_unref (playback_status_broadcast);

	quit_broadcast = xmmsc_broadcast_quit (conn);
	xmmsc_result_notifier_set (quit_broadcast, on_quit, NULL);
	xmmsc_result_unref (quit_broadcast);

	xmmsc_disconnect_callback_set (conn, on_disconnect, NULL);

	main_loop ();

	/* tell the curl thread to stop working */
	pthread_mutex_lock (&submissions_mutex);
	shutdown_thread = true;
	pthread_cond_signal (&cond);
	pthread_mutex_unlock (&submissions_mutex);

	/* and wait until it's gone */
	pthread_join (thread, NULL);

	curl_global_cleanup ();

#if 0
	/* disconnect broadcasts and signals */
	xmmsc_result_disconnect (current_id_broadcast);
	xmmsc_result_disconnect (playback_status_broadcast);
	xmmsc_result_disconnect (quit_broadcast);
#endif

	xmmsc_unref (conn);

	save_profile_submissions_queue ();

	return EXIT_SUCCESS;
}
