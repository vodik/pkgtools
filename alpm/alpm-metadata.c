#include "alpm-metadata.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <alpm.h>
#include <archive.h>
#include <archive_entry.h>

/* {{{ MAGIC */
struct archive_read_buffer {
    char *line;
    char *line_offset;
    size_t line_size;
    size_t real_line_size;

    char *block;
    char *block_offset;
    size_t block_size;

    long ret;
};

static int archive_fgets(struct archive *a, struct archive_read_buffer *b, size_t entry_size)
{
    /* ensure we start populating our line buffer at the beginning */
    b->line_offset = b->line;

    while(1) {
        size_t new, block_remaining;
        char *eol;

        /* have we processed this entire block? */
        if(b->block + b->block_size == b->block_offset) {
            int64_t offset;
            if(b->ret == ARCHIVE_EOF) {
                /* reached end of archive on the last read, now we are out of data */
                return b->ret;
            }

            /* zero-copy - this is the entire next block of data. */
            b->ret = archive_read_data_block(a, (const void **)&b->block,
                                             &b->block_size, &offset);
            b->block_offset = b->block;
            block_remaining = b->block_size;

            /* error, cleanup */
            if(b->ret != ARCHIVE_OK) {
                return b->ret;
            }
        } else {
            block_remaining = b->block + b->block_size - b->block_offset;
        }

        /* look through the block looking for EOL characters */
        eol = memchr(b->block_offset, '\n', block_remaining);
        if(!eol) {
            eol = memchr(b->block_offset, '\0', block_remaining);
        }

        /* note: we know eol > b->block_offset and b->line_offset > b->line,
         * so we know the result is unsigned and can fit in size_t */
        new = eol ? (size_t)(eol - b->block_offset) : block_remaining;
        if((b->line_offset - b->line + new + 1) > entry_size) {
            return -ERANGE;
        }

        if(eol) {
            size_t len = (size_t)(eol - b->block_offset);
            memcpy(b->line_offset, b->block_offset, len);
            b->line_offset[len] = '\0';
            b->block_offset = eol + 1;
            b->real_line_size = b->line_offset + len - b->line;
            /* this is the main return point; from here you can read b->line */
            return ARCHIVE_OK;
        } else {
            /* we've looked through the whole block but no newline, copy it */
            size_t len = (size_t)(b->block + b->block_size - b->block_offset);
            b->line_offset = mempcpy(b->line_offset, b->block_offset, len);
            b->block_offset = b->block + b->block_size;
            /* there was no new data, return what is left; saved ARCHIVE_EOF will be
             * returned on next call */
            if(len == 0) {
                b->line_offset[0] = '\0';
                b->real_line_size = b->line_offset - b->line;
                return ARCHIVE_OK;
            }
        }
    }

    return b->ret;
} /* }}} */

static void read_metadata_line(char *buf, alpm_pkg_meta_t *pkg)
{
    char *var;

    if (strchr(buf, '=') == NULL)
        return;

    var = strsep(&buf, " = ");
    buf += 2;

    if (strcmp(var, "pkgname") == 0)
        pkg->name = strdup(buf);
    else if (strcmp(var, "pkgbase") == 0)
        pkg->base = strdup(buf);
    else if (strcmp(var, "pkgver") == 0)
        pkg->version = strdup(buf);
    else if (strcmp(var, "pkgdesc") == 0)
        pkg->desc = strdup(buf);
    else if (strcmp(var, "url") == 0)
        pkg->url = strdup(buf);
    else if (strcmp(var, "builddate") == 0)
        pkg->builddate = atol(buf);
    else if (strcmp(var, "packager") == 0)
        pkg->packager = strdup(buf);
    else if (strcmp(var, "size") == 0)
        pkg->isize = atol(buf);
    else if (strcmp(var, "arch") == 0)
        pkg->arch = strdup(buf);
    else if (strcmp(var, "group") == 0)
        pkg->groups = alpm_list_add(pkg->groups, strdup(buf));
    else if (strcmp(var, "license") == 0)
        pkg->license = alpm_list_add(pkg->license, strdup(buf));
    else if (strcmp(var, "replaces") == 0)
        pkg->replaces = alpm_list_add(pkg->replaces, strdup(buf));
    else if (strcmp(var, "depend") == 0)
        pkg->depends = alpm_list_add(pkg->depends, strdup(buf));
    else if (strcmp(var, "conflict") == 0)
        pkg->conflicts = alpm_list_add(pkg->conflicts, strdup(buf));
    else if (strcmp(var, "provides") == 0)
        pkg->provides = alpm_list_add(pkg->provides, strdup(buf));
    else if (strcmp(var, "optdepend") == 0)
        pkg->optdepends = alpm_list_add(pkg->optdepends, strdup(buf));
    else if (strcmp(var, "makedepend") == 0)
        pkg->makedepends = alpm_list_add(pkg->makedepends, strdup(buf));
    else if (strcmp(var, "checkdepend") == 0)
        pkg->checkdepends = alpm_list_add(pkg->checkdepends, strdup(buf));
}

static void read_metadata(struct archive *a, struct archive_entry *ae, alpm_pkg_meta_t *pkg)
{
    off_t entry_size = archive_entry_size(ae);

    struct archive_read_buffer buf = {
        .line = malloc(entry_size)
    };

    while(archive_fgets(a, &buf, entry_size) == ARCHIVE_OK && buf.real_line_size > 0) {
        read_metadata_line(buf.line, pkg);
    }

    free(buf.line);
}

int alpm_pkg_load_metadata(const char *filename, alpm_pkg_meta_t **_pkg)
{
    struct archive *archive = archive_read_new();
    alpm_pkg_meta_t *pkg;
    struct stat st;
    char *memblock = MAP_FAILED;
    int fd = 0, rc = 0;

    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        if(errno != ENOENT)
            err(EXIT_FAILURE, "failed to open %s", filename);
        goto cleanup;
    }

    fstat(fd, &st);
    memblock = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (memblock == MAP_FAILED)
        err(EXIT_FAILURE, "failed to mmap package %s", filename);

    archive_read_support_filter_all(archive);
    archive_read_support_format_all(archive);

    int r = archive_read_open_memory(archive, memblock, st.st_size);
    if (r != ARCHIVE_OK) {
        warnx("%s is not an archive", filename);
        rc = -1;
        goto cleanup;
    }

    pkg = calloc(1, sizeof(alpm_pkg_meta_t));
    for (;;) {
        struct archive_entry *entry;

        r = archive_read_next_header(archive, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        } else if (r != ARCHIVE_OK) {
            errx(EXIT_FAILURE, "failed to read header: %s", archive_error_string(archive));
        }

        const mode_t mode = archive_entry_mode(entry);
        const char *path  = archive_entry_pathname(entry);

        if (S_ISREG(mode) && strcmp(path, ".PKGINFO") == 0) {
            read_metadata(archive, entry, pkg);
            break;
        }
    }

    pkg->filename  = strdup(filename);
    pkg->size      = st.st_size;

    *_pkg = pkg;

cleanup:
    if (fd)
        close(fd);

    if (memblock != MAP_FAILED)
        munmap(memblock, st.st_size);

    archive_read_close(archive);
    archive_read_free(archive);
    return rc;
}
