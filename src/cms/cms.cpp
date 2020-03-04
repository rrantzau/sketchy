#include "cms.capnp.h"
#include "cms.h"
#include <capnp/message.h>
#include <capnp/schema.h>
#include <algorithm> // min
#include <array>
#include <assert.h>
#include <climits> // LLONG_MAX
#include <fcntl.h> /// POSIX file control
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <math.h>
#include <pthread.h>
#include <random>
#include <set>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
using chrono::steady_clock;
using chrono::system_clock;

#define NUM_THREADS 6

pthread_mutex_t mtx;

/*
time_t steady_clock_to_time_t(steady_clock::time_point t)
{
	return system_clock::to_time_t(system_clock::now() + chrono::duration_cast<system_clock::duration>(t - steady_clock::now()));
	//return system_clock::to_time_t(system_clock::now() + (t - steady_clock::now()));
}
*/

CountMinSketch::CountMinSketch(UsageType usageType, double delta, double epsilon, int64_t point_V, int64_t topk_K, double heavy_F, time_t seed)
{
	timestamp = chrono::high_resolution_clock::now();

	builder.setDelta(delta);
	builder.setEpsilon(epsilon);
	builder.setPointV(point_V);
	builder.setTopkK(topk_K);
	builder.setHeavyF(heavy_F);
	builder.setD((int64_t)ceil(log(1.0 / delta)));
	builder.setW((int64_t)ceil(exp(1.0) / epsilon));
	builder.setPrime(pow(2, 31) - 1); // 2^31 - 1
	builder.setRowsN(0);
	builder.setTopkN(0);
	builder.setUsageType(usageType);

	const auto d = builder.getD();
	const auto w = builder.getW();
	const auto p = builder.getPrime();

	// Initialize hashes
	auto a = builder.initA(d);
	auto b = builder.initB(d);

	srand(seed);
	for (long unsigned int i = 0; i < d; i++)
	{
		a.set(i, rand() % p); // Random in range [1..P]
		b.set(i, rand() % p); // Random in range [1..P]
	}
	auto estimators = builder.initEstimators(d * w);
	for (long unsigned int i = 0; i < d; i++)
		for (long unsigned int j = 0; j < w; j++)
			estimators.set(i * w + j, 0);

	auto topkFrequencies = builder.initTopkFrequencies(topk_K);
	auto topkValues = builder.initTopkValues(topk_K);
	auto topkFrequenciesPrevious = builder.initTopkFrequenciesPrevious(topk_K);
	auto topkValuesPrevious = builder.initTopkValuesPrevious(topk_K);
	for (capnp::uint i = 0; i < topk_K; i++)
	{
		topkValues[i].setRank(topk_K - i);
		topkValues[i].setFrequency(0);
		topkValues[i].setValue(0);
		topkValues[i].setPayload("");
		topkFrequencies[i].setRank(topk_K - i);
		topkFrequencies[i].setFrequency(0);
		topkFrequencies[i].setValue(0);
		topkFrequencies[i].setPayload("");
		topkValuesPrevious[i].setRank(topk_K - i);
		topkValuesPrevious[i].setFrequency(0);
		topkValuesPrevious[i].setValue(0);
		topkValuesPrevious[i].setPayload("");
		topkFrequenciesPrevious[i].setRank(topk_K - i);
		topkFrequenciesPrevious[i].setFrequency(0);
		topkFrequenciesPrevious[i].setValue(0);
		topkFrequenciesPrevious[i].setPayload("");
	}

	for (int i = 0; i < topk_K; i++)
	{
		deltaRanks.push_back(numeric_limits<double>::quiet_NaN());
		deltaFrequencies.push_back(0);
	}
}

// Creates a READER copy of the sketch, i.e., we're not allowed to manipulate this copied object.
CountMinSketch::CountMinSketch(const CountMinSketch &other, capnp::MallocMessageBuilder &newBuilder)
{
	timestamp = other.timestamp;
	newBuilder.setRoot(other.reader);
	reader = newBuilder.getRoot<CMS>();
}

ostream &operator<<(ostream &os, CountMinSketch &c)
{
	const auto d = c.reader.getD();
	const auto w = c.reader.getW();
	//auto itt = steady_clock_to_time_t(c.timestamp);
	//auto itt = chrono::system_clock::to_time_t(c.timestamp);
	//auto itt = chrono::system_clock::to_time_t(chrono::time_point_cast<chrono::system_clock::duration>(c.timestamp));
	//const auto time_string = put_time(localtime(&itt), "%FT%TZ");
	const chrono::high_resolution_clock::time_point null_timestamp;
	const auto elapsed_nanoseconds = chrono::duration_cast<chrono::nanoseconds>(c.timestamp - null_timestamp).count();

	//os << left << setw(20) << "Timestamp:" << setw(20) << time_string << setw(0) << " (" << elapsed_nanoseconds << "ns)" << setw(20) << endl;
	os << left << setw(20) << "Timestamp:" << elapsed_nanoseconds << "ns" << setw(20) << endl;
	os << left << setw(20) << "Name:" << string(c.reader.getName().cStr()) << endl;
	os << left << setw(20) << "delta:" << setprecision(10) << c.reader.getDelta() << endl;
	os << left << setw(20) << "epsilon:" << setprecision(10) << c.reader.getEpsilon() << endl;
	os << left << setw(20) << "d:" << d << endl;
	os << left << setw(20) << "w:" << w << endl;
	// os << "Point v:" << c.reader.getPointV() << endl;
	// os << "heavyf:" << c.reader.getHeavyF() << endl;
	//os << "Prime:" << c.reader.getPrime() << endl;
	os << left << setw(20) << "Top-k k:" << c.reader.getTopkK() << endl;
	os << left << setw(20) << "Top-k n:" << c.reader.getTopkN() << endl;
	os << left << setw(20) << "Rows:" << c.reader.getRowsN() << endl;

	os << left << setw(20) << "a:";

	auto a = c.reader.getA();
	for (long unsigned int i = 0; i < d; i++)
		os << a[i] << "\t";
	os << endl;

	os << left << setw(20) << "b:";
	auto b = c.reader.getB();
	for (long unsigned int i = 0; i < d; i++)
		os << b[i] << "\t";
	os << endl;

	os << "estimators:\n";
	auto estimators = c.reader.getEstimators();
	for (long unsigned int i = 0; i < d; i++)
	{
		for (long unsigned int j = 0; j < w; j++)
			os << estimators[i * w + j] << "\t";
		os << endl
		   << endl;
	}

	// TODO: Not sure about the following:
	// Make sure the top-k arrays are sorted before we showComparison them to anyone.
	//c.sortTopkValues(c.builder.getTopkValues(), c.builder.getTopkN());
	//c.sortTopkFrequencies(c.builder.getTopkFrequencies(), c.builder.getTopkN());

	os << "TOP-K FREQUENCIES:" << endl;
	os << right << setw(20) << "k:" << right << setw(20) << "value:" << setw(20) << "frequency:"
	   << "   " << left << setw(20) << "payload:" << endl;
	auto topkFrequencies = c.reader.getTopkFrequencies();
	int i;
	for (i = topkFrequencies.size() - 1; i >= 0; i--)
	{
		auto item = topkFrequencies[i];
		os << right << setw(20) << item.getRank() << right << setw(20) << item.getValue() << setw(20) << item.getFrequency() << "   " << left << setw(20) << item.getPayload().cStr() << endl;
	}

	// os  << "top-k values (k, value, frequency, payload):" << endl;
	// auto topkValues = c.reader.getTopkValues();
	// for (i = topkValues.size() - 1; i >= 0; i--)
	// {
	//     auto item = topkValues[i];
	//     os << right << setw(15) << item.getRank() << right << setw(12) << item.getValue() << setw(12) << item.getFrequency() << setw(12) << item.getPayload().cStr() << endl;
	// }
	return os;
}

struct Item
{
	int64_t value;
	int64_t frequency;
	string payload;
};

int64_t CountMinSketch::estimatorHash(int64_t value, int64_t index)
{
	// Make sure that we do NOT return a NEGATIVE number.  That's why we make the computations unsigned
	auto a = (uint64_t)builder.getA()[index];
	auto b = (uint64_t)builder.getB()[index];
	auto p = (uint64_t)builder.getPrime();
	auto w = (uint64_t)builder.getW();

	return ((a * value + b) % p) % w;
}

int64_t CountMinSketch::findInValuesV(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t keyValue, int64_t min, int64_t max)
{
	while (min <= max)
	{
		int64_t middle = min + ((max - min) / 2);
		if (values[middle].getValue() == keyValue)
			return middle;
		else if (values[middle].getValue() < keyValue)
			min = middle + 1;
		else
			max = middle - 1;
	}

	// Key not found
	return -1;
}

int64_t CountMinSketch::findInValuesVF(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t keyValue, int64_t keyFrequency, int64_t min, int64_t max)
{
	while (min <= max)
	{
		int64_t middle = min + ((max - min) / 2);
		if (values[middle].getValue() == keyValue && values[middle].getFrequency() == keyFrequency)
			return middle;
		else if ((values[middle].getValue() < keyValue) ||
				 (values[middle].getValue() == keyValue && values[middle].getFrequency() < keyFrequency))
			min = middle + 1;
		else
			max = middle - 1;
	}

	// Key not found
	for (auto v : values)
	{
		cout << "### " << v.getRank() << "\t" << v.getValue() << "\t" << v.getFrequency() << endl;
	}
	return -1;
}

int64_t CountMinSketch::findInFrequenciesFV(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder frequencies, int64_t keyFrequency, int64_t keyValue, int64_t min, int64_t max)
{
	while (min <= max)
	{
		int64_t middle = min + ((max - min) / 2);
		if (frequencies[middle].getFrequency() == keyFrequency && frequencies[middle].getValue() == keyValue)
			return middle;
		else if ((frequencies[middle].getFrequency() < keyFrequency) ||
				 (frequencies[middle].getFrequency() == keyFrequency && frequencies[middle].getValue() < keyValue))
			min = middle + 1;
		else
			max = middle - 1;
	}

	// Key not found
	return -1;
}

void CountMinSketch::updateTopk(int64_t value, int64_t estimatedFrequency, string payload)
{
	const auto k = builder.getTopkK();
	const auto n = builder.getTopkN();
	const auto usageType = builder.getUsageType();
	const int64_t i_v = findInValuesV(builder.getTopkValues(), value, 0, n - 1);
	if (i_v != -1) // Value found
	{
		// Update with new frequency information
		if (usageType == UsageType::TOPK || usageType == UsageType::HEAVY)
		{
			const auto frequencyOld = builder.getTopkValues()[i_v].getFrequency();

			// Find the entry in the other array
			const auto i_f = findInFrequenciesFV(builder.getTopkFrequencies(), frequencyOld, value, 0, n - 1);

			assert(i_f != -1);

			builder.getTopkFrequencies()[i_f].setFrequency(estimatedFrequency);
			builder.getTopkValues()[i_v].setFrequency(estimatedFrequency);
		}
	}
	else // Value not found
	{
		if (usageType == UsageType::TOPK)
		{
			if (n < k)
			{
				// Append the new element to the current end of both arrays
				builder.getTopkValues()[n].setValue(value);
				builder.getTopkValues()[n].setFrequency(estimatedFrequency);
				builder.getTopkValues()[n].setPayload(payload);
				builder.getTopkFrequencies()[n].setValue(value);
				builder.getTopkFrequencies()[n].setFrequency(estimatedFrequency);
				builder.getTopkFrequencies()[n].setPayload(payload);

				// Maintain the volume of the data structures
				builder.setTopkN(n + 1);
			}
			// If the lowest frequent top-k element has a frequency that is lower than the new value, replace it in both arrays.
			// The arrays are in ascending order, so the smallest frequency is "on the left", i.e, at index 0.
			else if (builder.getTopkFrequencies()[0].getFrequency() < estimatedFrequency)
			{
				const auto f = builder.getTopkFrequencies()[0].getFrequency();
				const auto v = builder.getTopkFrequencies()[0].getValue();

				const auto i_v = findInValuesVF(builder.getTopkValues(), v, f, 0, n - 1);
				// TODO:  I've seen it happen that this assertion was violated (with a Zipf dataset).  How was this possible?  More testing needed!
				assert(i_v != -1);

				builder.getTopkValues()[i_v].setValue(value);
				builder.getTopkValues()[i_v].setFrequency(estimatedFrequency);
				builder.getTopkValues()[i_v].setPayload(payload);
				builder.getTopkFrequencies()[0].setValue(value);
				builder.getTopkFrequencies()[0].setFrequency(estimatedFrequency);
				builder.getTopkFrequencies()[0].setPayload(payload);
			}
			else
			{
				// Ignore the new element, it is not frequent enough.  No need to re-sort.
				return;
			}
		}
		else if (usageType == UsageType::HEAVY)
		{
			// If value v is not found,
			//   if estimated frequency f of v is >= N * F
			//     then add (v,f) to heap.
			//  Heap should be periodically or continuously cleaned up to remove elements that do not meet the threshold anymore.
			//    At least before we return the result in the final() function, we must go through the heap and remove all
			//    elements that are not frequent enough.
			// 	if (estimated_frequency >= state.rows_n * state.heavy_F)
			if (estimatedFrequency >= n * builder.getHeavyF())
			{
				// Create an array that has 1 element more space than the existing one.
				auto oldFrequencies = builder.getTopkFrequencies();
				auto oldValues = builder.getTopkValues();
				auto topkFrequencies = builder.initTopkFrequencies(n + 1);
				auto topkValues = builder.initTopkValues(n + 1);
				uint64_t i;
				for (i = 0; i < n; i++)
				{
					topkFrequencies[i].setValue(oldFrequencies[i].getValue());
					topkFrequencies[i].setFrequency(oldFrequencies[i].getFrequency());
					topkFrequencies[i].setPayload(oldFrequencies[i].getPayload());
					topkValues[i].setValue(oldValues[i].getValue());
					topkValues[i].setFrequency(oldValues[i].getFrequency());
					topkValues[i].setPayload(oldValues[i].getPayload());
				}

				// Append new value at the end
				topkFrequencies[i].setValue(value);
				topkFrequencies[i].setFrequency(estimatedFrequency);
				topkFrequencies[i].setPayload(payload);
				topkValues[i].setValue(value);
				topkValues[i].setFrequency(estimatedFrequency);
				topkValues[i].setPayload(payload);

				builder.setTopkN(n + 1);

				// WRONG:  It may be enough to weed out any infrequent values during finalize().
				// WE HAVE TO CLEAN UP NOW, otherwise the frequencies sort order may be disturbed
				// TODO:  Not done yet:
				// 		heavy_cleanup_heap(state);
			}
		}
	}

	// Adjust the top-k arrays
	if (usageType == UsageType::TOPK || usageType == UsageType::HEAVY)
	{
		sortTopkValues(builder.getTopkValues(), builder.getTopkN());
		sortTopkFrequencies(builder.getTopkFrequencies(), builder.getTopkN());
	}
}

int64_t CountMinSketch::estimateFrequency(int64_t value)
{
	int64_t minimum = LLONG_MAX;
	const auto d = (int64_t)builder.getD();
	const auto w = builder.getW();
	for (int64_t i = 0; i < d; i++)
	{
		auto h = estimatorHash(value, i);
		auto index = i * w + h;
		minimum = min(minimum, (int64_t)builder.getEstimators()[index]);
	}

	// If all cells have not been touched yet, then it must be 0.
	return (minimum == LLONG_MAX) ? 0 : minimum;
}

void CountMinSketch::add(int64_t value, string payload)
{
	builder.setRowsN(builder.getRowsN() + 1); // Increment #rows
	const auto d = builder.getD();
	const auto w = builder.getW();
	auto estimators = builder.getEstimators();
	for (uint64_t i = 0; i < d; i++)
	{
		auto hash = estimatorHash(value, i);
		auto index = w * i + hash;
		const auto prevEstimate = estimators[index];
		estimators.set(index, prevEstimate + 1);
	}

	const auto usageType = builder.getUsageType();
	if (usageType == UsageType::TOPK || usageType == UsageType::HEAVY)
	{
		updateTopk(value, estimateFrequency(value), payload);
	}
}

void readRandom(CountMinSketch &sketch)
{
	for (int i = 0; i < 1e6; i++)
	{
		const auto value = rand();
		auto payload = "aa";
		sketch.add(value, payload);
	}
}

void readData(CountMinSketch &sketch)
{
	ifstream in;
	in.open("data.csv");

	uint64_t counter, zipf, uniform, normal;
	while (in >> counter >> zipf >> uniform >> normal)
	{
		sketch.add(zipf, "zipf" + to_string(zipf));
	}
	in.close();
}

void readBook(CountMinSketch &sketch)
{
	ifstream in;
	//in.open("../data/holmes.txt");
	in.open("../data/shakespeare.txt");
	if (!in.is_open())
		return;

	string word;
	int count;
	size_t h;
	for (count = 0; in >> word; count++)
	{
		//if (word.length() >= 4)
		{
			h = hash<string>{}(word);
			sketch.add(h, word);
		}
	}
	cout << "#words:\t" << count << endl;
	cout << "hash:\t" << h << endl;
	in.close();
}

void CountMinSketch::sortTopkValues(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder top, int actualLength)
{
	// Copy original Capnp list to a temporary STL-type list.
	list<Item> tmp;
	auto iter = top.begin();
	for (int i = 0; i < actualLength; i++, iter++)
	{
		Item item;
		item.frequency = top[i].getFrequency();
		item.value = top[i].getValue();
		item.payload = top[i].getPayload().cStr();
		tmp.push_back(item);
	}

	// Sort the temporary list.
	typedef function<bool(Item, Item)> Comparator;
	Comparator compFunctor =
		[](Item item1, Item item2) {
			return item1.value < item2.value || (item1.value == item2.value && item1.frequency < item2.frequency);
		};
	tmp.sort(compFunctor);

	// Copy the now sorted temporary list to the original Capnp list.
	int i = 0;
	for (auto item : tmp)
	{
		top[i].setFrequency(item.frequency);
		top[i].setValue(item.value);
		top[i].setPayload(item.payload);
		i++;
	}
}

void CountMinSketch::sortTopkFrequencies(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder top, int actualLength)
{
	// Copy original Capnp list to a temporary STL-type list.
	list<Item> tmp;
	auto iter = top.begin();
	for (int i = 0; i < actualLength; i++, iter++)
	{
		Item item;
		item.frequency = top[i].getFrequency();
		item.value = top[i].getValue();
		item.payload = top[i].getPayload().cStr();
		tmp.push_back(item);
	}

	// Sort the temporary list.
	typedef function<bool(Item, Item)> Comparator;
	Comparator compFunctor =
		[](Item item1, Item item2) {
			return item1.frequency < item2.frequency || (item1.frequency == item2.frequency && item1.value < item2.value);
		};
	tmp.sort(compFunctor);

	// Copy the now sorted temporary list to the original Capnp list.
	int i = 0;
	for (auto item : tmp)
	{
		top[i].setFrequency(item.frequency);
		top[i].setValue(item.value);
		top[i].setPayload(item.payload);
		i++;
	}
}

void CountMinSketch::showComparison()
{
	const auto n = reader.getTopkN();
	const auto dt = ((double)elapsed_nanoseconds) / 1e9;
	cout << "VELOCITY OF TOP-K ITEMS:" << endl;
	cout << right << setw(20) << "k:" << right << setw(20) << "dr = diff(rank):" << setw(20) << "df = diff(freq):" << setw(20) << "df/dt = velocity" << endl;
	for (int64_t i = n - 1; i >= 0; i--)
	{
		const auto dr = deltaRanks[i];
		const auto df = deltaFrequencies[i];
		const auto df_dt = ((double)df) / dt;
		cout << right << setw(20) << n - i << right << setw(20) << setprecision(0) << dr << setw(20) << setprecision(5) << df << setw(20) << fixed << setprecision(1) << df_dt << endl;
	}
}

void CountMinSketch::compare(CountMinSketch &other)
{
	previousTimestamp = timestamp;
	timestamp = chrono::high_resolution_clock::now();
	elapsed_nanoseconds = chrono::duration_cast<chrono::nanoseconds>(timestamp - other.timestamp).count();

	//elapsed_nanoseconds = chrono::duration_cast<chrono::nanoseconds>(timestamp - other.timestamp).count();

	//
	// Compute the changes in ranks for the current top-k items.
	//
	auto currentFrequencies = builder.getTopkFrequencies();
	auto previousFrequencies = other.builder.getTopkFrequencies();
	int i = 0;
	for (auto curr : currentFrequencies)
	{
		const auto currValue = curr.getValue();
		for (auto prev : previousFrequencies)
		{
			const auto prevValue = prev.getValue();
			if (currValue == prevValue)
			{
				// We found the value.  If we didn't the value will remain NaN.
				deltaRanks[i] = curr.getRank() - prev.getRank();
			}
		}
		i++;
	}

	i = 0;
	for (auto curr : currentFrequencies)
	{
		//const auto curr = curr.getFrequency(); //curr.getValue();
		const auto currFrequency = curr.getFrequency();
		const auto currValue = curr.getValue();

		// TODO:  Check if the value existed in the previous top-k list.  If yes, use this actual value, otherwise use an estimate.
		int64_t prevFrequency = -1;
		for (auto prev : previousFrequencies)
		{
			const auto prevValue = prev.getValue();
			if (currValue == prevValue)
			{
				// We found the value.  If we didn't the value will remain NaN.
				prevFrequency = prev.getFrequency();
			}
		}
		if (prevFrequency == -1) // Not found in the previous top-k list.
			prevFrequency = estimateFrequency(curr.getValue());

		deltaFrequencies[i] = currFrequency - prevFrequency;
		i++;
	}

	// //
	// // Memorize the previous state
	// //
	// currentFrequencies = builder.getTopkFrequencies();
	// previousFrequencies = other.builder.getTopkFrequencies();
	// cout << 2001 << endl;
	// cout << currentFrequencies.size() << endl;
	// cout << previousFrequencies.size() << endl;
	// cout.flush();
	// i = 0;
	// for (auto curr : currentFrequencies)
	// {
	//     previousFrequencies[i].setFrequency(curr.getFrequency());
	//     previousFrequencies[i].setRank(curr.getRank());
	//     previousFrequencies[i].setValue(curr.getValue());
	//     previousFrequencies[i].setPayload(curr.getPayload());
	//     i++;
	// }
	// auto currentValues = builder.getTopkValues();
	// auto previousValues = other.builder.getTopkValues();
	// i = 0;
	// for (auto curr : currentValues)
	// {
	//     previousValues[i].setFrequency(curr.getFrequency());
	//     previousValues[i].setRank(curr.getRank());
	//     previousValues[i].setValue(curr.getValue());
	//     previousValues[i].setPayload(curr.getPayload());
	//     i++;
	// }
}

void CountMinSketch::snapshot()
{
	previousTimestamp = timestamp;
	timestamp = chrono::high_resolution_clock::now();
	elapsed_nanoseconds = chrono::duration_cast<chrono::nanoseconds>(timestamp - previousTimestamp).count();

	//
	// Compute the changes in ranks for the current top-k items.
	//
	auto currentFrequencies = builder.getTopkFrequencies();
	auto previousFrequencies = builder.getTopkFrequenciesPrevious();
	int i = 0;
	for (auto curr : currentFrequencies)
	{
		const auto currValue = curr.getValue();
		for (auto prev : previousFrequencies)
		{
			const auto prevValue = prev.getValue();
			if (currValue == prevValue)
			{
				// We found the value.  If we didn't the value will remain NaN.
				deltaRanks[i] = curr.getRank() - prev.getRank();
			}
		}
		i++;
	}

	i = 0;
	for (auto curr : currentFrequencies)
	{
		//const auto curr = curr.getFrequency(); //curr.getValue();
		const auto currFrequency = curr.getFrequency();
		const auto currValue = curr.getValue();

		// TODO:  Check if the value existed in the previous top-k list.  If yes, use this actual value, otherwise use an estimate.
		int64_t prevFrequency = -1;
		for (auto prev : previousFrequencies)
		{
			const auto prevValue = prev.getValue();
			if (currValue == prevValue)
			{
				// We found the value.  If we didn't the value will remain NaN.
				prevFrequency = prev.getFrequency();
			}
		}
		if (prevFrequency == -1) // Not found in the previous top-k list.
			prevFrequency = estimateFrequency(curr.getValue());

		deltaFrequencies[i] = currFrequency - prevFrequency;
		i++;
	}

	//
	// Memorize the previous state
	//
	currentFrequencies = builder.getTopkFrequencies();
	previousFrequencies = builder.getTopkFrequenciesPrevious();
	i = 0;
	for (auto curr : currentFrequencies)
	{
		previousFrequencies[i].setFrequency(curr.getFrequency());
		previousFrequencies[i].setRank(curr.getRank());
		previousFrequencies[i].setValue(curr.getValue());
		previousFrequencies[i].setPayload(curr.getPayload());
		i++;
	}
	auto currentValues = builder.getTopkValues();
	auto previousValues = builder.getTopkValuesPrevious();
	i = 0;
	for (auto curr : currentValues)
	{
		previousValues[i].setFrequency(curr.getFrequency());
		previousValues[i].setRank(curr.getRank());
		previousValues[i].setValue(curr.getValue());
		previousValues[i].setPayload(curr.getPayload());
		i++;
	}
}

void test_time()
{
	auto start = chrono::high_resolution_clock::now();

	cout << "Elapsed time:" << endl;

	auto finish = std::chrono::high_resolution_clock::now();
	auto elapsed_nanoseconds = chrono::duration_cast<chrono::nanoseconds>(finish - start).count();
	cout << "\t" << elapsed_nanoseconds << "ns" << endl;

	auto t1 = chrono::system_clock::now();
	auto t2 = chrono::system_clock::now();
	chrono::duration<double> elapsed_seconds = t2 - t1;
	cout << "\t" << elapsed_seconds.count() << "s" << endl;

	cout << "Finished computation on:" << endl;
	time_t end_time = chrono::system_clock::to_time_t(t2);
	ostringstream ss;
	ss << "\t" << ctime(&end_time);
	auto now = chrono::system_clock::now();
	auto itt = chrono::system_clock::to_time_t(now);
	ss << "\t" << put_time(gmtime(&itt), "%FT%TZ (ISO8601 UTC time)") << endl;
	ss << "\t" << put_time(localtime(&itt), "%FT%TZ (local time)") << endl;
	cout << ss.str();
}

typedef struct
{
	CountMinSketch *sketch;
} ThreadData;

void *producer(void *arg)
{
	ThreadData *data = (ThreadData *)arg;
	//const unsigned int sleepSeconds = 0.001 * 1e6; // Microseconds
	const unsigned int sleepSeconds = 0.000 * 1e6; // Microseconds
	for (int i = 0; i < 10; i++)
	// while (true)
	{
		// Read the same file again and again.
		ifstream in;
		in.open("data.csv");
		uint64_t counter, zipf, uniform, normal;
		while (in >> counter >> zipf >> uniform >> normal)
		{
			pthread_mutex_lock(&mtx);
			data->sketch->add(zipf, "zipf" + to_string(zipf));
			pthread_mutex_unlock(&mtx);

			usleep(sleepSeconds);
		}
		in.close();
	}
	pthread_exit((void *)arg);
}

/*
void *consumer(void *arg)
{
    ThreadData *data = (ThreadData *)arg;
    const unsigned int sleepSeconds = 2.0 * 1e6; // Microseconds

    while (true)
    {
        pthread_mutex_lock(&lock);
        //data->sketch->snapshot(); // Update the timestamp of the sketch to "now".
        capnp::MallocMessageBuilder newBuilder;
        CountMinSketch previous(*data->sketch, newBuilder);
        cout << "1001" << endl;
        cout.flush();
        pthread_mutex_unlock(&lock);

        usleep(sleepSeconds);

        cout << "\033c"; // HACK: Non-portable way to clear the terminal.  Sorry!
        pthread_mutex_lock(&lock);
        cout << *data->sketch;
        data->sketch->compare(previous);
        data->sketch->showComparison();
        cout.flush();
        pthread_mutex_unlock(&lock);
    }

    pthread_exit((void *)arg);
}
*/

void *consumer(void *arg)
{
	ThreadData *data = (ThreadData *)arg;
	const unsigned int sleepSeconds = 2.0 * 1e6; // Microseconds

	while (true)
	{
		usleep(sleepSeconds);
		//pthread_mutex_lock(&lock);
		//cout << "\033c"; // HACK: Non-portable way to clear the terminal.  Sorry!
		pthread_mutex_lock(&mtx);
		cout << *data->sketch;
		data->sketch->showComparison();
		cout.flush();
		data->sketch->snapshot(); // Update the timestamp of the sketch to "now".
		pthread_mutex_unlock(&mtx);
	}

	pthread_exit((void *)arg);
}

// void *consumer(void *arg)
// {
//     ThreadData *data = (ThreadData *)arg;
//     const unsigned int sleepSeconds = 2.0 * 1e6; // Microseconds

//     while (true)
//     {
//         pthread_mutex_lock(&lock);
//         data->sketch->snapshot(); // Update the timestamp of the sketch to "now".
//         //capnp::MallocMessageBuilder newBuilder;
//         //CountMinSketch previous(*data->sketch, newBuilder);
//         //data->sketch->compare(previous);
//         pthread_mutex_unlock(&lock);

//         usleep(sleepSeconds);

//         cout << "\033c"; // HACK: Non-portable way to clear the terminal.  Sorry!
//         pthread_mutex_lock(&lock);
//         cout << *data->sketch;
//         data->sketch->showComparison();
//         cout.flush();
//         pthread_mutex_unlock(&lock);
//     }

//     pthread_exit((void *)arg);
// }

void test_threads()
{
	const auto usageType = UsageType::TOPK;
	const auto delta = 0.001;
	const auto epsilon = 0.03;
	const auto point_V = 0;
	const auto topk_K = 20;
	const auto heavy_F = 0;
	const auto seed = time(0); // 1549994987;

	CountMinSketch sketch(usageType, delta, epsilon, point_V, topk_K, heavy_F, seed);
	cout << sketch;

	/*
    {
        readData(sketch);
        capnp::MallocMessageBuilder newBuilder;
        CountMinSketch other(sketch, newBuilder);
        cout << other << endl;
        cout << "0---------------------------------------------------------------------------------------------------" << endl;
        readData(sketch);
        cout << sketch << endl;
        cout << "1---------------------------------------------------------------------------------------------------" << endl;
        cout << other << endl;
        cout << "Bye!" << endl;
        exit(0);
    }
    */

	// Initialize and set thread detached attribute
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	if (pthread_mutex_init(&mtx, NULL) != 0)
	{
		cerr << "ERROR; mutex initilaization failed" << endl;
		exit(-1);
	}

	// Populate the sketch with date read from a file
	ThreadData *data = (ThreadData *)malloc(sizeof(ThreadData));
	data->sketch = &sketch;
	pthread_t producerThreads[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++)
	{
		int rc = pthread_create(&producerThreads[i], &attr, producer, (void *)data);
		if (rc)
		{
			cerr << "ERROR; return code from pthread_create() is " << rc << endl;
			exit(-1);
		}
	}

	// Every few seconds, show the current sketch with the top-k frequency info.
	pthread_t consumerThread;
	int rc = pthread_create(&consumerThread, &attr, consumer, (void *)data);
	if (rc)
	{
		cerr << "ERROR; return code from pthread_create() is " << rc << endl;
		exit(-1);
	}

	// Free attribute and wait for the other threads
	pthread_attr_destroy(&attr);

	for (int i = 0; i < NUM_THREADS; i++)
	{
		rc = pthread_join(producerThreads[i], NULL);
		if (rc)
		{
			cerr << "ERROR; return code from pthread_join() is " << rc << endl;
			exit(-1);
		}
	}

	rc = pthread_cancel(consumerThread);
	//rc = pthread_join(consumerThread, NULL);
	if (rc)
	{
		cerr << "ERROR; return code from pthread_cancel() is " << rc << endl;
		exit(-1);
	}

	pthread_mutex_destroy(&mtx);
	pthread_exit(NULL);
	cout << "Good-bye!" << endl;
}

int main()
{
	test_threads();
}
