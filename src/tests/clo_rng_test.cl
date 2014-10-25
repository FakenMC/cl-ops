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
 * Kernel for testing RNGs
 */

__kernel void clo_rng_test(
		__global rng_state *seeds,
		__global uint *result,
		const uint bits) {

	/* Grid position for this work-item. */
	uint gid = get_global_id(0);

#ifdef CLO_RNCLO_RNG_TEST_MAXINT
	result[gid] = clo_rng_next_int(seeds, bits);
#else
	result[gid] = clo_rng_next(seeds, gid) >> (32 - bits);
#endif

}

