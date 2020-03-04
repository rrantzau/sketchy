#ifndef CMS_H
#define CMS_H

#include "cms.capnp.h"
#include <capnp/message.h>
#include <capnp/schema.h>
#include <capnp/serialize.h>
#include <chrono> // time_t
#include <iostream>
#include <string>
#include <vector>

using namespace std;

class CountMinSketch
{
public:
  CountMinSketch() : CountMinSketch(UsageType::TOPK, 0.001, 0.03, 0, 5, 0, time(0)) {}
  CountMinSketch(UsageType usageType, double delta, double epsilon, int64_t point_V, int64_t topk_K, double heavy_F, time_t seed);
  CountMinSketch(const CountMinSketch &other, capnp::MallocMessageBuilder &newBuilder);
  void compare(CountMinSketch &other);

  void add(int64_t value, string payload);
  int64_t estimateFrequency(int64_t value);
  CMS::Builder getBuilder() { return builder; }
  CMS::Reader getReader() { return reader; }
  void setReader(CMS::Reader &r) { reader = r; }
  friend ostream &operator<<(ostream &os, CountMinSketch &c);
  void snapshot();
  void showComparison();

  chrono::high_resolution_clock::time_point getTimestamp() { return timestamp; }

private:
  capnp::MallocMessageBuilder messageBuilder;
  CMS::Builder builder = messageBuilder.initRoot<CMS>();
  CMS::Reader reader = messageBuilder.getRoot<CMS>();

  //kj::Array<capnp::word> serialized;

  chrono::high_resolution_clock::time_point timestamp;
  chrono::high_resolution_clock::time_point previousTimestamp;

  int64_t elapsed_nanoseconds;
  vector<double> currentRanks;
  vector<double> previousRanks;
  vector<double> deltaRanks;

  vector<int64_t> currentFrequencies;
  vector<int64_t> previousFrequencies;
  vector<int64_t> deltaFrequencies;

  int64_t estimatorHash(int64_t value, int64_t index);
  int64_t findInValuesV(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t keyValue, int64_t min, int64_t max);
  int64_t findInValuesVF(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t keyValue, int64_t key_frequency, int64_t min, int64_t max);
  int64_t findInFrequenciesFV(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder frequencies, int64_t key_frequency, int64_t keyValue, int64_t min, int64_t max);
  void sortTopkValues(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder top, int actualLength);
  void sortTopkFrequencies(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder top, int actualLength);
  void updateTopk(int64_t value, int64_t estimatedFrequency, string payload);
};

#endif