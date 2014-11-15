/*
 * This file is part of CL-Ops.
 *
 * CL-Ops is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * CL-Ops is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with CL-Ops. If not, see
 * <http://www.gnu.org/licenses/>.
 * */

/**
 * @file
 * Global memory selection sort host implementation.
 */

#include "cl_ops/clo_sort_gselect.h"

/**
 * @internal
 * Perform sort using device data.
 * */
static CCLEvent* clo_sort_gselect_sort_with_device_data(
	CloSort* sorter, CCLQueue* cq_exec, CCLQueue* cq_comm,
	CCLBuffer* data_in, CCLBuffer* data_out, size_t numel,
	size_t lws_max, GError** err) {

	/* Make sure err is NULL or it is not set. */
	g_return_val_if_fail(err == NULL || *err == NULL, NULL);

	/* Make sure cq_exec is not NULL. */
	g_return_val_if_fail(cq_exec != NULL, NULL);

	/* Worksizes. */
	size_t lws, gws;

	/* OpenCL object wrappers. */
	CCLContext* ctx = NULL;
	CCLDevice* dev = NULL;
	CCLKernel* krnl = NULL;
	CCLEvent* evt = NULL;

	/* Event wait list. */
	CCLEventWaitList ewl = NULL;

	/* Internal error reporting object. */
	GError* err_internal = NULL;

	/* Flag indicating if sorted data is to be copied back to original
	 * buffer, simulating an in-place sort. */
	cl_bool copy_back;

	/* If data transfer queue is NULL, use exec queue for data
	 * transfers. */
	if (cq_comm == NULL) cq_comm = cq_exec;

	/* Get device where sort will occurr. */
	dev = ccl_queue_get_device(cq_exec, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Get the kernel wrapper. */
	krnl = ccl_program_get_kernel(clo_sort_get_program(sorter),
		"gselect", &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Determine worksizes. */
	gws = numel;
	lws = lws_max;
	ccl_kernel_suggest_worksizes(
		krnl, dev, 1, &gws, NULL, &lws, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Check if data_out is set. */
	if (data_out == NULL) {
		/* If not create it and set the copy back flag to TRUE. */

		/* Get context. */
		ctx = ccl_queue_get_context(cq_comm, &err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);

		/* Set copy-back flag to true. */
		copy_back = CL_TRUE;

		/* Create output buffer. */
		data_out = ccl_buffer_new(ctx, CL_MEM_WRITE_ONLY,
			numel * clo_sort_get_element_size(sorter), NULL,
			&err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);

	} else {

		/* Set copy back flag to FALSE. */
		copy_back = CL_FALSE;
	}

	/* Set kernel arguments. */
	cl_ulong numel_l = numel;
	ccl_kernel_set_args(
		krnl, data_in, data_out, ccl_arg_priv(numel_l, cl_ulong), NULL);

	/* Perform global memory selection sort. */
	evt = ccl_kernel_enqueue_ndrange(
		krnl, cq_exec, 1, NULL, &gws, &lws, NULL, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);
	ccl_event_set_name(evt, "gselect_ndrange");

	/* If copy-back flag is set, copy sorted data back to original
	 * buffer. */
	if (copy_back) {
		ccl_event_wait_list_add(&ewl, evt, NULL);
		evt = ccl_buffer_enqueue_copy(data_out, data_in, cq_comm, 0, 0,
			numel * clo_sort_get_element_size(sorter), &ewl,
			&err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);
		ccl_event_set_name(evt, "gselect_copy");
	}

	/* If we got here, everything is OK. */
	g_assert(err == NULL || *err == NULL);
	goto finish;

error_handler:
	/* If we got here there was an error, verify that it is so. */
	g_assert(err == NULL || *err != NULL);
	evt = NULL;

finish:

	/* Free data out buffer if copy-back flag is set. */
	if ((copy_back) && (data_out != NULL)) ccl_buffer_destroy(data_out);

	/* Return event wait list. */
	return evt;

}

/**
 * @internal
 * Initializes a simple bitonic sorter object and returns the
 * respective source code.
 * */
static const char* clo_sort_gselect_init(
	CloSort* sorter, const char* options, GError** err) {

	/* Make sure err is NULL or it is not set. */
	g_return_val_if_fail(err == NULL || *err == NULL, NULL);

	/* Ignore specific gselect sort options and error handling. */
	(void)options;
	(void)err;
	(void)sorter;

	/* Return source to be compiled. */
	return CLO_SORT_GSELECT_SRC;

}

/**
 * @internal
 * Finalizes a bitonic sorter object.
 * */
static void clo_sort_gselect_finalize(CloSort* sorter) {
	/* Nothing to finalize. */
	(void)sorter;
	return;
}

/**
 * @internal
 * Get the maximum number of kernels used by the sort implementation.
 * */
static cl_uint clo_sort_gselect_get_num_kernels(
	CloSort* sorter, GError** err) {

	/* Avoid compiler warnings. */
	(void)sorter;
	(void)err;

	/* Return number of kernels. */
	return 1;

}

/**
 * @internal
 * Get name of the i^th kernel used by the sort implementation.
 * */
static const char* clo_sort_gselect_get_kernel_name(
	CloSort* sorter, cl_uint i, GError** err) {

	/* i must be zero because there is only one kernel. */
	g_return_val_if_fail(i == 0, NULL);

	/* Avoid compiler warnings. */
	(void)sorter;
	(void)err;

	/* Return kernel name. */
	return CLO_SORT_GSELECT_KNAME;
}

/**
 * @internal
 * Get local memory usage of i^th kernel used by the sort implementation
 * for the given maximum local worksize and number of elements to sort.
 * */
static size_t clo_sort_gselect_get_localmem_usage(CloSort* sorter,
	cl_uint i, size_t lws_max, size_t numel, GError** err) {

	/* i must be zero because there is only one kernel. */
	g_return_val_if_fail(i == 0, NULL);

	/* Avoid compiler warnings. */
	(void)sorter;
	(void)lws_max;
	(void)numel;
	(void)err;

	/* Return local memory usage, which is zero for global selection
	 * sort. */
	return 0;

}

/* Definition of the gselect sort implementation. */
const CloSortImplDef clo_sort_gselect_def = {
	"gselect",
	CL_FALSE,
	clo_sort_gselect_init,
	clo_sort_gselect_finalize,
	clo_sort_gselect_sort_with_device_data,
	clo_sort_gselect_get_num_kernels,
	clo_sort_gselect_get_kernel_name,
	clo_sort_gselect_get_localmem_usage
};
