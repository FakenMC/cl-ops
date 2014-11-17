/*
 * This file is part of CL_Ops.
 *
 * CL_Ops is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * CL_Ops is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with CL_Ops. If not, see
 * <http://www.gnu.org/licenses/>.
 * */

/**
 * @file
 * Implementation of XorShift random number generator with
 * 128 bit state.
 *
 * Based on code available [here](http://en.wikipedia.org/wiki/Xorshift).
 */

/* For the Xor-Shift128 RNG, the size of each seed is four integers. */
typedef uint4 clo_statetype;

#define clo_ulong2statetype(seed) (uint4) (0xFFFFFFFF & seed, 0xFFFFFFFF & (seed >> 16), 0xFFFFFFFF & (seed >> 32), 0xFFFFFFFF & (seed >> 46))

/**
 * Returns the next pseudorandom value using a xorshift random
 * number generator with 128 bit state.
 *
 * @param[in,out] states Array of RNG states.
 * @param[in] index Index of relevant state to use and update.
 * @return The next pseudorandom value using a xorshift random number
 * generator with 128 bit state.
 */
uint clo_rng_next(__global clo_statetype *states, uint index) {

	/* Get current state */
	clo_statetype state = states[index];

	/* Update state */
	uint t = state.x ^ (state.x << 11);
	state.x = state.y;
	state.y = state.z;
	state.z = state.w;
	state.w = state.w ^ (state.w >> 19) ^ (t ^ (t >> 8));

	/* Keep state */
	states[index] = state;

	/* Return value */
	return state.w;

}

