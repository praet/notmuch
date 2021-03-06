/* notmuch - Not much of an email program, (just index and search)
 *
 * Copyright © 2009 Carl Worth
 * Copyright © 2009 Keith Packard
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Authors: Carl Worth <cworth@cworth.org>
 *          Keith Packard <keithp@keithp.com>
 *          Austin Clements <aclements@csail.mit.edu>
 */

#include "notmuch-client.h"

/* Context that gets inherited from the root node. */
typedef struct mime_node_context {
    /* Per-message resources.  These are allocated internally and must
     * be destroyed. */
    FILE *file;
    GMimeStream *stream;
    GMimeParser *parser;
    GMimeMessage *mime_message;

    /* Context provided by the caller. */
#ifdef GMIME_ATLEAST_26
    GMimeCryptoContext *cryptoctx;
#else
    GMimeCipherContext *cryptoctx;
#endif
    notmuch_bool_t decrypt;
} mime_node_context_t;

static int
_mime_node_context_free (mime_node_context_t *res)
{
    if (res->mime_message)
	g_object_unref (res->mime_message);

    if (res->parser)
	g_object_unref (res->parser);

    if (res->stream)
	g_object_unref (res->stream);

    if (res->file)
	fclose (res->file);

    return 0;
}

notmuch_status_t
mime_node_open (const void *ctx, notmuch_message_t *message,
#ifdef GMIME_ATLEAST_26
		GMimeCryptoContext *cryptoctx,
#else
		GMimeCipherContext *cryptoctx,
#endif
		notmuch_bool_t decrypt, mime_node_t **root_out)
{
    const char *filename = notmuch_message_get_filename (message);
    mime_node_context_t *mctx;
    mime_node_t *root;
    notmuch_status_t status;

    root = talloc_zero (ctx, mime_node_t);
    if (root == NULL) {
	fprintf (stderr, "Out of memory.\n");
	status = NOTMUCH_STATUS_OUT_OF_MEMORY;
	goto DONE;
    }

    /* Create the tree-wide context */
    mctx = talloc_zero (root, mime_node_context_t);
    if (mctx == NULL) {
	fprintf (stderr, "Out of memory.\n");
	status = NOTMUCH_STATUS_OUT_OF_MEMORY;
	goto DONE;
    }
    talloc_set_destructor (mctx, _mime_node_context_free);

    mctx->file = fopen (filename, "r");
    if (! mctx->file) {
	fprintf (stderr, "Error opening %s: %s\n", filename, strerror (errno));
	status = NOTMUCH_STATUS_FILE_ERROR;
	goto DONE;
    }

    mctx->stream = g_mime_stream_file_new (mctx->file);
    g_mime_stream_file_set_owner (GMIME_STREAM_FILE (mctx->stream), FALSE);

    mctx->parser = g_mime_parser_new_with_stream (mctx->stream);

    mctx->mime_message = g_mime_parser_construct_message (mctx->parser);

    mctx->cryptoctx = cryptoctx;
    mctx->decrypt = decrypt;

    /* Create the root node */
    root->part = GMIME_OBJECT (mctx->mime_message);
    root->envelope_file = message;
    root->nchildren = 1;
    root->ctx = mctx;

    root->parent = NULL;
    root->part_num = 0;
    root->next_child = 0;
    root->next_part_num = 1;

    *root_out = root;
    return NOTMUCH_STATUS_SUCCESS;

DONE:
    talloc_free (root);
    return status;
}

#ifdef GMIME_ATLEAST_26
static int
_signature_list_free (GMimeSignatureList **proxy)
{
    g_object_unref (*proxy);
    return 0;
}
#else
static int
_signature_validity_free (GMimeSignatureValidity **proxy)
{
    g_mime_signature_validity_free (*proxy);
    return 0;
}
#endif

static mime_node_t *
_mime_node_create (mime_node_t *parent, GMimeObject *part)
{
    mime_node_t *node = talloc_zero (parent, mime_node_t);
    GError *err = NULL;

    /* Set basic node properties */
    node->part = part;
    node->ctx = parent->ctx;
    if (!talloc_reference (node, node->ctx)) {
	fprintf (stderr, "Out of memory.\n");
	talloc_free (node);
	return NULL;
    }
    node->parent = parent;
    node->part_num = node->next_part_num = -1;
    node->next_child = 0;

    /* Deal with the different types of parts */
    if (GMIME_IS_PART (part)) {
	node->nchildren = 0;
    } else if (GMIME_IS_MULTIPART (part)) {
	node->nchildren = g_mime_multipart_get_count (GMIME_MULTIPART (part));
    } else if (GMIME_IS_MESSAGE_PART (part)) {
	/* Promote part to an envelope and open it */
	GMimeMessagePart *message_part = GMIME_MESSAGE_PART (part);
	GMimeMessage *message = g_mime_message_part_get_message (message_part);
	node->envelope_part = message_part;
	node->part = GMIME_OBJECT (message);
	node->nchildren = 1;
    } else {
	fprintf (stderr, "Warning: Unknown mime part type: %s.\n",
		 g_type_name (G_OBJECT_TYPE (part)));
	talloc_free (node);
	return NULL;
    }

    /* Handle PGP/MIME parts */
    if (GMIME_IS_MULTIPART_ENCRYPTED (part)
	&& node->ctx->cryptoctx && node->ctx->decrypt) {
	if (node->nchildren != 2) {
	    /* this violates RFC 3156 section 4, so we won't bother with it. */
	    fprintf (stderr, "Error: %d part(s) for a multipart/encrypted "
		     "message (must be exactly 2)\n",
		     node->nchildren);
	} else {
	    GMimeMultipartEncrypted *encrypteddata =
		GMIME_MULTIPART_ENCRYPTED (part);
	    node->decrypt_attempted = TRUE;
#ifdef GMIME_ATLEAST_26
	    GMimeDecryptResult *decrypt_result = NULL;
	    node->decrypted_child = g_mime_multipart_encrypted_decrypt
		(encrypteddata, node->ctx->cryptoctx, &decrypt_result, &err);
#else
	    node->decrypted_child = g_mime_multipart_encrypted_decrypt
		(encrypteddata, node->ctx->cryptoctx, &err);
#endif
	    if (node->decrypted_child) {
		node->decrypt_success = node->verify_attempted = TRUE;
#ifdef GMIME_ATLEAST_26
		/* This may be NULL if the part is not signed. */
		node->sig_list = g_mime_decrypt_result_get_signatures (decrypt_result);
		if (node->sig_list)
		    g_object_ref (node->sig_list);
		g_object_unref (decrypt_result);
#else
		node->sig_validity = g_mime_multipart_encrypted_get_signature_validity (encrypteddata);
#endif
	    } else {
		fprintf (stderr, "Failed to decrypt part: %s\n",
			 (err ? err->message : "no error explanation given"));
	    }
	}
    } else if (GMIME_IS_MULTIPART_SIGNED (part) && node->ctx->cryptoctx) {
	if (node->nchildren != 2) {
	    /* this violates RFC 3156 section 5, so we won't bother with it. */
	    fprintf (stderr, "Error: %d part(s) for a multipart/signed message "
		     "(must be exactly 2)\n",
		     node->nchildren);
	} else {
#ifdef GMIME_ATLEAST_26
	    node->sig_list = g_mime_multipart_signed_verify
		(GMIME_MULTIPART_SIGNED (part), node->ctx->cryptoctx, &err);
	    node->verify_attempted = TRUE;

	    if (!node->sig_list)
		fprintf (stderr, "Failed to verify signed part: %s\n",
			 (err ? err->message : "no error explanation given"));
#else
	    /* For some reason the GMimeSignatureValidity returned
	     * here is not a const (inconsistent with that
	     * returned by
	     * g_mime_multipart_encrypted_get_signature_validity,
	     * and therefore needs to be properly disposed of.
	     *
	     * In GMime 2.6, they're both non-const, so we'll be able
	     * to clean up this asymmetry. */
	    GMimeSignatureValidity *sig_validity = g_mime_multipart_signed_verify
		(GMIME_MULTIPART_SIGNED (part), node->ctx->cryptoctx, &err);
	    node->verify_attempted = TRUE;
	    node->sig_validity = sig_validity;
	    if (sig_validity) {
		GMimeSignatureValidity **proxy =
		    talloc (node, GMimeSignatureValidity *);
		*proxy = sig_validity;
		talloc_set_destructor (proxy, _signature_validity_free);
	    }
#endif
	}
    }

#ifdef GMIME_ATLEAST_26
    /* sig_list may be created in both above cases, so we need to
     * cleanly handle it here. */
    if (node->sig_list) {
	GMimeSignatureList **proxy = talloc (node, GMimeSignatureList *);
	*proxy = node->sig_list;
	talloc_set_destructor (proxy, _signature_list_free);
    }
#endif

#ifndef GMIME_ATLEAST_26
    if (node->verify_attempted && !node->sig_validity)
	fprintf (stderr, "Failed to verify signed part: %s\n",
		 (err ? err->message : "no error explanation given"));
#endif

    if (err)
	g_error_free (err);

    return node;
}

mime_node_t *
mime_node_child (mime_node_t *parent, int child)
{
    GMimeObject *sub;
    mime_node_t *node;

    if (!parent || child < 0 || child >= parent->nchildren)
	return NULL;

    if (GMIME_IS_MULTIPART (parent->part)) {
	if (child == 1 && parent->decrypted_child)
	    sub = parent->decrypted_child;
	else
	    sub = g_mime_multipart_get_part
		(GMIME_MULTIPART (parent->part), child);
    } else if (GMIME_IS_MESSAGE (parent->part)) {
	sub = g_mime_message_get_mime_part (GMIME_MESSAGE (parent->part));
    } else {
	/* This should have been caught by message_part_create */
	INTERNAL_ERROR ("Unexpected GMimeObject type: %s",
			g_type_name (G_OBJECT_TYPE (parent->part)));
    }
    node = _mime_node_create (parent, sub);

    if (child == parent->next_child && parent->next_part_num != -1) {
	/* We're traversing in depth-first order.  Record the child's
	 * depth-first numbering. */
	node->part_num = parent->next_part_num;
	node->next_part_num = node->part_num + 1;

	/* Prepare the parent for its next depth-first child. */
	parent->next_child++;
	parent->next_part_num = -1;

	if (node->nchildren == 0) {
	    /* We've reached a leaf, so find the parent that has more
	     * children and set it up to number its next child. */
	    mime_node_t *iter = node->parent;
	    while (iter && iter->next_child == iter->nchildren)
		iter = iter->parent;
	    if (iter)
		iter->next_part_num = node->part_num + 1;
	}
    }

    return node;
}

static mime_node_t *
_mime_node_seek_dfs_walk (mime_node_t *node, int *n)
{
    mime_node_t *ret = NULL;
    int i;

    if (*n == 0)
	return node;

    *n -= 1;
    for (i = 0; i < node->nchildren && !ret; i++) {
	mime_node_t *child = mime_node_child (node, i);
	ret = _mime_node_seek_dfs_walk (child, n);
	if (!ret)
	    talloc_free (child);
    }
    return ret;
}

mime_node_t *
mime_node_seek_dfs (mime_node_t *node, int n)
{
    if (n < 0)
	return NULL;
    return _mime_node_seek_dfs_walk (node, &n);
}
