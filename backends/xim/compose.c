/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * compose.c
 * Copyright (C) 2008 Red Hat, Inc. All rights reserved.
 * 
 * Authors:
 *   Akira TAGOH  <tagoh@redhat.com>
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <locale.h>
#include <langinfo.h>
#include <string.h>
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include "compose.h"


#ifdef GNOME_ENABLE_DEBUG
#define d(e)	e
#else
#define d(e)
#endif

#define _is_whitespace(_p_)			\
	(g_ascii_isspace(_p_) || (_p_) == ':')
#define _is_comment(_p_)			\
	((_p_) == '#')
#define _skip_whitespaces(_p_)				\
	while (*(_p_) && g_ascii_isspace(*(_p_)))	\
			(_p_)++;

typedef enum _SeqError		SeqError;

enum _SeqError {
	SEQ_ERR_INVALID_SEQUENCE,
	SEQ_ERR_SEQUENCE_EXISTS,
};

static gboolean _sequence_list_free(gpointer key,
				    gpointer val,
				    gpointer data);

/*
 * Private functions
 */
static GQuark
sequence_get_error_quark(void)
{
	static GQuark quark = 0;

	if (!quark)
		quark = g_quark_from_static_string("imsettings-sequence-quark");

	return quark;
}

static gint
_sequence_compare(gconstpointer a,
		  gconstpointer b)
{
	return (gulong)a - (gulong)b;
}

static Sequence *
sequence_new(gulong keysym,
	     guint  modifiers,
	     guint  mod_mask)
{
	Sequence *retval = g_new0(Sequence, 1);

	retval->keysym = keysym;
	retval->modifiers = modifiers;
	retval->mod_mask = mod_mask;

	return retval;
}

static void
sequence_free(Sequence *seq)
{
	if (seq->candidates) {
		g_tree_foreach(seq->candidates, _sequence_list_free, NULL);
		g_tree_destroy(seq->candidates);
	}
	g_free(seq->string);
	g_free(seq);
}

static gboolean
_sequence_list_free(gpointer key,
		    gpointer val,
		    gpointer data)
{
	GSList *list = val, *l;

	for (l = list; l != NULL; l = g_slist_next(l))
		sequence_free(list->data);
	g_slist_free(list);

	return FALSE;
}

static gboolean
sequence_add(Sequence  *seq,
	     Sequence  *next,
	     GError   **error)
{
	GSList *list = NULL, *l;

	if (seq->string) {
		g_set_error(error, sequence_get_error_quark(), SEQ_ERR_INVALID_SEQUENCE,
			    "Child sequence won't be matched.");

		return FALSE;
	}
	if (seq->candidates == NULL) {
		seq->candidates = g_tree_new(_sequence_compare);
	}
	if ((list = g_tree_lookup(seq->candidates, (gpointer)next->keysym)) != NULL) {
		for (l = list; l != NULL; l = g_slist_next(l)) {
			Sequence *s = l->data;

			if (s->keysym == next->keysym &&
			    s->modifiers == next->modifiers &&+
			    s->mod_mask == next->mod_mask) {
				g_set_error(error, sequence_get_error_quark(), SEQ_ERR_SEQUENCE_EXISTS,
					    "Sequence [keysym:0x%lx,mods:0x%x,mask:0x%x] already exists.",
					    next->keysym, next->modifiers, next->mod_mask);
				return FALSE;
			}
		}
	}
	list = g_slist_append(list, next);
	g_tree_insert(seq->candidates, (gpointer)next->keysym, list);

	return TRUE;
}

static gboolean
sequence_terminate(Sequence     *seq,
		   const gchar  *string,
		   gulong        keysym,
		   GError      **error)
{
	if (seq->candidates) {
		g_set_error(error, sequence_get_error_quark(), SEQ_ERR_INVALID_SEQUENCE,
			    "Child sequence was about to be discarded.");

		return FALSE;
	}
	seq->string = g_strdup(string);
	seq->composed = keysym;

	return TRUE;
}

static Sequence *
sequence_lookup(Sequence *seq,
		gulong    keysym,
		guint     modifiers)
{
	GSList *list, *l;

	g_return_val_if_fail (seq != NULL, NULL);
	g_return_val_if_fail (seq->candidates != NULL, NULL);

	if ((list = g_tree_lookup(seq->candidates, (gpointer)keysym)) != NULL) {
		for (l = list; l != NULL; l = g_slist_next(l)) {
			Sequence *s = l->data;

			if (s->keysym == keysym && (modifiers & s->mod_mask) == s->modifiers) {
				return s;
			}
		}
	}

	return NULL;
}

static Sequence *
sequence_match(Sequence *seq,
	       Sequence *pattern)
{
	GSList *list, *l;

	g_return_val_if_fail (seq != NULL, NULL);
	g_return_val_if_fail (pattern != NULL, NULL);

	if (seq->candidates == NULL) {
		return NULL;
	}

	if ((list = g_tree_lookup(seq->candidates, (gpointer)pattern->keysym)) != NULL) {
		for (l = list; l != NULL; l = g_slist_next(l)) {
			Sequence *s = l->data;

			if (s->keysym == pattern->keysym &&
			    s->modifiers == pattern->modifiers &&
			    s->mod_mask == pattern->mod_mask)
				return s;
		}
	}

	return NULL;
}

static gchar *
compose_get_compose_filename(const gchar  *locale,
			     gchar       **registered_locale)
{
	gchar *dirfile = g_build_filename(XLOCALEDIR, "compose.dir", NULL);
	gchar *charset = nl_langinfo(CODESET);
	gchar *default_locale = g_strdup_printf("en_US.%s", charset);
	gchar *retval = NULL;
	gchar buf[1024], *p;
	gchar filename[1024], localename[1024];
	gchar *canonical_locale = NULL;
	gint i;
	FILE *fp;

	if (!g_file_test(dirfile, G_FILE_TEST_EXISTS)) {
		g_printerr("No %s file available. unable to provide a compose feature.\n",
			   dirfile);
		g_free(dirfile);
		return NULL;
	}

	if (locale == NULL) {
		locale = default_locale;
	} else {
		gchar *tmp_locale, *tmp;

		tmp_locale = g_strdup(locale);
		tmp = strchr(tmp_locale, '.');
		if (tmp)
			*tmp = 0;
		tmp = strchr(tmp_locale, '@');
		if (tmp)
			*tmp = 0;
		canonical_locale = g_strdup_printf("%s.%s", tmp_locale, charset);
		g_free(tmp_locale);
	}

	if ((fp = fopen(dirfile, "r")) == NULL) {
		g_printerr("Unable to open %s\n", dirfile);

		return NULL;
	}

	while (!feof(fp)) {
		fgets(buf, 1024, fp);
		p = buf;

		_skip_whitespaces(p);
		if (*p == 0)
			continue;
		if (_is_comment(*p))
			continue;

		/* get a filename */
		for (i = 0; *p && !_is_whitespace(*p); i++, p++) {
			filename[i] = *p;
		}
		filename[i] = 0;

		if (*p == ':')
			p++;
		_skip_whitespaces(p);
		if (*p == 0) {
			/* invalid entry */
			d(g_print("Invalid entry: %s\n", buf));
			continue;
		}

		/* get a locale name */
		for (i = 0; *p && !g_ascii_isspace(*p); i++, p++) {
			localename[i] = *p;
		}
		localename[i] = 0;

		d(g_print("file: [%s], locale: [%s]\n", filename, localename));

		if (strcmp(localename, locale) == 0) {
			retval = g_strdup(filename);
			*registered_locale = g_strdup(localename);
			break;
		} else if (canonical_locale &&
			   strcmp(localename, canonical_locale) == 0) {
			retval = g_strdup(filename);
			*registered_locale = g_strdup(localename);
			break;
		}
	}

	fclose(fp);

	if (retval == NULL && strcmp(locale, default_locale) != 0) {
		retval = compose_get_compose_filename(default_locale, registered_locale);
	}
	g_free(dirfile);
	g_free(default_locale);

	return retval;
}

/*
 * Public functions
 */
Compose *
compose_new(void)
{
	Compose *retval = g_new0(Compose, 1);

	retval->seq_tree = sequence_new(NoSymbol, 0, 0);

	return retval;
}

void
compose_free(Compose *compose)
{
	g_return_if_fail (compose != NULL);

	if (compose->fp)
		compose_close(compose);
	g_free(compose->locale);
	sequence_free(compose->seq_tree);

	g_free(compose);
}

gboolean
compose_open(Compose     *compose,
	     const gchar *locale)
{
	gchar *composefile;
	gchar *fullname = NULL;
	gchar *localename = NULL;
	gboolean retry = FALSE;
	gboolean retval = FALSE;

	g_return_val_if_fail (compose != NULL, FALSE);

	if (locale == NULL)
		retry = TRUE;

  retry:
	composefile = compose_get_compose_filename(locale, &localename);
	if (composefile == NULL) {
		if (!retry) {
			locale = NULL;
			retry = TRUE;
			goto retry;
		}
		goto end;
	}

	fullname = g_build_filename(XLOCALEDIR, composefile, NULL);
	if (!g_file_test(fullname, G_FILE_TEST_EXISTS)) {
		if (!retry) {
			/* retry with the default locale */
			g_free(composefile);
			g_free(fullname);
			locale = NULL;
			retry = TRUE;
			goto retry;
		}
		goto end;
	}

	if ((compose->fp = fopen(fullname, "r")) == NULL) {
		g_warning("Unable to open %s", fullname);
		goto end;
	}
	compose->locale = g_strdup(localename);
	retval = TRUE;
  end:
	g_free(localename);
	g_free(composefile);
	g_free(fullname);

	return retval;
}

void
compose_close(Compose *compose)
{
	g_return_if_fail (compose != NULL);

	fclose(compose->fp);
	compose->fp = NULL;
}

gboolean
compose_parse(Compose *compose)
{
	gchar buf[1024], *tmp, *p, *sep;
	gchar *seq, *data;
	gchar seqbuf[1025], string[1025], symbol[1025];
	gchar *utf8_string;
	gchar *old_locale, *charset;
	gint i;
	guint modifiers, mod_mask;
	GPtrArray *seqarray;
	gulong keysym, result_keysym;
	gboolean pending = FALSE;
	Sequence *node, *sequence;
	GError *error = NULL;

	g_return_val_if_fail (compose != NULL, FALSE);
	g_return_val_if_fail (compose->fp != NULL, FALSE);

	old_locale = setlocale(LC_CTYPE, NULL);
	setlocale(LC_CTYPE, compose->locale);
	charset = g_strdup(nl_langinfo(CODESET));
	setlocale(LC_CTYPE, old_locale);

	d(g_print("compose data charset: %s", charset));

	while (!feof(compose->fp)) {
		fgets(buf, 1024, compose->fp);
		p = buf;

		_skip_whitespaces(p);
		if (*p == 0)
			continue;
		if (_is_comment(*p))
			continue;

		tmp = g_strdup(p);
		sep = strchr(tmp, ':');
		if (sep == NULL) {
			g_warning("Invalid entry: %s", buf);
			g_free(tmp);
			continue;
		}
		*sep = 0;
		seq = tmp;
		data = sep + 1;

		/* get sequences */
		p = seq;
		seqarray = g_ptr_array_new();
		sequence = NULL;
		while (*p) {
			if (*p != '<') {
				g_warning("Invalid sequence [<:%c]: %ld of %s", *p, (long)(p - seq), seq);
				goto fail;
			}
			p++;
			/* FIXME: modifier keys, exclusive sequences support */
			modifiers = 0;
			mod_mask = GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK;

			for (i = 0; *p && !_is_whitespace(*p) && *p != '>' && i < 1024; i++, p++)
				seqbuf[i] = *p;
			seqbuf[i] = 0;
			if (*p != '>') {
				g_warning("Invalid sequence [>:%c]: %ld of %s", *p, (long)(p - seq), seq);
				goto fail;
			}
			p++;

			keysym = XStringToKeysym(seqbuf);
			if (keysym == NoSymbol) {
				/* dirty hack to get rid of known warnings */
				if (strncmp(seqbuf, "combining_", 10) != 0)
					g_warning("Invalid symbol: %s", seqbuf);
				goto fail;
			}
			sequence = sequence_new(keysym, modifiers, mod_mask);
			g_ptr_array_add(seqarray, sequence);

			_skip_whitespaces(p);
		}
		if (seqarray->len == 0) {
			g_warning("No valid sequences found: %s", buf);
			goto fail;
		}

		/* get string */
		_skip_whitespaces(data);
		p = data;

		if (*p == 0 || _is_comment(*p)) {
			g_warning("Invalid entry [no string]: %s", buf);
			goto fail;
		}

		if (*p != '"') {
			g_warning("Invalid data [no initiator for string]: %s", data);
			goto fail;
		}
		p++;
		pending = FALSE;
		for (i = 0; *p && i < 1024; p++) {
			if (*p == '\\') {
				if (!pending) {
					pending = TRUE;
					continue;
				}
			}
			if (pending) {
				/* FIXME: hexadecimal, octadecimal support */
				switch (*p) {
				    case '\\':
				    case '"':
					    string[i++] = *p;
					    break;
				    case 'n':
					    string[i++] = '\n';
					    break;
				    case 'r':
					    string[i++] = '\r';
					    break;
				    case 't':
					    string[i++] = '\t';
					    break;
				    default:
					    g_warning("unknown escape format: \\%c", *p);
					    break;
				}
				pending = FALSE;
				continue;
			}
			if (*p == '"')
				break;
			string[i++] = *p;
		}
		string[i] = 0;
		if (*p != '"') {
			g_warning("Invalid data [no terminator for string]: %s", data);
			goto fail;
		}
		p++;

		/* get symbol */
		_skip_whitespaces(p);
		if (*p == 0 || _is_comment(*p)) {
			g_warning("Invalid entry [no symbol]: %s", buf);
			goto fail;
		}
		for (i = 0; *p && !_is_whitespace(*p) && i < 1024; i++, p++)
			symbol[i] = *p;
		symbol[i] = 0;
		result_keysym = XStringToKeysym(symbol);
		if (result_keysym == NoSymbol) {
			g_warning("Invalid symbol for result: %s", symbol);
		}
		utf8_string = g_convert(string, -1, "UTF-8", charset, NULL, NULL, &error);
		if (error) {
			g_warning("%s: %s", error->message, buf);
			g_error_free(error);
			goto fail;
		}
		sequence_terminate(sequence, utf8_string, result_keysym, &error);
		g_free(utf8_string);
		if (error) {
			/* unlikely to happen usually */
			g_warning("%s", error->message);
			g_error_free(error);
			goto fail;
		}

		node = compose->seq_tree;
		for (i = 0; i < seqarray->len; i++) {
			Sequence *child, *s = g_ptr_array_index(seqarray, i);

			g_ptr_array_index(seqarray, i) = NULL;
			if ((child = sequence_match(node, s)) == NULL) {
				sequence_add(node, s, &error);
				if (error) {
					g_warning("%s: %s", error->message, seq);
					sequence_free(s);
					goto fail;
				}
				node = s;
			} else {
				if ((i + 1) == seqarray->len) {
					if (child->candidates != NULL) {
						g_warning("Duplicate sequence: %s", seq);
						goto fail;
					} else if (s->composed != child->composed ||
						   (s->string == NULL && child->string != NULL) ||
						   (s->string != NULL && child->string == NULL) ||
						   (s->string && child->string && strcmp(s->string, child->string))) {
						g_warning("Duplicate sequence and different result assigned: %s", seq);
						goto fail;
					}
				}
				sequence_free(s);
				node = child;
			}
		}

		g_ptr_array_free(seqarray, TRUE);
		g_free(tmp);
		continue;
	  fail:
		if (seqarray) {
			for (i = 0; i < seqarray->len; i++) {
				Sequence *s = g_ptr_array_index(seqarray, i);

				if (s)
					sequence_free(s);
			}
			g_ptr_array_free(seqarray, TRUE);
		}
		g_free(tmp);
	}

	return TRUE;
}

gboolean
compose_lookup(Compose   *composer,
	       Sequence **sequence,
	       gulong     keysym,
	       guint      modifiers,
	       gchar    **result_string,
	       gulong    *result_keysym)
{
	Sequence *seq;

	g_return_val_if_fail (composer != NULL, FALSE);
	g_return_val_if_fail (sequence != NULL, FALSE);
	g_return_val_if_fail (result_string != NULL, FALSE);
	g_return_val_if_fail (result_keysym != NULL, FALSE);

	if (*sequence == NULL)
		*sequence = composer->seq_tree;

	seq = sequence_lookup(*sequence, keysym, modifiers);
	if (seq == NULL)
		return FALSE;

	*result_string = g_strdup(seq->string);
	*result_keysym = seq->composed;
	*sequence = seq;

	return TRUE;
}
