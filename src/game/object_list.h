#pragma once

#include <stdexcept>

namespace darker::game {

// non-owning intrusive list: the enclosing pool keeps record addresses stable
// free records use only next; previous and payload are deliberately retained
template<typename Record>
class object_list {
public:
  struct links {
    Record *head{nullptr};
    Record *tail{nullptr};
    Record *free{nullptr};
  };

  object_list() = default;
  explicit object_list(links initial) noexcept : roots{initial} {}
  object_list(object_list const &) = delete;
  object_list &operator=(object_list const &) = delete;

  Record *head() const noexcept { return roots.head; }
  Record *tail() const noexcept { return roots.tail; }
  Record *free_head() const noexcept { return roots.free; }

  void add_free_record(Record &record) noexcept {
    /// Attach an unused record from the owning pool without clearing its retained payload
    record.next = roots.free;
    roots.free = &record;
  }

  Record *allocate() {
    /// 1C95 takes the free-list head and prepends it without clearing payload
    auto *record{roots.free};
    if(!record) return nullptr;
    roots.free = record->next;
    record->next = roots.head;
    record->previous = nullptr;
    if(roots.head) roots.head->previous = record;
    else roots.tail = record;
    roots.head = record;
    return record;
  }

  Record *unlink(Record &record) {
    /// 7AB4 repairs active neighbours and returns the next record without recycling
    auto *next{record.next};
    if(record.previous) record.previous->next = next;
    else roots.head = next;
    if(next) next->previous = record.previous;
    else roots.tail = record.previous;
    return next;
  }

  Record *recycle(Record &record) {
    /// 1CD4 unlinks the active record and prepends it to the free list
    auto *next{unlink(record)};
    record.next = roots.free;
    roots.free = &record;
    return next;
  }

  Record *allocate_or_reuse() {
    /// 1CBF falls back to moving the oldest active record to the head
    if(auto *record{allocate()}) return record;
    // the original fallback assumes at least two active records
    if(!roots.tail || !roots.tail->previous) throw std::logic_error{"object reuse requires at least two active records"};
    auto *record{roots.tail};
    roots.tail = record->previous;
    roots.tail->next = nullptr;
    record->next = roots.head;
    record->previous = nullptr;
    roots.head->previous = record;
    roots.head = record;
    return record;
  }

private:
  links roots;
};

} // namespace darker::game
