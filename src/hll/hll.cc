#include "hll.capnp.h"
#include "hll.h"
#include "util.h"
#include <capnp/schema.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <algorithm> // min
#include <assert.h>
#include <bitset>  // for binary representation of a number; for testing
#include <climits> // LLONG_MAX
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <random>
#include <set>
#include <string>
#include "libcount/include/count/hll.h"

using namespace std;

void initSketch(HLL::Builder sketch, int8_t b)
{
	sketch.setB(b);
	const auto mLength = 1 << b;
	double alpha;
	switch (mLength)
	{
	case 16:
		alpha = 0.673;
		break;
	case 32:
		alpha = 0.697;
		break;
	case 64:
		alpha = 0.709;
		break;
	default:
		alpha = 0.7213 / (1.0 + 1.079 / mLength);
	}
	const auto alphaMM = alpha * mLength * mLength;
	sketch.setMLength(mLength);
	sketch.setAlpha(alpha);
	sketch.setAlphaMM(alphaMM);

	sketch.initM(mLength);
	auto m = sketch.getM();
	for (auto i = 0; i < mLength; i++)
		m.set(i, 0);
	sketch.setRowsN(0);
}

// Source:
// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
uint64_t myHash(uint64_t value)
{
	auto h = value;
	h = (h ^ (h >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
	h = (h ^ (h >> 27)) * UINT64_C(0x94d049bb133111eb);
	h = h ^ (h >> 31);
	return h;
}

short rho(int64_t x, short b)
{
	cout << hex << x << endl;
	cout << "7777777766666666555555554444444433333333222222221111111100000000" << endl;
	cout << "7654321076543210765432107654321076543210765432107654321076543210" << endl;
	cout << bitset<64>(x) << endl;
	short v = 1;
	while (v <= b && !(x & 0x8000000000000000))
	{
		v++;
		x <<= 1;
		cout << "@@@001\t" << v << endl;
	}
	return v;
}

void add(HLL::Builder sketch, uint64_t value)
{
	const auto n = sketch.getRowsN();
	const auto b = sketch.getB();
	const auto h = myHash(value);
	const uint64_t index = h >> (64 - b);
	const uint64_t rank = rho((h << b), 64 - b);
	auto m = sketch.getM();
	cout << "hash:\t" << h << endl;
	cout << "index:\t" << index << endl;
	cout << "rank:\t" << rank << endl;
	cout << "m[index]:\t" << m[index] << " before" << endl;
	if (rank > m[index])
		m.set(index, rank);
	cout << "m[index]:\t" << m[index] << " after" << endl;
	sketch.setRowsN(n + 1);
}

void testOld()
{
	capnp::MallocMessageBuilder message;
	auto sketch = message.initRoot<HLL>();
	const auto b = 8;
	initSketch(sketch, b);
	add(sketch, 1);
}

void testLibcountOld()
{
	cout << "testLibcount:\tbegin" << endl;
	//auto precision = libcount::HLL_MIN_PRECISION;
	auto precision = libcount::HLL_MAX_PRECISION;
	libcount::HLL *hll = libcount::HLL::Create(precision);
	for (int batch = 0; batch < 3; batch++)
		for (uint64_t i = 0; i < 1000; i++)
			hll->Update(i);
	cout << "Estimate:\t" << hll->Estimate() << endl;
}

void testLibcount()
{
	//const int precision = 9;
	const auto precision = libcount::HLL_MAX_PRECISION;
	//const auto precision = libcount::HLL_MIN_PRECISION;

	// Create an HLL object to track set cardinality.
	libcount::HLL *hll = libcount::HLL::Create(precision);

	// Count 'iterations' elements with 'actualCardinality' cardinality.
	const uint64_t iterations = 1e6;
	const uint64_t actualCardinality = 1e5;
	for (uint64_t i = 0; i < iterations; ++i)
	{
		hll->Update(utility::hashInt(i % actualCardinality));
	}

	// Obtain the cardinality estimate.
	const uint64_t estimate = hll->Estimate();

	// Display results.
	cout << "actual cardinality:    " << actualCardinality << endl;
	cout << "estimated cardinality: " << estimate << endl;

	// Delete object.
	delete hll;
}

int main()
{
	testLibcount();
	//testLibcountOld();
}