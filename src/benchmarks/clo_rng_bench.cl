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
 * Kernel for RNGs benchmark
 */

__kernel void clo_rng_bench(
		__global clo_statetype *seeds,
		__global uint *result,
		const uint bits) {

	/* Grid position for this work-item. */
	uint gid = get_global_id(0);

#ifdef CLO_RNG_BENCHMARK_MAXINT
	result[gid] = clo_rng_next_int(seeds, bits);
#else
	result[gid] = clo_rng_next(seeds, gid) >> (32 - bits);
#endif

}

