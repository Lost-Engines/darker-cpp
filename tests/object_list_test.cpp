#include <catch2/catch_test_macros.hpp>
#include <array>
#include "game/object_list.h"
#include "reference/world_samples.h"

namespace {

struct record {
  record *next{nullptr};
  record *previous{nullptr};
  int payload{0};
};

} // namespace

TEST_CASE("Object allocation, tail reuse, unlinking and recycling match native link mutations") {
  for(auto const &sample : darker::test_reference::object_list_samples) {
    CAPTURE(sample.active, sample.operation, sample.target);
    std::array<record, 6> records;
    auto const pointer{[&](int const index)->record *{ return index == 0 ? nullptr : &records[static_cast<std::size_t>(index - 1)]; }};
    auto const index{[&](record const *value){ return value ? static_cast<int>(value - records.data()) + 1 : 0; }};
    for(int i{1}; i <= 6; ++i) {
      *pointer(i) = {
        .next{pointer(i < (i <= sample.active ? sample.active : 6) ? i + 1 : 0)},
        .previous{pointer(i <= sample.active ? i - 1 : i)},
        .payload{i * 17}
      };
    }
    darker::game::object_list<record> list{
      .head{pointer(sample.active ? 1 : 0)},
      .tail{pointer(sample.active)},
      .free{pointer(sample.active < 6 ? sample.active + 1 : 0)},
    };
    record *result{nullptr};
    switch(sample.operation) {
      case 0: result = darker::game::allocate_object(list); break;
      case 1: result = darker::game::allocate_or_reuse_object(list); break;
      case 2: result = darker::game::unlink_object(list, *pointer(sample.target)); break;
      case 3: result = darker::game::recycle_object(list, *pointer(sample.target)); break;
    }
    std::array<int, 16> actual{index(result), index(list.head), index(list.tail), index(list.free)};
    for(std::size_t i{0}; i < records.size(); ++i) {
      actual[4 + i * 2] = index(records[i].next);
      actual[5 + i * 2] = index(records[i].previous);
      CHECK(records[i].payload == static_cast<int>((i + 1) * 17));
    }
    CHECK(actual == sample.result);
  }
}
