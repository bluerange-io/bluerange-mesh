////////////////////////////////////////////////////////////////////////////////
//MIT License
//
//Copyright(c) 2016 Jakob "Brotcrunsher" Schaal
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.
#include "MersenneTwister.h"
#include <cmath>
#include "Exceptions.h"

void MersenneTwister::twistIteration(uint32_t i)
{
	uint32_t x = (m_mt[i] & MASK_UPPER) + (m_mt[(i + 1) % N] & MASK_LOWER);

	uint32_t xA = x >> 1;

	if (x & 1)
	{
		xA ^= A;
	}

	m_mt[i] = m_mt[(i + M) % N] ^ xA;
}

void MersenneTwister::twist()
{
	for (uint32_t i = 0; i < N; i++)
	{
		twistIteration(i);
	}
	m_index = 0;
}

MersenneTwister::MersenneTwister()
{
}

MersenneTwister::MersenneTwister(uint32_t seed)
{
	setSeed(seed);
}

void MersenneTwister::setSeed(uint32_t seed)
{
	m_seed = seed;
	m_mt[0] = seed;

	for (uint32_t i = 1; i < N; i++)
	{
		m_mt[i] = (F * (m_mt[i - 1] ^ (m_mt[i - 1] >> 30)) + i);
	}
	m_index = N;

	//Warmup the MersenneTwister.
	for (uint32_t i = 0; i < 1000 * 10; i++)
	{
		nextU32();
	}
}

double MersenneTwister::nextDouble()
{
	while (true)
	{
		double retVal = (double)nextU32() / (double)0xFFFFFFFF;
		if (retVal != 1.0)
		{
			return retVal;
		}
	}
}

uint32_t MersenneTwister::nextU32(uint32_t min, uint32_t max)
{
	const uint32_t range = max - min + 1;
	return (uint32_t)(nextDouble() * range) + min;
}

double MersenneTwister::nextDouble(double min, double max)
{
	const double range = max - min;
	return nextDouble() * range + min;
}

double MersenneTwister::nextNormal(double mean, double sigma)
{
	double v1, sx;
	do {
		v1 = 2 * nextDouble() - 1;
		double v2 = 2 * nextDouble() - 1;
		sx = v1 * v1 + v2 * v2;
	} while (sx >= 1);
	
	double fx = std::sqrt(-2.0 * std::log(sx) / sx);

	return (fx * v1 * sigma + mean);
}

uint32_t MersenneTwister::nextU32()
{
	if (m_seed == 0)
	{
		SIMEXCEPTION(IllegalStateException);
	}
	if (m_index >= N)
	{
		twist();
	}

	uint32_t x = m_mt[m_index];
	m_index++;

	x ^= (x >> U);
	x ^= (x << S) & B;
	x ^= (x << T) & C;
	x ^= (x >> L);

	return x;
}
