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
 * @brief Advanced bitonic sort host implementation.
 */

#include "clo_sort_abitonic.h"

struct clo_sort_abitonic_data {

	/** Maximum number of in-kernel steps for private memory kernels. */
	cl_uint max_inkrnl_stps;

	/** Minimum number of in-kernel steps for private memory kernels. */
	cl_uint min_inkrnl_stps;

	/** Maximum in-kernel "stage finish" step. */
	cl_uint max_inkrnl_sfs;

};

/**
 * @internal
 * Perform sort using device data.
 *
 * @copydetails ::CloSort::sort_with_device_data()
 * */
static CCLEventWaitList clo_sort_abitonic_sort_with_device_data(
	CloSort* sorter, CCLQueue* cq_exec, CCLQueue* cq_comm,
	CCLBuffer* data_in, CCLBuffer* data_out, size_t numel,
	size_t lws_max, GError** err) {

	/* Worksizes. */
	size_t lws, gws;

	/* Number of bitonic sort stages. */
	cl_uint tot_stages;

	/* Implementation of the strategy to follow on each step. */
	clo_sort_abitonic_step* steps = NULL;

	/* OpenCL object wrappers. */
	CCLDevice* dev;
	CCLKernel* krnl;
	CCLEvent* evt;

	/* Event wait list. */
	CCLEventWaitList ewl = NULL;

	/* Internal error reporting object. */
	GError* err_internal = NULL;

	/* If data transfer queue is NULL, use exec queue for data
	 * transfers. */
	if (cq_comm == NULL) cq_comm = cq_exec;

	/* Get device where sort will occurr. */
	dev = ccl_queue_get_device(cq_exec, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Get the kernel wrapper. */
	krnl = ccl_program_get_kernel(clo_sort_get_program(sorter),
		"clo_sort_sbitonic", &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Determine worksizes. */
	if (lws_max != 0) {
		lws = MIN(lws_max, clo_nlpo2(numel) / 2);
		gws = CLO_GWS_MULT(numel / 2, lws);
	} else {
		size_t realws = numel / 2 + numel % 2;
		ccl_kernel_suggest_worksizes(
			krnl, dev, 1, &realws, &gws, &lws, &err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);
	}

	/* Determine number of bitonic sort stages. */
	tot_stages = (cl_uint) clo_tzc(gws * 2);

	/* Determine which buffer to use. */
	if (data_out == NULL) {
		/* Sort directly in original data. */
		data_out = data_in;
	} else {
		/* Copy data_in to data_out first, and then sort on copied
		 * data. */
		evt = ccl_buffer_enqueue_copy(data_in, data_out, cq_comm, 0, 0,
			clo_sort_get_element_size(sorter) * numel, NULL,
			&err_internal);

		ccl_if_err_propagate_goto(err, err_internal, error_handler);
		ccl_event_set_name(evt, "copy_sbitonic");
		ccl_event_wait_list_add(&ewl, evt, NULL);
	}

	/* Set first kernel argument. */
	ccl_kernel_set_arg(krnl, 0, data_out);

	/* Perform simple bitonic sort. */
	for (cl_uint curr_stage = 1; curr_stage <= tot_stages; curr_stage++) {

		ccl_kernel_set_arg(krnl, 1, ccl_arg_priv(curr_stage, cl_uint));

		cl_uint step = curr_stage;

		for (cl_uint curr_step = step; curr_step > 0; curr_step--) {

			ccl_kernel_set_arg(krnl, 2, ccl_arg_priv(curr_step, cl_uint));

			evt = ccl_kernel_enqueue_ndrange(krnl, cq_exec, 1, NULL,
				&gws, &lws, &ewl, &err_internal);
			ccl_if_err_propagate_goto(err, err_internal, error_handler);
			ccl_event_set_name(evt, "ndrange_sbitonic");

		}
	}

	/* Add last event to wait list to return. */
	ccl_event_wait_list_add(&ewl, evt, NULL);

	/* If we got here, everything is OK. */
	g_assert(err == NULL || *err == NULL);
	goto finish;

error_handler:
	/* If we got here there was an error, verify that it is so. */
	g_assert(err == NULL || *err != NULL);

finish:

	/* Return. */
	return ewl;

}

/**
 * @internal
 * Perform sort using host data.
 *
 * @copydetails ::CloSort::sort_with_host_data()
 * */
static cl_bool clo_sort_abitonic_sort_with_host_data(CloSort* sorter,
	CCLQueue* cq_exec, CCLQueue* cq_comm, void* data_in, void* data_out,
	size_t numel, size_t lws_max, GError** err) {

	/* Function return status. */
	cl_bool status;

	/* OpenCL wrapper objects. */
	CCLContext* ctx = NULL;
	CCLBuffer* data_in_dev = NULL;
	CCLQueue* intern_queue = NULL;
	CCLDevice* dev = NULL;
	CCLEvent* evt;

	/* Event wait list. */
	CCLEventWaitList ewl = NULL;

	/* Internal error object. */
	GError* err_internal = NULL;

	/* Determine data size. */
	size_t data_size = numel * clo_sort_get_element_size(sorter);

	/* Get context wrapper. */
	ctx = clo_sort_get_context(sorter);

	/* If execution queue is NULL, create own queue using first device
	 * in context. */
	if (cq_exec == NULL) {
		/* Get first device in context. */
		dev = ccl_context_get_device(ctx, 0, &err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);
		/* Create queue. */
		intern_queue = ccl_queue_new(ctx, dev, 0, &err_internal);
		ccl_if_err_propagate_goto(err, err_internal, error_handler);
		cq_exec = intern_queue;
	}

	/* If data transfer queue is NULL, use exec queue for data
	 * transfers. */
	if (cq_comm == NULL) cq_comm = cq_exec;

	/* Create device buffer. */
	data_in_dev = ccl_buffer_new(
		ctx, CL_MEM_READ_WRITE, data_size, NULL, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Transfer data to device. */
	evt = ccl_buffer_enqueue_write(data_in_dev, cq_comm, CL_FALSE, 0,
		data_size, data_in, NULL, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);
	ccl_event_set_name(evt, "write_sbitonic");

	/* Explicitly wait for transfer (some OpenCL implementations don't
	 * respect CL_TRUE in data transfers). */
	ccl_event_wait_list_add(&ewl, evt, NULL);
	ccl_event_wait(&ewl, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Perform sort with device data. */
	ewl = clo_sort_abitonic_sort_with_device_data(sorter, cq_exec,
		cq_comm, data_in_dev, NULL, numel, lws_max, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* Transfer data back to host. */
	evt = ccl_buffer_enqueue_read(data_in_dev, cq_comm, CL_FALSE, 0,
		data_size, data_out, &ewl, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);
	ccl_event_set_name(evt, "read_sbitonic");

	/* Explicitly wait for transfer (some OpenCL implementations don't
	 * respect CL_TRUE in data transfers). */
	ccl_event_wait_list_add(&ewl, evt, NULL);
	ccl_event_wait(&ewl, &err_internal);
	ccl_if_err_propagate_goto(err, err_internal, error_handler);

	/* If we got here, everything is OK. */
	g_assert(err == NULL || *err == NULL);
	status = CL_TRUE;
	goto finish;

error_handler:

	/* If we got here there was an error, verify that it is so. */
	g_assert(err == NULL || *err != NULL);
	status = CL_FALSE;

finish:

	/* Free stuff. */
	if (data_in_dev) ccl_buffer_destroy(data_in_dev);
	if (intern_queue) ccl_queue_destroy(intern_queue);

	/* Return function status. */
	return status;

}

/**
 * Initializes an advanced bitonic sorter object and returns the
 * respective source code.
 *
 * @param[in] Sorter object to initialize.
 * @param[in] options Algorithm options.
 * @param[out] err Return location for a GError, or `NULL` if error
 * reporting is to be ignored.
 * @return Advanced bitonic sorter source code or `NULL` if an error
 * occurs.
 * */
const char* clo_sort_abitonic_init(
	CloSort* sorter, const char* options, GError** err) {

	const char* abitonic_src;
	
	struct clo_sort_abitonic_data* data;

	data = g_slice_new0(struct clo_sort_abitonic_data);

	/* Set internal data default values. */
	data->max_inkrnl_stps = 4;
	data->min_inkrnl_stps = 1;
	data->max_inkrnl_sfs = UINT_MAX;

	/* Number of tokens. */
	int num_toks;

	/* Tokenized options. */
	gchar** opts = NULL;
	gchar** opt = NULL;

	/* Value to read from current option. */
	guint value;

	/* Check options. */
	if (options) {
		opts = g_strsplit_set(options, ",", -1);
		for (guint i = 0; opts[i] != NULL; i++) {

			/* Ignore empty tokens. */
			if (opts[i][0] == '\0') continue;

			/* Parse current option, get key and value. */
			opt = g_strsplit_set(opts[i], "=", 2);

			/* Count number of tokens. */
			for (num_toks = 0; opt[num_toks] != NULL; num_toks++);

			/* If number of tokens is not 2 (key and value), throw error. */
			ccl_if_err_create_goto(*err, CLO_ERROR, num_toks != 2,
				CLO_ERROR_ARGS, error_handler,
				"Invalid option '%s' for abitonic sort.", opts[i]);

			/* Get option value. */
			value = atoi(opt[1]);

			/* Check key/value option. */
			if (g_strcmp0("minps", opt[0]) == 0) {
				/* Minimum in-kernel private memory steps key option. */
				ccl_if_err_create_goto(*err, CLO_ERROR,
					(value > 4) || (value < 1), CLO_ERROR_ARGS,
					error_handler,
					"Option 'minps' must be between 1 and 4.");
				data->min_inkrnl_stps = value;
			} else if (g_strcmp0("maxps", opt[0]) == 0) {
				/* Maximum in-kernel private memory steps key option. */
				ccl_if_err_create_goto(*err, CLO_ERROR,
					(value > 4) || (value < 1), CLO_ERROR_ARGS,
					error_handler,
					"Option 'maxps' must be between 1 and 4.");
				data->max_inkrnl_stps = value;
			} else if (g_strcmp0("maxsfs", opt[0]) == 0) {
				/* Maximum in-kernel "stage finish" step. */
				data->max_inkrnl_sfs = value;
			} else {
				ccl_if_err_create_goto(*err, CLO_ERROR, TRUE,
					CLO_ERROR_ARGS, error_handler,
					"Invalid option key '%s' for a-bitonic sort.", opt[0]);
			}

			/* Free token. */
			g_strfreev(opt);
			opt = NULL;

		}
		ccl_if_err_create_goto(*err, CLO_ERROR,
			data->max_inkrnl_stps < data->min_inkrnl_stps, 
			CLO_ERROR_ARGS, error_handler,
			"'minps' (%d) must be less or equal than 'maxps' (%d).",
			data->min_inkrnl_stps, data->max_inkrnl_stps);

	}

	/* If we got here, everything is OK. */
	g_assert(err == NULL || *err == NULL);
	abitonic_src = CLO_SORT_ABITONIC_SRC;
	goto finish;

error_handler:
	/* If we got here there was an error, verify that it is so. */
	g_assert(err == NULL || *err != NULL);
	abitonic_src = NULL;

finish:

	/* Free parsed a-bitonic options. */
	g_strfreev(opts);
	g_strfreev(opt);

	/* Set object methods and internal data. */
	sorter->sort_with_host_data = clo_sort_abitonic_sort_with_host_data;
	sorter->sort_with_device_data = clo_sort_abitonic_sort_with_device_data;
	sorter->_data = (void*) data;

	/* Return source to be compiled. */
	return abitonic_src;

}


/**
 * Finalizes an advanced bitonic sorter object.
 *
 * @param[in] sorter Sorter object to finalize.
 * */
void clo_sort_abitonic_finalize(CloSort* sorter) {

	/* Release internal data. */
	g_slice_free(struct clo_sort_abitonic_data, sorter->_data);

	return;
}
