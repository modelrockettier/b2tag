/*
 * Copyright (C) 2018 Tim Schlueter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the author of this program
 * gives permission to link the code portions of this program with the
 * OpenSSL library under certain conditions as described in each file,
 * and distribute linked combinations including the two.
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL.  If you modify this file(s)
 * with this exception, you may extend this exception to your version
 * of the file(s), but you are not obligated to do so.  If you do not
 * wish to do so, delete this exception statement from your version.
 * If you delete this exception statement from all source files in the
 * program, then also delete it here.
 */

/** @file
 * Checks files against their stored hashes and print the statuses.
 */

#include "file.h"

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utilities.h"
#include "xa.h"

/** Call the kernel's fadvise() on files larger than this. */
#define FADVISE_THRESHOLD 65536

/**
 * An array holding the inode and dev numbers for a directory.
 */
struct dir_no {
	dev_t device; /**< The directory's device ID. */
	ino_t inode;  /**< The directory's inode number. */
};

/**
 * An array holding the device id and inodes of all parent directories.
 * This is used to check for filesystem loops.
 */
struct parent_dirs {
	struct dir_no *data; /**< An array of directories. */
	size_t count;        /**< The current number of elements used in the array. */
	size_t allocated;    /**< The number of elements allocated in the array. */
};

/** A file's current state (e.g outdated). */
enum file_state {
	FILE_FAULT,     /**< An error occurred reading file. */
	FILE_OK,        /**< File hash and mtime matches. */
	FILE_SAME,      /**< File hash matches, mtime doesn't. */
	FILE_NEW,       /**< File doesn't have stored hash or mtime. */
	FILE_OUTDATED,  /**< File hash differs, mtime is newer. */
	FILE_BACKDATED, /**< File hash differs, mtime is older. */
	FILE_CORRUPT,   /**< File hash differs, mtime matches. */
	FILE_INVALID,   /**< Xattrs corrupted. */
};

/** The string representation of the ::file_state enum values. */
static char const * const file_state_str[] = {
	"FAULT",
	"OK",
	"HASH OK",
	"NEW",
	"OUTDATED",
	"BACKDATED",
	"CORRUPT",
	"INVALID",
};


/* Forward declarations. */
static int process_path2(const char *filename, struct parent_dirs *parents);

/**
 * Prints information about a file's state.
 *
 * @param state     The file's state (e.g. ok).
 * @param filename  The name of the file.
 * @param stored    The file's stored attributes (may be NULL).
 * @param actual    The file's actual attributes (may be NULL).
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static void print_state(enum file_state state, const char *filename, xa_t *stored, xa_t *actual)
{
	bool print_status;

	switch (state) {
	case FILE_BACKDATED:
	case FILE_CORRUPT:
	case FILE_FAULT:
	case FILE_INVALID:
		print_status = check_crit();
		break;

	default:
		print_status = check_err();
		break;
	}

	if (!print_status)
		return;

	printf("%s: %s\n", filename, file_state_str[state]);

	if (check_debug()) {
		if (stored != NULL && stored->valid)
			printf("# stored: %s\n", xa_format(stored));
		if (actual != NULL && actual->valid)
			printf("# actual: %s\n", xa_format(actual));
	}
}

/**
 * Prints information about a file in the coreutils sha*sum format.
 *
 * @param state     The file's state (e.g. ok).
 * @param filename  The name of the file.
 * @param stored    The file's stored attributes (may be NULL).
 * @param actual    The file's actual attributes (may be NULL).
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static void print_sum(enum file_state state __attribute__((unused)),
	const char *filename, xa_t *stored, xa_t *actual)
{
	assert(filename != NULL);

	if (actual != NULL && actual->valid)
		printf("%s  %s\n", actual->hash, filename);
	else if (stored != NULL && stored->valid)
		printf("%s  %s\n", stored->hash, filename);
	else
		pr_err("Error no hash found for \"%s\"\n", filename);
}

/**
 * Checks if a file's stored hash and timestamp match the current values.
 *
 * @note @p stored and @p actual may be filled with data depending on the
 *       file's state (but may not be so check the xa_t::valid field).
 *       Also, @p actual->mtime will not be changed if non-zero.
 *
 * @param[in]     fd      The file to get the state of.
 * @param[out]    stored  The xa structure to hold the file's stored attributes.
 * @param[in,out] actual  The xa structure to hold the file's current hash+mtime.
 *
 * @returns Returns the file's state.
 *
 * @see file_state
 */
static enum file_state get_file_state(int fd, xa_t *stored, xa_t *actual)
{
	int err;
	int comparison;

	assert(fd >= 0);
	assert(stored != NULL);
	assert(actual != NULL);
	assert(stored->alg == actual->alg);

	/* Skip the fstat call if mtime seconds is already set. */
	if (actual->mtime.tv_sec == 0) {
		struct stat st;

		err = fstat(fd, &st);
		if (err != 0)
			return FILE_FAULT;

		actual->mtime = st.st_mtim;
	}

	err = xa_read(fd, stored);
	if (err < 0)
		return FILE_FAULT;

	if (err == 1) {
		xa_compute(fd, actual);
		return FILE_NEW;
	}

	if (err >= 2) {
		xa_compute(fd, actual);
		return FILE_INVALID;
	}

	comparison = ts_compare(stored->mtime, actual->mtime, stored->fuzzy);

	/* Quick check. If stored timestamps match, skip hashing. */
	if (comparison == 0 && !args.check)
		return FILE_OK;

	xa_compute(fd, actual);

	/* hash and mtime matches -> ok, hash matches and mtime differs -> same */
	if (strcmp(stored->hash, actual->hash) == 0)
		return (comparison == 0) ? FILE_OK : FILE_SAME;

	/* file mtime is newer than the xattr mtime. */
	if (comparison < 0)
		return FILE_OUTDATED;

	/* file mtime is older than the xattr mtime. */
	if (comparison > 0)
		return FILE_BACKDATED;

	/* Same timestamp, different hashes. */
	return FILE_CORRUPT;
}

/**
 * Checks if a file's stored hash and timestamp match the current values.
 *
 * @param fd        A readable open file descriptor to the file to check.
 * @param filename  The file to check.
 * @param st        The stat() structure of the file to check.
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static int check_file(int fd, const char *filename, struct stat *st)
{
	enum file_state state;
	int err = 0;
	xa_t a;
	xa_t s;

	assert(fd >= 0);
	assert(filename != NULL);
	assert(st != NULL);

	assert(S_ISREG(st->st_mode));

	pr_debug("Processing file: %s\n", filename);

	/* If the file is large (enough), tell the kernel we'll be accessing it
	 * sequentially.
	 */
	if (st->st_size > FADVISE_THRESHOLD) {
		err = posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
		/* Ignore if fadvise fails for some reason (just print a warning). */
		if (err != 0)
			pr_warn("Warning: fadvise failed: %m\n");
	}

	a = s = (xa_t){ .alg = args.alg };

	a.mtime = st->st_mtim;

	state = get_file_state(fd, &s, &a);
	if (state == FILE_FAULT)
		return -1;

	/* Whether to print the file status or the sha*sum data. */
	if (args.print)
		print_sum(state, filename, &s, &a);
	else
		print_state(state, filename, &s, &a);

	if (state == FILE_OK)
		return 0;

	switch (state) {
	case FILE_BACKDATED:
	case FILE_CORRUPT:
	case FILE_FAULT:
	case FILE_INVALID:
		err = 1;

		/* Don't update the stored xattrs unless -f is specified for backdated,
		 * corrupt, fault, or invalid files.
		 */
		if (!args.force)
			return 1;

		break;

	default:
		break;
	}

	if (args.dry_run)
		return err;

	err = xa_write(fd, &a);
	if (err != 0) {
		pr_err("Error: could not write extended attributes to file \"%s\": %m\n", filename);
		return 2;
	}

	return 0;
}

/**
 * Figure out whether a file path is a file or directory and process it.
 *
 * If @p filename is a regular file, this will pass it to check_file().
 *
 * If @p filename is a directory and --recursive was set on the command-line,
 * this will pass it on to check_dir().
 *
 * @param fd        A readable open file descriptor to the directory to check.
 * @param filename  The path of the directory to check.
 * @param st        The stat() structure of the directory to check.
 * @param parents   The parent directories' inodes (to check for loops).
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static int check_dir(int fd, const char *filename, struct stat *st, struct parent_dirs *parents)
{
	char *buffer;
	int ret = 0;
	int err;
	size_t i;
	struct dirent *entry;
	DIR *dirp;

	assert(filename != NULL);
	assert(parents != NULL);

	pr_debug("Processing dir: %s\n", filename);

	/* Check for filesystem loop. */
	for (i = 0; i < parents->count; i++) {
		if (parents->data[i].inode != st->st_ino)
			continue;
		if (parents->data[i].device != st->st_dev)
			continue;

		pr_err("File system loop detected at \"%s\"\n", filename);
		close(fd);
		return 1;
	}

	/* Allocate more space in the parents struct if necessary. */
	if (parents->count >= parents->allocated) {
		void *tmp;

		parents->allocated += 16;

		tmp = realloc(parents->data, parents->allocated * sizeof(parents->data[0]));
		if (tmp == NULL) {
			parents->allocated -= 16;
			close(fd);
			return -1;
		}

		parents->data = tmp;
	}

	dirp = fdopendir(fd);
	if (dirp == NULL) {
		pr_err("Failed to open directory \"%s\": %m\n", filename);
		close(fd);
		return 1;
	}

	fd = -1;

	/* Add the current dir to the parents struct. */
	parents->data[parents->count].device = st->st_dev;
	parents->data[parents->count].inode  = st->st_ino;
	parents->count++;

	while ((entry = readdir(dirp)) != NULL) {
		/* Ignore "." and ".." entries. */
		if (entry->d_name[0] == '.') {
			if (entry->d_name[1] == '\0')
				continue;
			if (entry->d_name[1] == '.' && entry->d_name[2] == '\0')
				continue;
		}

		err = asprintf(&buffer, "%s/%s", filename, entry->d_name);
		if (err < 0) {
			pr_err("Error formatting directory entry \"%s\"/\"%s\": %m\n",
				filename, entry->d_name);
			ret = -1;
			break;
		}

		err = process_path2(buffer, parents);
		free(buffer);
		if (err != 0) {
			ret = err;
			if (err < 0)
				break;
		}

	}

	parents->count--;
	parents->data[parents->count].inode = 0;
	closedir(dirp);

	return ret;
}

/**
 * Figure out whether a file path is a file or directory and process it.
 *
 * If @p filename is a regular file, this will pass it to check_file().
 *
 * If @p filename is a directory and --recursive was set on the command-line,
 * this will pass it on to check_dir().
 *
 * If @p filename is a symbolic link and --no-dereference was set on the
 * command-line, then the link will not be followed and no processing is done.
 *
 * @param filename  The path to check.
 * @param parents   The parent directories' inodes (to check for loops).
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
static int process_path2(const char *filename, struct parent_dirs *parents)
{
	int ret = 0;
	int err;
	int fd;
	struct stat st;

	assert(filename != NULL);

	if (args.no_dereference) {
		err = lstat(filename, &st);
	}
	else {
		err = stat(filename, &st);
	}
	if (err != 0) {
		pr_err("Error: could not stat file \"%s\": %m\n", filename);
		return -1;
	}

	if (S_ISLNK(st.st_mode)) {
		return ret;
	}
	else if (!(S_ISREG(st.st_mode) || S_ISDIR(st.st_mode))) {
		pr_err("Error: \"%s\": not a regular file or directory\n", filename);
		return 1;
	}

	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		pr_err("Error: could not open file \"%s\": %m\n", filename);
		return 1;
	}

	if (S_ISREG(st.st_mode)) {
		ret = check_file(fd, filename, &st);
		close(fd);
	}
	else if (S_ISDIR(st.st_mode)) {
		if (!args.recursive) {
			pr_err("Error: \"%s\" is a directory\n", filename);
			close(fd);
			return 1;
		}

		ret = check_dir(fd, filename, &st, parents);
	}

	return ret;
}

/**
 * Figure out whether a file path is a file or directory and process it.
 *
 * If @p filename is a regular file, this will pass it to check_file().
 *
 * If @p filename is a directory and --recursive was set on the command-line,
 * this will pass it on to check_dir().
 *
 * @param filename  The path to check.
 *
 * @retval 0  The file was processed successfully.
 * @retval >0 An recoverable error occurred.
 * @retval <0 A fatal error occurred.
 */
int process_path(const char *filename)
{
	int ret;
	struct parent_dirs parents = { NULL, 0, 0 };

	ret = process_path2(filename, &parents);

	free(parents.data);

	return ret;
}
