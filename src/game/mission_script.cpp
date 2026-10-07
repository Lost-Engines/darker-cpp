#include "game/mission_script.h"
#include <bit>
#include <format>
#include <stdexcept>

namespace darker::game {
namespace {

bool negative_difference(std::uint16_t const left, std::uint16_t const right) noexcept {
  /// BF97 tests the subtraction's sign bit, including the exact half-range boundary
  return static_cast<std::uint16_t>(left - right) & 0x8000;
}

std::uint8_t read_byte(std::span<std::byte const> const bytes, std::size_t &cursor) {
  /// Keep scripts and counted-message streams bounded independently
  if(cursor >= bytes.size()) throw std::invalid_argument{"Mission script or message exceeds its resource section"};
  return std::to_integer<std::uint8_t>(bytes[cursor++]);
}

bool flag_condition(std::uint8_t const value, std::uint8_t const operand) noexcept {
  /// C1FA uses a sign-extended operand to encode both all-set and all-clear bit tests
  auto const sign{operand & 128 ? 255 : 0};
  return ((value ^ operand) & (operand ^ sign)) == 0;
}

} // namespace

std::size_t advance_mission_script(mission_script &script, mission_context &context) {
  /// BF97/BFA9 execute overdue instructions until a deadline yields; failed waits rewind to the saved checkpoint
  auto const now{static_cast<std::uint16_t>(context.clock)};
  std::size_t dispatched{0};
  if(negative_difference(now, script.deadline)) return dispatched;
  if(script.stopped) {
    script.deadline = now;
    return dispatched;
  }
  auto cursor{script.continuation};
  auto const byte{[&]{ return read_byte(context.program, cursor); }};
  auto const delay{[&](unsigned int const duration){
    script.deadline = static_cast<std::uint16_t>(script.deadline + duration * context.time_multiplier);
    script.continuation = cursor;
  }};
  auto const wait{[&](bool const ready){
    if(!ready) {
      cursor = script.checkpoint;
      delay(8);
    }
  }};
  while(!negative_difference(now, script.deadline)) {
    // Valid programs can catch up many waits; reject an unbounded zero-time loop explicitly.
    if(dispatched == 65536) throw std::runtime_error{"Mission script exceeded its instruction budget without yielding"};
    ++dispatched;
    auto const offset{cursor};
    auto const opcode{byte()};
    if(opcode >= 128) {
      context.animation_parameter = static_cast<std::uint8_t>(opcode << 1);
      continue;
    }
    switch(opcode) {
    case 0x07: break;
    case 0x09:
    case 0x0a:
    case 0x0b:
      {
        auto const count{byte()};
        if(!context.activate_reserves) throw std::runtime_error{"Mission reserve activation has no world consumer"};
        context.objectives_complete = context.activate_reserves(opcode,count);
      }
      break;
    case 0x0c:
    case 0x0d:
    case 0x0e:
      {
        auto const duration{byte()};
        auto const interval{byte()};
        if(context.suppress_messages) break;
        auto const width{read_byte(context.text, context.text_cursor)};
        if(!width) break;
        auto const length{read_byte(context.text, context.text_cursor)};
        if(length > context.text.size() - context.text_cursor) throw std::invalid_argument{"Counted mission message exceeds its language section"};
        context.messages.push_back({.offset{context.text_cursor}, .length{length}, .width{width},
          .expiry{static_cast<std::uint16_t>(now + duration * context.time_multiplier)},
          .alignment{static_cast<message_alignment>(opcode - 0x0c)}});
        context.text_cursor += length;
        delay(interval);
      }
      break;
    case 0x1a:
      script.checkpoint = cursor;
      script.checkpoint_clock = static_cast<std::uint16_t>((context.clock >> 8) + ((context.clock >> 7) & 1));
      break;
    case 0x1b:
      {
        auto const target{static_cast<std::uint16_t>(script.checkpoint_clock + byte() * 8)};
        wait(negative_difference(target, static_cast<std::uint16_t>(context.clock >> 8)));
      }
      break;
    case 0x1e: wait(context.objectives_complete); break;
    case 0x1c:
      {
        auto const index{byte()};
        auto const mask{byte()};
        if(index >= context.object_flags.size()) throw std::out_of_range{"Mission wait refers to an unknown object"};
        wait(flag_condition(context.object_flags[index], mask));
      }
      break;
    case 0x1d:
      {
        auto const column_byte{byte()};
        auto const row{byte()};
        auto const mask{byte()};
        auto const index{static_cast<std::size_t>(row) * 128 + (column_byte >> 1)};
        if(index >= context.cells.size()) throw std::out_of_range{"Mission wait refers to an unknown world cell"};
        auto const &cell{context.cells[index]};
        wait(flag_condition(column_byte & 1 ? cell.state : cell.type, mask));
      }
      break;
    case 0x1f: wait(byte() <= context.object_counter); break;
    case 0x20: wait(byte() <= context.counter); break;
    case 0x21: wait(context.at_target_cell); break;
    case 0x22: delay(byte()); break;
    case 0x23:
      script.stopped = true;
      script.continuation = cursor;
      script.deadline = now;
      return dispatched;
    case 0x25:
      {
        auto const displacement{std::bit_cast<std::int8_t>(byte())};
        auto const target{static_cast<std::ptrdiff_t>(cursor) + displacement};
        if(target < 0 || static_cast<std::size_t>(target) >= context.program.size()) throw std::invalid_argument{"Mission branch exceeds its shared section"};
        cursor = static_cast<std::size_t>(target);
      }
      break;
    default:
      throw std::runtime_error{std::format("Mission opcode {:02x} at {:04x} is not connected yet", opcode, offset)};
    }
  }
  return dispatched;
}

} // namespace darker::game
