#pragma once

#include <stdexcept>

namespace darker::game {

// Records provide Record *next and Record *previous and keep a stable address.
// Free records use only next; previous and payload are deliberately retained.
template<typename Record>
struct object_list {
  Record *head{nullptr};
  Record *tail{nullptr};
  Record *free{nullptr};
};

template<typename Record>
Record *allocate_object(object_list<Record> &list) {
  /// 1C95 takes the free-list head and prepends it without clearing payload
  auto *record{list.free};
  if(!record) return nullptr;
  list.free = record->next;
  record->next = list.head;
  record->previous = nullptr;
  if(list.head) list.head->previous = record;
  else list.tail = record;
  list.head = record;
  return record;
}

template<typename Record>
Record *unlink_object(object_list<Record> &list, Record &record) {
  /// 7AB4 repairs active neighbours and returns the next record without recycling
  auto *next{record.next};
  if(record.previous) record.previous->next = next;
  else list.head = next;
  if(next) next->previous = record.previous;
  else list.tail = record.previous;
  return next;
}

template<typename Record>
Record *recycle_object(object_list<Record> &list, Record &record) {
  /// 1CD4 unlinks the active record and prepends it to the free list
  auto *next{unlink_object(list, record)};
  record.next = list.free;
  list.free = &record;
  return next;
}

template<typename Record>
Record *allocate_or_reuse_object(object_list<Record> &list) {
  /// 1CBF falls back to moving the oldest active record to the head
  if(auto *record{allocate_object(list)}) return record;
  // The original fallback assumes at least two active records.
  if(!list.tail || !list.tail->previous) throw std::logic_error{"object reuse requires at least two active records"};
  auto *record{list.tail};
  list.tail = record->previous;
  list.tail->next = nullptr;
  record->next = list.head;
  record->previous = nullptr;
  list.head->previous = record;
  list.head = record;
  return record;
}

} // namespace darker::game
