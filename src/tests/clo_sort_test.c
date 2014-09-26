/*
 * This file is part of CL-Ops.
 *
 * CL-Ops is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CL-Ops is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CL-Ops.  If not, see <http://www.gnu.org/licenses/>.
 * */

/**
 * @file
 * @brief Test sorting algorithms.
 */

#include "clo_sort_test.h"

#define CLO_SORT_TEST_TYPE "uint"
#define CLO_SORT_TEST_RUNS 1
#define CLO_SORT_TEST_MAXPO2 24
#define CLO_SORT_TEST_ALGORITHM "sbitonic"
#define CLO_SORT_TEST_ALG_OPTS ""

/** A description of the program. */
#define CLO_SORT_DESCRIPTION "Test sorting algorithms"


/* Command line arguments and respective default values. */
static gchar *algorithm = NULL;
static gchar *alg_options = NULL;
static guint32 runs = CLO_SORT_TEST_RUNS;
static size_t lws = 0;
static int dev_idx = -1;
static guint32 rng_seed = CLO_DEFAULT_SEED;
static gchar* type = NULL;
static guint32 maxpo2 = CLO_SORT_TEST_MAXPO2;
static gchar* out = NULL;
static gchar* compiler_opts = NULL;

/* Valid command line options. */
static GOptionEntry entries[] = {
	{"algorithm",    'a', 0, G_OPTION_ARG_STRING,   &algorithm,     "Sorting algorithm to use (default is " CLO_SORT_TEST_ALGORITHM ")",                                            "ALGORITHM"},
	{"alg-opts",     'g', 0, G_OPTION_ARG_STRING,   &alg_options,   "Algorithm options",                 "STRING"},
	{"runs",         'r', 0, G_OPTION_ARG_INT,      &runs,          "Number of runs (default is " STR(CLO_SORT_TEST_RUNS) ")",                           "RUNS"},
	{"localsize",    'l', 0, G_OPTION_ARG_INT,      &lws,           "Maximum local work size (default is auto-select)",                       "SIZE"},
	{"device",       'd', 0, G_OPTION_ARG_INT,      &dev_idx,       "Device index",                                                                 "INDEX"},
	{"rng-seed",     's', 0, G_OPTION_ARG_INT,      &rng_seed,      "Seed for random number generator (default is " STR(CLO_DEFAULT_SEED) ")",      "SEED"},
	{"type",         't', 0, G_OPTION_ARG_STRING,   &type,          "Type of elements to sort (default " CLO_SORT_TEST_TYPE ")",     "TYPE"},
	{"maxpo2",       'n', 0, G_OPTION_ARG_INT,      &maxpo2,        "Log2 of the maximum number of elements to sort, e.g. 2^N (default N=" STR(CLO_SORT_TEST_MAXPO2) ")", "N"},
	{"out",          'o', 0, G_OPTION_ARG_STRING,   &out,           "File where to output sorting benchmarks (default is no file output)",          "FILENAME"},
	{"compiler",     'c', 0, G_OPTION_ARG_STRING,   &compiler_opts, "Compiler options",                 "STRING"},
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

/**
 * @brief Main program.
 *
 * @param argc Number of command line arguments.
 * @param argv Vector of command line arguments.
 * @return @link clo_error_codes::CLO_SUCCESS @endlink if program
 * terminates successfully, or another value of #clo_error_codes if an
 * error occurs.
 * */
int main(int argc, char **argv)
{

	/* Status var aux */
	int status;

	/* Context object for command line argument parsing. */
	GOptionContext *context = NULL;

	/* Test data structures. */
	gchar* host_data = NULL;
	size_t bytes;
	double total_time, duration;
	FILE *outfile = NULL;
	CloType clotype_elem;

	/* Sorter object. */
	CloSort* sorter = NULL;

	/* cf4ocl wrappers. */
	CCLQueue* queue = NULL;
	CCLContext* ctx = NULL;
	CCLDevice* dev = NULL;

	/* Host-based random number generator (mersenne twister) */
	GRand* rng_host = NULL;

	/* Error management object. */
	GError *err = NULL;

	/* Sorting benchmarks. */
	gdouble** benchmarks = NULL;

	/* Parse command line options. */
	context = g_option_context_new (" - " CLO_SORT_DESCRIPTION);
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_parse(context, &argc, &argv, &err);
	ccl_if_err_goto(err, error_handler);

	clotype_elem = clo_type_by_name(
		type != NULL ? type : CLO_SORT_TEST_TYPE, &err);
	ccl_if_err_goto(err, error_handler);

	if (algorithm == NULL) algorithm = g_strdup(CLO_SORT_TEST_ALGORITHM);
	if (alg_options == NULL) alg_options = g_strdup(CLO_SORT_TEST_ALG_OPTS);

	/* Determine size in bytes of each element to sort. */
	bytes = clo_type_sizeof(clotype_elem, NULL);

	/* Initialize random number generator. */
	rng_host = g_rand_new_with_seed(rng_seed);

	/* Get the context wrapper and the chosen device. */
	ctx = ccl_context_new_from_menu_full(&dev_idx, &err);
	ccl_if_err_goto(err, error_handler);
	dev = ccl_context_get_device(ctx, 0, &err);
	ccl_if_err_goto(err, error_handler);

	/* Get sorter object. */
	sorter = clo_sort_new(
		algorithm, alg_options, ctx, clotype_elem, compiler_opts, &err);
	ccl_if_err_goto(err, error_handler);

	/* Create command queue. */
	queue = ccl_queue_new(ctx, dev, 0, &err);
	ccl_if_err_goto(err, error_handler);

	/* Print options. */
	printf("\n   =========================== Selected options ============================\n\n");
	printf("     Random number generator seed: %u\n", rng_seed);
	printf("     Maximum local worksize (0 is auto-select): %d\n", (int) lws);
	printf("     Type of elements to sort: %s\n", clo_type_get_name(clotype_elem, NULL));
	printf("     Number of runs: %d\n", runs);
	printf("     Compiler Options: %s\n", compiler_opts);


	/* Create benchmarks table. */
	benchmarks = g_new(gdouble*, maxpo2);
	for (unsigned int i = 0; i < maxpo2; i++)
		benchmarks[i] = g_new0(gdouble, runs);

	/* Create host buffer. */
	host_data = g_slice_alloc(bytes * (1 << maxpo2));

	/* Perform test. */
	for (unsigned int N = 1; N <= maxpo2; N++) {

		unsigned int num_elems = 1 << N;
		gboolean sorted_ok;

		for (unsigned int r = 0; r < runs; r++) {

			/* Initialize host buffer. */
			for (unsigned int i = 0;  i < num_elems; i++) {
				/* Get a random 64-bit value by default... */
				gulong value = (gulong) (g_rand_double(rng_host) * G_MAXULONG);
				/* But just use the specified bits. */
				memcpy(host_data + bytes*i, &value, bytes);
			}

			/* Perform sort. */
			sorter->sort_with_host_data(sorter, queue, host_data,
				host_data, num_elems, lws, &duration, &err);
			ccl_if_err_goto(err, error_handler);

			/* Save duration to benchmarks. */
			benchmarks[N - 1][r] = duration;

			/* Check if sorting was well performed. */
			sorted_ok = TRUE;
			gulong value1 = 0, value2 = 0;
			for (unsigned int i = 0;  i < num_elems - 1; i++) {
				/* Compare value two by two... */
				value1 = 0; value2 = 0;
				/* Get values. */
				memcpy(&value1, host_data + bytes*i, bytes);
				memcpy(&value2, host_data + bytes*(i + 1), bytes);
				/* Compare. */
				if (value1 > value2) {
					sorted_ok = FALSE;
					break;
				}

			}

		}

		/* Print info. */
		total_time = 0;
		for (unsigned int i = 0;  i < runs; i++)
			total_time += benchmarks[N - 1][i];
		printf("       - 2^%d: %f Mkeys/s %s\n", N, 1e-6 * num_elems * runs / total_time, sorted_ok ? "" : "(sort did not work)");
	}

	/* Save benchmarks to file, if filename was given as cli option. */
	if (out) {
		outfile = fopen(out, "w");
		for (unsigned int i = 0; i < maxpo2; i++) {
			fprintf(outfile, "%d", i);
			for (unsigned int j = 0; j < runs; j++) {
				fprintf(outfile, "\t%lf", benchmarks[i][j]);
			}
			fprintf(outfile, "\n");
		}
		fclose(outfile);
	}

	/* If we get here, everything went Ok. */
	status = CLO_SUCCESS;
	g_assert(err == NULL);
	goto cleanup;

error_handler:
	/* Handle error. */
	g_assert(err != NULL);
	fprintf(stderr, "Error: %s\n", err->message);
	g_error_free(err);

cleanup:

	/* Free command line options. */
	if (context) g_option_context_free(context);
	if (algorithm) g_free(algorithm);
	if (alg_options) g_free(alg_options);
	if (compiler_opts) g_free(compiler_opts);
	if (out) g_free(out);

	/* Free benchmarks. */
	if (benchmarks) {
		for (unsigned int i = 0; i < maxpo2; i++)
			if (benchmarks[i]) g_free(benchmarks[i]);
		g_free(benchmarks);
	}

	/* Free host-based random number generator. */
	if (rng_host) g_rand_free(rng_host);

	/* Free sorter object. */
	if (sorter) sorter->destroy(sorter);

	/* Free host resources */
	if (host_data) g_slice_free1(bytes * (1 << maxpo2), host_data);

	/* Bye. */
	return status;

}


