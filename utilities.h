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
 * Utility macro and function declarations.
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>
#include <time.h>

#include "b2tag.h"

#ifndef MSEC_PER_SEC
#define MSEC_PER_SEC 1000
#endif

#ifndef USEC_PER_SEC
#define USEC_PER_SEC 1000000
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000
#endif


/**
 * Compare two timespec structures.
 *
 * @param ts1    Timespec structure to compare.
 * @param ts2    Timespec structure to compare.
 * @param fuzzy  If true, consider timestamps within 1us equal.
 *
 * @retval <0 @p ts1 is earlier than @p ts2.
 * @retval 0 @p ts1 and @p ts2 are equal.
 * @retval >0 @p ts1 is later than @p ts2.
 */
int ts_compare(struct timespec ts1, struct timespec ts2, bool fuzzy);

/**
 * Prints an error message to stderr and exits the program.
 *
 * @param fmt  The printf-style format string to display.
 * @param ...  Additional arguments for @p fmt.
 */
void die(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));

/** Returns the number of elements in a statically allocated array. */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/**
 * @internal
 * Helper macro to tell if the program's verbosity is higher than @p level.
 */
#define _check_level(level) (args.verbose >= (level))

/** Returns whether critical error messages should be printed. */
#define check_crit()  _check_level(-1)
/** Returns whether error messages should be printed. */
#define check_err()   _check_level(0)
/** Returns whether warning messages should be printed. */
#define check_warn()  _check_level(1)
/** Returns whether debug messages should be printed. */
#define check_debug() _check_level(2)

/**
 * @internal
 * Helper macro to print a message if the program's verbosity is higher than @p level.
 */
#define _print(level, ...) \
	do { \
		if (check_##level()) \
			fprintf(stderr, __VA_ARGS__); \
	} while (0)

/** Print a critical error message. */
#define pr_crit(...)  _print(crit,  __VA_ARGS__)
/** Print an error message. */
#define pr_err(...)   _print(err,   __VA_ARGS__)
/** Print a warning message. */
#define pr_warn(...)  _print(warn,  __VA_ARGS__)
/** Print a debug message. */
#define pr_debug(...) _print(debug, __VA_ARGS__)

#endif /* UTILITIES_H */
