#include "zstream.h"
#include "blob.h"
#include "delta.h"
#include "git2/sys/diff.h"
	else
		pi->flags = 0;
	if (diff && diff->opts.id_abbrev != 0)
		pi->oid_strlen = diff->opts.id_abbrev;
	git_buf_puts(out, delta->new_file.path);
	git_buf_putc(out, '\n');
	if (git_buf_oom(out))
	return pi->print_cb(delta, NULL, &pi->line, pi->payload);
	return pi->print_cb(delta, NULL, &pi->line, pi->payload);
	git_oid_tostr(start_oid, pi->oid_strlen, &delta->old_file.id);
	git_oid_tostr(end_oid, pi->oid_strlen, &delta->new_file.id);
		out, (pi->oid_strlen <= GIT_OID_HEXSZ) ?
			":%06o %06o %s... %s... %c" : ":%06o %06o %s %s %c",
	return pi->print_cb(delta, NULL, &pi->line, pi->payload);
	git_oid_tostr(start_oid, oid_strlen, &delta->old_file.id);
	git_oid_tostr(end_oid, oid_strlen, &delta->new_file.id);
	return git_buf_oom(out) ? -1 : 0;
	if (git_oid_iszero(&delta->old_file.id)) {
	if (git_oid_iszero(&delta->new_file.id)) {
	GITERR_CHECK_ERROR(diff_print_oid_range(out, delta, oid_strlen));
static int print_binary_hunk(diff_print_info *pi, git_blob *old, git_blob *new)
{
	git_buf deflate = GIT_BUF_INIT, delta = GIT_BUF_INIT, *out = NULL;
	const void *old_data, *new_data;
	git_off_t old_data_len, new_data_len;
	unsigned long delta_data_len, inflated_len;
	const char *out_type = "literal";
	char *scan, *end;
	int error;

	old_data = old ? git_blob_rawcontent(old) : NULL;
	new_data = new ? git_blob_rawcontent(new) : NULL;

	old_data_len = old ? git_blob_rawsize(old) : 0;
	new_data_len = new ? git_blob_rawsize(new) : 0;

	/* The git_delta function accepts unsigned long only */
	if (!git__is_ulong(old_data_len) || !git__is_ulong(new_data_len))
		return GIT_EBUFS;

	out = &deflate;
	inflated_len = (unsigned long)new_data_len;

	if ((error = git_zstream_deflatebuf(
			out, new_data, (size_t)new_data_len)) < 0)
		goto done;

	/* The git_delta function accepts unsigned long only */
	if (!git__is_ulong((git_off_t)deflate.size)) {
		error = GIT_EBUFS;
		goto done;
	}

	if (old && new) {
		void *delta_data = git_delta(
			old_data, (unsigned long)old_data_len,
			new_data, (unsigned long)new_data_len,
			&delta_data_len, (unsigned long)deflate.size);

		if (delta_data) {
			error = git_zstream_deflatebuf(
				&delta, delta_data, (size_t)delta_data_len);

			git__free(delta_data);

			if (error < 0)
				goto done;

			if (delta.size < deflate.size) {
				out = &delta;
				out_type = "delta";
				inflated_len = delta_data_len;
			}
		}
	}

	git_buf_printf(pi->buf, "%s %lu\n", out_type, inflated_len);
	pi->line.num_lines++;

	for (scan = out->ptr, end = out->ptr + out->size; scan < end; ) {
		size_t chunk_len = end - scan;
		if (chunk_len > 52)
			chunk_len = 52;

		if (chunk_len <= 26)
			git_buf_putc(pi->buf, (char)chunk_len + 'A' - 1);
		else
			git_buf_putc(pi->buf, (char)chunk_len - 26 + 'a' - 1);

		git_buf_put_base85(pi->buf, scan, chunk_len);
		git_buf_putc(pi->buf, '\n');

		if (git_buf_oom(pi->buf)) {
			error = -1;
			goto done;
		}

		scan += chunk_len;
		pi->line.num_lines++;
	}

done:
	git_buf_free(&deflate);
	git_buf_free(&delta);

	return error;
}

/* git diff --binary 8d7523f~2 8d7523f~1 */
static int diff_print_patch_file_binary(
	diff_print_info *pi, const git_diff_delta *delta,
	const char *oldpfx, const char *newpfx)
{
	git_blob *old = NULL, *new = NULL;
	const git_oid *old_id, *new_id;
	int error;
	size_t pre_binary_size;

	if ((pi->flags & GIT_DIFF_SHOW_BINARY) == 0)
		goto noshow;

	pre_binary_size = pi->buf->size;
	git_buf_printf(pi->buf, "GIT binary patch\n");
	pi->line.num_lines++;

	old_id = (delta->status != GIT_DELTA_ADDED) ? &delta->old_file.id : NULL;
	new_id = (delta->status != GIT_DELTA_DELETED) ? &delta->new_file.id : NULL;

	if (old_id && (error = git_blob_lookup(&old, pi->diff->repo, old_id)) < 0)
		goto done;
	if (new_id && (error = git_blob_lookup(&new, pi->diff->repo,new_id)) < 0)
		goto done;

	if ((error = print_binary_hunk(pi, old, new)) < 0 ||
		(error = git_buf_putc(pi->buf, '\n')) < 0 ||
		(error = print_binary_hunk(pi, new, old)) < 0)
	{
		if (error == GIT_EBUFS) {
			giterr_clear();
			git_buf_truncate(pi->buf, pre_binary_size);
			goto noshow;
		}
	}

	pi->line.num_lines++;

done:
	git_blob_free(old);
	git_blob_free(new);

	return error;

noshow:
	pi->line.num_lines = 1;
	return diff_delta_format_with_paths(
		pi->buf, delta, oldpfx, newpfx,
		"Binary files %s%s and %s%s differ\n");
}

	int error;
	bool binary = !!(delta->flags & GIT_DIFF_FLAG_BINARY);
	bool show_binary = !!(pi->flags & GIT_DIFF_SHOW_BINARY);
	int oid_strlen = binary && show_binary ?
		GIT_OID_HEXSZ + 1 : pi->oid_strlen;

	if ((error = git_diff_delta__format_file_header(
			pi->buf, delta, oldpfx, newpfx, oid_strlen)) < 0)
		return error;
	if ((error = pi->print_cb(delta, NULL, &pi->line, pi->payload)) != 0)
		return error;
	if (!binary)
	if ((error = diff_print_patch_file_binary(pi, delta, oldpfx, newpfx)) < 0)
		return error;
	return pi->print_cb(delta, NULL, &pi->line, pi->payload);
	return pi->print_cb(d, h, &pi->line, pi->payload);
	return pi->print_cb(delta, hunk, line, pi->payload);
	{
		if (error) /* make sure error message is set */
			giterr_set_after_callback_function(error, "git_diff_print");
	}

	{
		if (error) /* make sure error message is set */
			giterr_set_after_callback_function(error, "git_patch_print");
	}

int git_diff_print_callback__to_buf(
	if (!output) {
		giterr_set(GITERR_INVALID, "Buffer pointer must be provided");
		return -1;
	}

int git_diff_print_callback__to_file_handle(
	const git_diff_delta *delta,
	const git_diff_hunk *hunk,
	const git_diff_line *line,
	void *payload)
	FILE *fp = payload ? payload : stdout;
	GIT_UNUSED(delta); GIT_UNUSED(hunk);
	if (line->origin == GIT_DIFF_LINE_CONTEXT ||
		line->origin == GIT_DIFF_LINE_ADDITION ||
		line->origin == GIT_DIFF_LINE_DELETION)
		fputc(line->origin, fp);
	fwrite(line->content, 1, line->content_len, fp);
	return 0;
}
/* print a git_patch to a git_buf */
int git_patch_to_buf(git_buf *out, git_patch *patch)
{
	assert(out && patch);
	git_buf_sanitize(out);
	return git_patch_print(patch, git_diff_print_callback__to_buf, out);