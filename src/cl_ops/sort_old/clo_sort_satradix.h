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
 * Satish et al radix sort header file.
 */

#ifndef CLO_SORTSATRADIX_H
#define CLO_SORTSATRADIX_H

#include "clo_sort.h"

/** Name of single Satish radix sort kernel. */
#define CLO_SORT_SATRADIX_KNAME_0 "satradixHistogram"

/** Number of command queues used by the Satish radix sort. */
#define CLO_SORT_SATRADIX_NUMQUEUES 1

/** Number of kernels used by the Satish radix sort. */
#define CLO_SORT_SATRADIX_NUMKERNELS 1

/** Array of strings containing names of the kernels used by the Satish radix sort strategy. */
#define CLO_SORT_SATRADIX_KERNELNAMES {CLO_SORT_SATRADIX_KNAME_0}

/** Sort elements using the Satish radix sort. */
int clo_sort_satradix_sort(cl_command_queue *queues, cl_kernel *krnls,
	size_t lws_max, size_t len, unsigned int numel, const char* options,
	GArray *evts, gboolean profile, GError **err);

/** Returns the name of the kernel identified by the given
 * index. */
const char* clo_sort_satradix_kernelname_get(unsigned int index);

/** Create kernels for the Satish radix sort. */
int clo_sort_satradix_kernels_create(cl_kernel **krnls, cl_program program, GError **err);

/** Get local memory usage for the Satish radix sort kernels. */
size_t clo_sort_satradix_localmem_usage(const char* kernel_name, size_t lws_max, size_t len, unsigned int numel);

/** Set kernels arguments for the Satish radix sort. */
int clo_sort_satradix_kernelargs_set(cl_kernel **krnls, cl_mem data, size_t lws, size_t len, GError **err);

/** Free the Satish radix sort kernels. */
void clo_sort_satradix_kernels_free(cl_kernel **krnls);

#endif

