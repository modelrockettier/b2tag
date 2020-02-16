/*
 * Copyright (C) 2012 Jakob Unterwurzacher <jakobunt@gmail.com>
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
 * Main file for the b2tag utility (mostly command-line processing).
 */

/**
 * @mainpage
 * @include README
 */

#include "b2tag.h"

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>

#include "file.h"
#include "utilities.h"


/** The options set by command-line arguments. */
struct args_s args;

/**
 * Prints version information for b2tag.
 *
 * @note VERSION_STRING is generated in the Makefile and defined on the compiler command-line.
 */
static void version(void)
{
	printf("b2tag version %s\n", VERSION_STRING);
}

/**
 * Prints a usage message for b2tag.
 *
 * @param program  The name of the program being run.
 */
static void usage(const char *program)
{
	printf(
		"Usage: %s [OPTION]... <FILE>...\n"
		"\n"
		"Display and update xattr-based checksums.\n"
		"\n"
		"Positional arguments:\n"
		"  FILE                  files to checksum\n"
		"\n"
		"Optional arguments:\n"
		"  -c, --check           check the hashes on all specified files\n"
		"  -f, --force           update the stored hashes for backdated, corrupted, or\n"
		"                        invalid files\n"
		"  -h, --help            show this help message and exit\n"
		"  -n, --dry-run         don't update any stored attributes\n"
		"  -p, --print           print the hashes of all specified files\n"
		"  -P, --no-dereference  never follow symbolic links (default is to follow them)\n"
		"  -q, --quiet           only print errors (including checksum failures)\n"
		"  -r, --recursive       process directories and their contents (not just files)\n"
		"  -v, --verbose         print all checksums (not just missing/changed)\n"
		"  -V, --version         output version information and exit\n"
		"\n"
		"Hash algorithms:\n"
		"  --blake2b (default, 512-bit)  --blake2s (256-bit, recommended on 32-bit)\n"
		"  --sha512                      --sha256 (shatag compatible)\n"
		"  --sha1 (deprecated)           --md5 (deprecated)\n",
		program);
}

/**
 * Long options to pass to getopt.
 */
static const struct option long_opts[] = {
	{ "check",          no_argument, 0, 'c' },
	{ "dry-run",        no_argument, 0, 'n' },
	{ "force",          no_argument, 0, 'f' },
	{ "help",           no_argument, 0, 'h' },
	{ "no-dereference", no_argument, 0, 'P' },
	{ "print",          no_argument, 0, 'p' },
	{ "quiet",          no_argument, 0, 'q' },
	{ "recursive",      no_argument, 0, 'r' },
	{ "verbose",        no_argument, 0, 'v' },
	{ "version",        no_argument, 0, 'V' },
	{ "md5",            no_argument, 0,  0  },
	{ "sha1",           no_argument, 0,  0  },
	{ "sha256",         no_argument, 0,  0  },
	{ "sha512",         no_argument, 0,  0  },
	{ "blake2",         no_argument, 0,  1  },
	{ "blake2b",        no_argument, 0,  1  },
	{ "blake2b512",     no_argument, 0,  1  },
	{ "blake2s",        no_argument, 0,  2  },
	{ "blake2s256",     no_argument, 0,  2  },
	{ NULL, 0, 0, 0 }
};

/**
 * The entry point to the b2tag utility.
 *
 * @param argc  The number of command-line arguments.
 * @param argv  The command-line arguments.
 *
 * @retval 0  Program completed successfully.
 * @retval !0 An error occurred.
 */
int main(int argc, char *argv[])
{
	int ret = 0;
	char *program = basename(argv[0]);
	int opt;
	int option_index = 0;

	args.alg = HASH_ALG_BLAKE2B;

	while ((opt = getopt_long(argc, argv, "cfhnpPqrvV", long_opts, &option_index)) != -1) {
		switch (opt) {
		case 0:
			ret = get_alg_by_name(long_opts[option_index].name, &args.alg);
			assert(ret == 0);
			break;
		case 1:
			args.alg = HASH_ALG_BLAKE2B;
			break;
		case 2:
			args.alg = HASH_ALG_BLAKE2S;
			break;
		case 'c':
			args.check = true;
			break;
		case 'f':
			args.force = true;
			break;
		case 'h':
			usage(program);
			return EXIT_SUCCESS;
		case 'n':
			args.dry_run = true;
			break;
		case 'p':
			args.print = true;
			break;
		case 'P':
			args.no_dereference = true;
			break;
		case 'q':
			args.verbose--;
			break;
		case 'r':
			args.recursive = true;
			break;
		case 'v':
			args.verbose++;
			break;
		case 'V':
			version();
			return EXIT_SUCCESS;

		default:
			if (isprint(opt))
				fprintf(stderr, "Unknown argument '-%c' (%#02x)\n", opt, opt);
			else
				fprintf(stderr, "Unknown argument -%#02x\n", opt);

			/* Fall through. */

		case '?':
			usage(program);
			return EXIT_FAILURE;
		}
	}

	ret = 0;

	argc -= optind;
	argv += optind;

	if (argc < 1) {
		fprintf(stderr, "No file specified.\n");
		usage(program);
		return EXIT_FAILURE;
	}

	if (args.dry_run && args.force)
		pr_warn("Warning: --dry-run takes precedence over --force.\n");

	while (argc >= 1) {
		int err;
		char *pos = argv[0] + strlen(argv[0]) - 1;

		/* Remove trailing slashes */
		while (pos > argv[0] && *pos == '/')
			*pos-- = '\0';

		err = process_path(argv[0]);

		if (err < 0)
			return ret;
		else if (ret == 0 && err > 0)
			ret = err;

		argc--;
		argv++;
	}

	return ret;
}
