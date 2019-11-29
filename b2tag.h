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
 * Shared functions for the b2tag utility.
 */

#ifndef B2TAG_H
#define B2TAG_H

#include <stdbool.h>

#include "hash.h"

/**
 * The options passed to the program on the command-line.
 */
struct args_s {
	/** Which hash algorithm to use. */
	hash_alg_t alg;
	/** Whether to check the hashes on up-to-date files. */
	bool check;
	/** Don't change any extended attributes. */
	bool dry_run;
	/** Whether to update the hashes on backdated, corrupt, or invalid files. */
	bool force;
	/** Do not follow symbolic links when processing a file. */
	bool no_dereference;
	/** Print file hashes in the standard sha*sum, etc. format. */
	bool print;
	/** Process all files under the specified directories. */
	bool recursive;
	/** The verbosity level (how many messages to print). */
	int verbose;
};

/** The options set by command-line arguments. */
extern struct args_s args;

#endif /* B2TAG_H */
