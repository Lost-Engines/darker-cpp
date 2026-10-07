#include "game/mission_script.h"
#include <bit>
#include <algorithm>
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
  auto const word{[&]{ auto const low{byte()}; return static_cast<uint16_t>(low | byte()*256); }};
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
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
    case 0x06:
      {
        uint16_t target{context.current_cell};
        if(opcode < 2) {
          auto const column{byte()};
          target = static_cast<uint16_t>(column | byte()*256);
        } else if(opcode < 4) target = static_cast<uint16_t>(0xd986 + byte()*112);
        else if(opcode == 6) target = 0xd986;
        if(!context.set_target) throw std::runtime_error{"Mission target change has no object consumer"};
        context.set_target(target,(opcode & 1) != 0);
        delay(10);
      }
      break;
    case 0x07: break;
    case 0x08:
      if(!context.retire_distant_actor) throw std::runtime_error{"Mission distance retirement has no actor consumer"};
      if(context.retire_distant_actor()) {
        script.stopped = true;
        script.continuation = cursor;
        script.deadline = now;
        return dispatched;
      }
      --cursor;
      delay(8);
      break;
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
          .alignment{static_cast<message_alignment>(opcode - 0x0c)}, .text{context.text}});
        context.text_cursor += length;
        delay(interval);
      }
      break;
    case 0x0f: context.message_setting = byte(); break;
    case 0x16: context.hud_reference = word(); break;
    case 0x17: context.transition_output = word(); break;
    case 0x18:
      if(!context.reset_shield) throw std::logic_error{"Mission shield reset has no player consumer"};
      context.reset_shield();
      context.transition_output = 700;
      break;
    case 0x19:
      if(!context.refill_weapon) throw std::logic_error{"Mission ammunition refill has no player consumer"};
      context.refill_weapon();
      break;
    case 0x2d: context.progress = std::max(context.progress,byte()); break;
    case 0x30:
      if(!context.toggle_weapons) throw std::logic_error{"Mission weapon toggle has no player consumer"};
      {
        auto const range{byte()};
        auto const mask{static_cast<uint16_t>(static_cast<int16_t>(0x8000) >> (range & 15))};
        context.toggle_weapons(std::rotl(mask,range >> 4));
      }
      break;
    case 0x35:
      if(!context.mark_aircraft_sites) throw std::logic_error{"Mission aircraft sites have no world consumer"};
      cursor += context.mark_aircraft_sites(context.program.subspan(cursor));
      break;
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
      {
        uint8_t origin{0}, count{0};
        if(opcode == 0x12 || opcode == 0x13) { origin = byte(); count = byte(); }
        if(!context.change_beacons) throw std::runtime_error{"Mission beacon change has no world consumer"};
        context.change_beacons(opcode,origin,count);
        if(opcode != 0x14) delay(6);
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
    case 0x24:
      if(!context.register_owner) throw std::logic_error{"Mission owner registration has no world consumer"};
      script.checkpoint = context.register_owner();
      script.stopped = true;
      script.continuation = cursor;
      script.deadline = now;
      return dispatched;
    case 0x26:
      if(!context.exchange_context) throw std::logic_error{"Mission context exchange has no saved context"};
      script.continuation = cursor;
      context.exchange_context(script);
      cursor = script.continuation;
      break;
    case 0x25:
      {
        auto const displacement{std::bit_cast<std::int8_t>(byte())};
        auto const target{static_cast<std::ptrdiff_t>(cursor) + displacement};
        if(target < 0 || static_cast<std::size_t>(target) >= context.program.size()) throw std::invalid_argument{"Mission branch exceeds its shared section"};
        cursor = static_cast<std::size_t>(target);
      }
      break;
    case 0x27:
      if(!context.set_tunnel_oscillation) throw std::logic_error{"Scripted tunnel direction has no actor consumer"};
      context.set_tunnel_oscillation(byte());
      break;
    case 0x2f:
      {
        auto const selection{byte()};
        if(!context.select_weapon) throw std::runtime_error{"Mission weapon selection has no player consumer"};
        context.select_weapon(selection);
      }
      break;
    case 0x34:
      if(!context.replace_world_objectives) throw std::logic_error{"Scripted building objectives have no world consumer"};
      cursor += context.replace_world_objectives(context.program.subspan(cursor));
      break;
    case 0x33:
      if(!context.adjust_objectives) throw std::logic_error{"Mission objective adjustment has no world consumer"};
      context.objectives_complete = context.adjust_objectives(byte());
      break;
    case 0x31:
      {
        auto const setting{byte()};
        if(!context.set_aircraft_spawning) throw std::runtime_error{"Mission aircraft spawning setting has no world consumer"};
        context.set_aircraft_spawning(setting);
      }
      break;
    case 0x32:
      {
        auto const setting{byte()};
        if(!context.set_building_attacks) throw std::runtime_error{"Mission building attack setting has no combat consumer"};
        context.set_building_attacks(setting);
      }
      break;
    default:
      throw std::runtime_error{std::format("Mission opcode {:02x} at {:04x} is not connected yet", opcode, offset)};
    }
  }
  return dispatched;
}

} // namespace darker::game
