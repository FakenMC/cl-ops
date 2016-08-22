/*
 * This file is part of CL_Ops.
 *
 * CL_Ops is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CL_Ops is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CL_Ops.  If not, see <http://www.gnu.org/licenses/>.
 * */

/**
 * @file
 * Scan benchmark implementation.
 */

#include "clo_scan_bench.h"
#include "common/_g_err_macros.h"

#define CLO_SCAN_BENCHMARK_RUNS 1
#define CLO_SCAN_BENCHMARK_INITELEMS 4
#define CLO_SCAN_BENCHMARK_NUMDOUB 24
#define CLO_SCAN_BENCHMARK_TYPE "uint"
#define CLO_SCAN_BENCHMARK_TYPE_SUM "ulong"
#define CLO_SCAN_BENCHMARK_ALGORITHM "blelloch"
#define CLO_SCAN_BENCHMARK_ALG_OPTS ""

/** A description of the program. */
#define CLO_SCAN_DESCRIPTION "Test CL_Ops scan implementations"

/* Command line arguments and respective default values. */
static guint32 runs = CLO_SCAN_BENCHMARK_RUNS;
static size_t lws = 0;
static int dev_idx = -1;
static guint32 rng_seed = CLO_DEFAULT_SEED;
static gchar* type = NULL;
static gchar* type_sum = NULL;
static guint32 init_elems = CLO_SCAN_BENCHMARK_INITELEMS;
static guint32 num_doub = CLO_SCAN_BENCHMARK_NUMDOUB;
static gchar* out = NULL;
static gboolean no_check = FALSE;
static gchar* compiler_opts = NULL;
static gchar* algorithm = NULL;
static gchar* alg_options = NULL;

/* Valid command line options. */
static GOptionEntry entries[] = {
	{"runs",         'r', 0, G_OPTION_ARG_INT,    &runs,
		"Number of runs (default is " G_STRINGIFY(CLO_SCAN_BENCHMARK_RUNS) ")",
		"RUNS"},
	{"localsize",    'l', 0, G_OPTION_ARG_INT,    &lws,
		"Maximum local work size (default is auto-select)",
		"SIZE"},
	{"device",       'd', 0, G_OPTION_ARG_INT,    &dev_idx,
		"Device index",
		"INDEX"},
	{"rng-seed",     's', 0, G_OPTION_ARG_INT,    &rng_seed,
		"Seed for random number generator (default is " G_STRINGIFY(CLO_DEFAULT_SEED) ")",
		"SEED"},
	{"type",         't', 0, G_OPTION_ARG_STRING, &type,
		"Type of elements to scan (default " CLO_SCAN_BENCHMARK_TYPE ")",
		"TYPE"},
	{"type-sum",     'y', 0, G_OPTION_ARG_STRING, &type_sum,
		"Type of elements in scan result (default " CLO_SCAN_BENCHMARK_TYPE_SUM ")",
		"TYPE"},
	{"init-elems",   'i', 0, G_OPTION_ARG_INT,    &init_elems,
		"The starting number of elements to scan (default is " G_STRINGIFY(CLO_SCAN_BENCHMARK_INITELEMS) ")",
		"INIT"},
	{"num-doub",     'n', 0, G_OPTION_ARG_INT,    &num_doub,
		"Number of times min-elems is doubled (default is " G_STRINGIFY(CLO_SCAN_BENCHMARK_NUMDOUB) ")",
		"DOUB"},
	{"compiler",     'c', 0, G_OPTION_ARG_STRING, &compiler_opts,
		"Compiler options",                 "STRING"},
	{"algorithm",    'a', 0, G_OPTION_ARG_STRING, &algorithm,
		"Scan algorithm to use (default is '" CLO_SCAN_BENCHMARK_ALGORITHM "')",
		"STRING"},
	{"alg-opts",     'p', 0, G_OPTION_ARG_STRING, &alg_options,
		"Algorithm options",                 "STRING"},
	{"no-check",     'u', 0, G_OPTION_ARG_NONE,   &no_check,
		"Don't check scan with serial version",
		NULL},
	{"out",          'o', 0, G_OPTION_ARG_STRING, &out,
		"File where to output scan benchmarks (default is no file output)",
		"FILENAME"},
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};


/**
 * Main program.
 *
 * @param[in] argc Number of command line arguments.
 * @param[in] Vector of command line arguments.
 * @return ::CLO_SUCCESS if program terminates successfully, or another
 * value of ::clo_error_codes if an error occurs.
 * */
int main(int argc, char **argv) {

	/* Status var aux */
	int status;

	/* Context object for command line argument parsing. */
	GOptionContext *context = NULL;

	/* Test data structures. */
	gchar* host_data = NULL;
	gchar* host_data_scanned = NULL;
	size_t bytes, bytes_sum;
	CloType clotype_elem;
	CloType clotype_sum;
	size_t max_buffer_size, max_buffer_size_sum;
	gdouble total_time;
	FILE *outfile = NULL;

	/* Scanner object. */
	CloScan* scanner = NULL;

	/* cf4ocl wrappers. */
	CCLQueue* cq_exec = NULL;
	CCLQueue* cq_comm = NULL;
	CCLContext* ctx = NULL;
	CCLDevice* dev = NULL;

	/* Profiler object. */
	CCLProf* prof;

	/* Host-based random number generator (mersenne twister) */
	GRand* rng_host = NULL;

	/* Error management object. */
	GError *err = NULL;

	/* Scan benchmarks. */
	gdouble** benchmarks = NULL;

	/* Parse command line options. */
	context = g_option_context_new (" - " CLO_SCAN_DESCRIPTION);
	g_option_context_add_main_entries(context, entries, NULL);
	g_option_context_parse(context, &argc, &argv, &err);
	g_if_err_goto(err, error_handler);

	clotype_elem = clo_type_by_name(
		type != NULL ? type : CLO_SCAN_BENCHMARK_TYPE, &err);
	g_if_err_goto(err, error_handler);
	clotype_sum = clo_type_by_name(
		type_sum != NULL ? type_sum : CLO_SCAN_BENCHMARK_TYPE_SUM, &err);
	g_if_err_goto(err, error_handler);

	if (algorithm == NULL) algorithm = g_strdup(CLO_SCAN_BENCHMARK_ALGORITHM);
	if (alg_options == NULL) alg_options = g_strdup(CLO_SCAN_BENCHMARK_ALG_OPTS);

	/* Determine size in bytes of each element to sort. */
	bytes = clo_type_sizeof(clotype_elem);
	bytes_sum = clo_type_sizeof(clotype_sum);

	/* Initialize random number generator. */
	rng_host = g_rand_new_with_seed(rng_seed);

	/* Get the context wrapper and the chosen device. */
	ctx = ccl_context_new_from_menu_full(&dev_idx, &err);
	g_if_err_goto(err, error_handler);
	dev = ccl_context_get_device(ctx, 0, &err);
	g_if_err_goto(err, error_handler);

	/* Get scan object. */
	scanner = clo_scan_new(algorithm, alg_options, ctx, clotype_elem,
		clotype_sum, compiler_opts, &err);
	g_if_err_goto(err, error_handler);

	/* Create command queues. */
	cq_exec = ccl_queue_new(ctx, dev, CL_QUEUE_PROFILING_ENABLE, &err);
	g_if_err_goto(err, error_handler);
	cq_comm = ccl_queue_new(ctx, dev, 0, &err);
	g_if_err_goto(err, error_handler);

	/* Print options. */
	printf("\n   =========================== Selected options ============================\n\n");
	printf("     Random number generator seed: %u\n", rng_seed);
	printf("     Maximum local worksize (0 = auto): %d\n", (int) lws);
	printf("     Type of elements to scan: %s\n", clo_type_get_name(clotype_elem));
	printf("     Type of elements in scan result: %s\n", clo_type_get_name(clotype_sum));
	printf("     Starting number of elements: %d\n", init_elems);
	printf("     Number of times number of elements will be doubled: %d\n", num_doub);
	printf("     Number of runs: %d\n", runs);
	printf("     Compiler Options: %s\n", compiler_opts);

	/* Create benchmarks table. */
	benchmarks = g_new(gdouble*, num_doub);
	for (unsigned int i = 0; i < num_doub; i++)
		benchmarks[i] = g_new0(gdouble, runs);

	/* Determine maximum buffer size. */
	max_buffer_size = bytes * init_elems * (1 << num_doub);
	max_buffer_size_sum = bytes_sum * init_elems * (1 << num_doub);

	/* Create host buffers */
	host_data = g_new0(gchar, max_buffer_size);
	host_data_scanned = g_new0(gchar, max_buffer_size_sum);

	/* Start with the inital number of elements. */
	unsigned int num_elems = init_elems;

	/* Perform test. */
	for (unsigned int N = 1; N <= num_doub; N++) {

		const gchar* scan_ok;

		for (unsigned int r = 0; r < runs; r++) {

			g_debug("|===== Num. elems: %d (run %d): =====|", num_elems, r);

			/* Initialize host buffer. */
			for (unsigned int i = 0;  i < num_elems; i++) {
				/* Get a random 64-bit value by default, but keep it small... */
				gulong value = (gulong) (g_rand_double(rng_host) * 128);
				/* But just use the specified bits. */
				memcpy(host_data + bytes * i, &value, bytes);
			}

			/* Perform scan. */
			clo_scan_with_host_data(scanner, cq_exec, cq_comm,
				host_data, host_data_scanned, num_elems, lws, &err);
			g_if_err_goto(err, error_handler);

			/* Perform profiling. */
			prof = ccl_prof_new();
			ccl_prof_add_queue(prof, "q_exec", cq_exec);
			ccl_prof_calc(prof, &err);
			g_if_err_goto(err, error_handler);

			/* Save time to benchmarks. */
			benchmarks[N - 1][r] =  ccl_prof_get_duration(prof);
			ccl_prof_destroy(prof);

			/* Wait on host thread for data transfer queue to finish... */
			ccl_queue_finish(cq_comm, &err);
			g_if_err_goto(err, error_handler);

			/* Check if scan was well performed. */
			if (no_check) {
				scan_ok = "[Unverified]";
			} else {
				g_debug("== CHECK ==");
				g_debug("%10s %10s %10s", "Host", "Serial", "Dev");
				scan_ok = "";
				gulong value_dev = 0, value_host = 0;
				for (unsigned int i = 0; i < num_elems; i++) {
					/* Perform a CPU scan. */
					value_host = (i == 0) ? 0 : value_host + CLO_SCAN_HOST_GET(host_data, i - 1, bytes);
					/* Check for overflow. */
					if (value_host > CLO_SCAN_MAXU(bytes_sum)) {
						scan_ok = "[Overflow]";
						break;
					}
					/* Get device value. */
					memcpy(&value_dev, host_data_scanned + bytes_sum * i, bytes_sum);
					/* Compare. */
					if (value_dev != value_host) {
						scan_ok = "[Scan did not work]";
						break;
					}
					g_debug("%10lu %10lu %10lu", CLO_SCAN_HOST_GET(host_data, i, bytes), value_host, value_dev);

				}
			}
		}

		/* Print info. */
		total_time = 0;
		for (unsigned int i = 0;  i < runs; i++)
			total_time += benchmarks[N - 1][i];
		printf("       - %10d : %f MValues/s %s\n", num_elems, (1e-6 * num_elems * runs) / (total_time * 1e-9), scan_ok);

		num_elems *= 2;

	}

	/* Save benchmarks to file, if filename was given as cli option. */
	if (out) {
		outfile = fopen(out, "w");
		for (unsigned int i = 0; i < num_doub; i++) {
			fprintf(outfile, "%d", i);
			for (unsigned int j = 0; j < runs; j++) {
				fprintf(outfile, "\t%lf", benchmarks[i][j]);
			}
			fprintf(outfile, "\n");
		}
		fclose(outfile);
	}

	/* If we get here, everything went Ok. */
	g_assert(err == NULL);
	status = CLO_SUCCESS;
	goto cleanup;

error_handler:
	/* Handle error. */
	g_assert(err != NULL);
	fprintf(stderr, "Error: %s\n", err->message);
	status = err->code;
	g_error_free(err);

cleanup:

	/* Destroy scanner object. */
	if (scanner) clo_scan_destroy(scanner);

	/* Release OpenCL wrappers. */
	if (cq_exec) ccl_queue_destroy(cq_exec);
	if (cq_comm) ccl_queue_destroy(cq_comm);
	if (ctx) ccl_context_destroy(ctx);

	/* Free host resources */
	g_free(host_data);
	g_free(host_data_scanned);

	/* Free command line options. */
	if (context) g_option_context_free(context);
	if (compiler_opts) g_free(compiler_opts);
	if (out) g_free(out);
	if (algorithm) g_free(algorithm);
	if (alg_options) g_free(alg_options);

	/* Free benchmarks. */
	if (benchmarks) {
		for (unsigned int i = 0; i < num_doub; i++)
			if (benchmarks[i]) g_free(benchmarks[i]);
		g_free(benchmarks);
	}

	/* Free host-based random number generator. */
	if (rng_host) g_rand_free(rng_host);

	/* Bye bye. */
	return status;

}

