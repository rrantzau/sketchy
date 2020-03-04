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
  CountMinSketch(UsageType usageType, double delta, double epsilon, int64_t point_v, int64_t topk_k, double heavy_f, time_t seed);
  CountMinSketch(const CountMinSketch &other, capnp::MallocMessageBuilder &new_builder);
  void compare(CountMinSketch &other);

  void add(int64_t value, string payload);
  int64_t estimate_frequency(int64_t value);
  CMS::Builder get_builder() { return builder; }
  CMS::Reader get_reader() { return reader; }
  void set_reader(CMS::Reader &r) { reader = r; }
  friend ostream &operator<<(ostream &os, CountMinSketch &c);
  void snapshot();
  void show_comparison();

  chrono::high_resolution_clock::time_point get_timestamp() { return timestamp; }

private:
  capnp::MallocMessageBuilder message_builder;
  CMS::Builder builder = message_builder.initRoot<CMS>();
  CMS::Reader reader = message_builder.getRoot<CMS>();

  //kj::Array<capnp::word> serialized;

  chrono::high_resolution_clock::time_point timestamp;
  chrono::high_resolution_clock::time_point previous_timestamp;

  int64_t elapsed_nanoseconds;
  vector<double> current_ranks;
  vector<double> previous_ranks;
  vector<double> delta_ranks;

  vector<int64_t> current_frequencies;
  vector<int64_t> previous_frequencies;
  vector<int64_t> delta_frequencies;

  int64_t estimator_hash(int64_t value, int64_t index);
  int64_t find_in_Values_v(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t key_value, int64_t min, int64_t max);
  int64_t find_in_Values_v_f(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder values, int64_t key_value, int64_t key_frequency, int64_t min, int64_t max);
  int64_t find_in_frequencies_f_v(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder frequencies, int64_t key_frequency, int64_t key_value, int64_t min, int64_t max);
  void sort_topk_values(capnp::List<TopkValue, capnp::Kind::STRUCT>::Builder top, int actual_length);
  void sort_topk_frequencies(capnp::List<TopkFrequency, capnp::Kind::STRUCT>::Builder top, int actual_length);
  void update_topk(int64_t value, int64_t estimated_frequency, string payload);
};

#endif