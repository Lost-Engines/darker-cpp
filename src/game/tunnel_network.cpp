#include "game/tunnel_network.h"
#include <bit>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include "maths/direction.h"
#include "maths/sine_table.h"
#include "maths/world_coordinates.h"

namespace darker::game {
namespace {

int segment_offset(uint8_t const type, uint8_t const route) {
  /// Normal and alternate junction records share the native five-byte backwards indexing
  if(route & 0x10) return type*16 + 1003 - (route & 15)*4 - (route & 3);
  return type*16 - 5 - (route & 31)*5;
}

std::array<int,2> boundary(uint8_t const encoded) {
  /// D120 decodes a clockwise position around a cell's 32-unit square perimeter
  if(encoded >= 128) throw std::invalid_argument{"Tunnel boundary coordinate exceeds its encoded perimeter"};
  auto const along{encoded & 31};
  switch(encoded >> 5) {
  case 0: return {along,0};
  case 1: return {32,along};
  case 2: return {32 - along,32};
  default: return {0,32 - along};
  }
}

uint8_t segment_length(int const x, int const y) {
  /// D0BE/92E6 take the integer square root of sixteen times the squared perimeter distance
  unsigned int const square{static_cast<unsigned int>((x*x + y*y)*16)};
  unsigned int low{0}, high{256};
  while(low + 1 < high) {
    auto const middle{(low + high) / 2};
    if(middle*middle <= square) low = middle;
    else high = middle;
  }
  return static_cast<uint8_t>(low);
}

int height_byte(unsigned int const nibble) {
  /// D37C/D384 replace the lowest two height levels before interpolating the vertical route
  return nibble == 0 ? 6 : nibble == 1 ? 11 : static_cast<int>(nibble*8);
}

uint16_t proximity(tunnel_segment const edge, uint16_t const x, uint16_t const y, uint8_t const height) {
  /// D474 scores a point against an edge using separately truncated products and the original perimeter orientation
  auto const origin{boundary(edge.first)};
  auto const dx{std::bit_cast<int16_t>(static_cast<uint16_t>((x - origin[0]*8)*2))};
  auto const dy{std::bit_cast<int16_t>(static_cast<uint16_t>((y - origin[1]*8)*2))};
  auto const sine{[&](unsigned int const phase){ return maths::original_sine[(edge.heading*2 + phase) % 1024]; }};
  auto perpendicular{static_cast<uint16_t>(((sine(256)*dy) >> 16) - ((sine(0)*dx) >> 16))};
  auto along{static_cast<uint16_t>(((sine(768)*dx) >> 16) - ((sine(0)*dy) >> 16))};
  if(edge.first >= 0x60 && static_cast<uint8_t>(0xa0 - edge.first) <= edge.second) {
    perpendicular = static_cast<uint16_t>(-perpendicular);
    along = static_cast<uint16_t>(-along);
  }
  along >>= 1;
  if(along >= edge.length) return 0xffff;
  auto const start{(edge.heights & 15)*4}, end{(edge.heights >> 4)*4};
  auto const vertical{std::abs(start + (end - start)*along/edge.length - height)};
  bool const negative{(perpendicular & 0x8000) != 0};
  perpendicular = static_cast<uint16_t>((perpendicular << 1) | (negative ? 1 : 0));
  if(negative) perpendicular = static_cast<uint16_t>(-perpendicular);
  return static_cast<uint16_t>(vertical + perpendicular);
}

} // namespace

tunnel_network::tunnel_network(std::span<std::byte const> const source) {
  /// D0BE expands the 200 ten-byte route records into sixteen-byte runtime records
  if(source.size() != 2000) throw std::invalid_argument{"Tunnel network requires the 200 original route records"};
  for(size_t tile{0}; tile < 200; ++tile) {
    data[tile*16] = source[tile*10];
    for(size_t edge{0}; edge < 3; ++edge) {
      auto const input{tile*10 + 1 + edge*3}, output{tile*16 + 1 + edge*5};
      uint8_t const first{std::to_integer<uint8_t>(source[input])}, second{std::to_integer<uint8_t>(source[input+1])};
      auto const a{boundary(first)}, b{boundary(second)};
      auto const x{a[0] - b[0]}, y{b[1] - a[1]};
      data[output] = source[input];
      data[output+1] = source[input+1];
      data[output+2] = source[input+2];
      data[output+3] = static_cast<std::byte>(segment_length(x,y));
      data[output+4] = static_cast<std::byte>((maths::direction_index(static_cast<uint16_t>(x),static_cast<uint16_t>(y)) >> 2) + 128);
    }
  }
}

std::span<std::byte const> tunnel_network::prepared_bytes() const noexcept {
  /// Preserve native table offsets for route selection, including the alternate junction records
  return data;
}

tunnel_segment tunnel_network::segment(uint8_t const type, uint8_t const route) const {
  /// D24B/D206 address five-byte edges backwards from each cell record, with the junction-table displacement
  return segment_at(segment_offset(type,route));
}

tunnel_segment tunnel_network::segment_at(int const index) const {
  /// Decode a segment view without changing the original table offsets
  if(index < 0 || static_cast<size_t>(index) + 5 > data.size()) throw std::out_of_range{"Tunnel segment leaves the prepared route table"};
  auto const read{[&](size_t const offset){ return std::to_integer<uint8_t>(data[static_cast<size_t>(index) + offset]); }};
  return {read(0),read(1),read(2),read(3),read(4)};
}

tunnel_junction tunnel_network::junction(uint8_t const type) const {
  /// D219 selects the next cell's first edge and exposes the shared junction flag
  if(type == 0 || type > 200) throw std::out_of_range{"Tunnel junction type exceeds its directory"};
  return {
    .flags{std::to_integer<uint8_t>(data[(type - 1)*16])},
    .edges{segment(type,2),segment(type,1),segment(type,0)}
  };
}

tunnel_boundary tunnel_network::crossing(uint8_t const type, tunnel_connection const source) const {
  /// D219 converts the selected entry endpoint into its adjoining cell and matching perimeter coordinate
  auto const edge{segment(type,source.route)};
  auto const endpoint{source.route & 0x80 ? edge.second : edge.first};
  constexpr std::array<int,4> steps{-256,1,256,-1};
  return {
    .cell{static_cast<uint16_t>(source.cell + steps[(endpoint >> 5) & 3])},
    .perimeter{static_cast<uint8_t>(((endpoint & 0x20 ? 0xa0 : 0x60) - endpoint) & 127)},
    .height{static_cast<uint8_t>(source.route & 0x80 ? edge.heights >> 4 : edge.heights & 15)}
  };
}

maths::world_position tunnel_network::point(uint8_t const type, uint8_t const route, uint16_t const cell, uint16_t const distance) const {
  /// D31F interpolates the route with signed division and wrapped word coordinates
  auto const edge{segment(type,route)};
  if(edge.length == 0) throw std::invalid_argument{"Cannot interpolate a zero-length tunnel segment"};
  bool const reverse{(route & 0x80) != 0};
  auto const from{boundary(reverse ? edge.second : edge.first)}, to{boundary(reverse ? edge.first : edge.second)};
  int const divisor{edge.length*2};
  int const along{std::bit_cast<int16_t>(distance)};
  auto const x{(cell & 255)*256 + from[0]*8 + (to[0] - from[0])*8*along/divisor};
  auto const y{(cell >> 8)*256 + from[1]*8 + (to[1] - from[1])*8*along/divisor};
  auto const start_height{height_byte(reverse ? edge.heights >> 4 : edge.heights & 15)};
  auto const end_height{height_byte(reverse ? edge.heights & 15 : edge.heights >> 4)};
  auto const height{std::bit_cast<int16_t>(static_cast<uint16_t>(((end_height - start_height)*256*along)/divisor + start_height*256))};
  return {static_cast<uint16_t>(x),static_cast<uint16_t>(y),static_cast<uint16_t>(height >> 3)};
}

uint8_t tunnel_network::direction(uint8_t const type, uint8_t const route, uint16_t const heading) const {
  /// D894 chooses the traversal end from the segment orientation and the object's heading
  auto const edge{segment(type,route)};
  auto phase{static_cast<uint8_t>((edge.heading >> 1) + (heading >> 8))};
  if(edge.first < 0x60 || static_cast<uint8_t>(0xa0 - edge.first) > edge.second) phase = static_cast<uint8_t>(~phase);
  return static_cast<uint8_t>((route & 0x7f) | (phase & 0x80));
}

tunnel_start tunnel_network::start(uint8_t const type, uint16_t const cell, uint16_t const encoded_heading) const {
  /// BEE7 snaps moving objects to the segment midpoint and separates route-index bits from their heading
  auto const route{static_cast<uint8_t>((encoded_heading >> 8) & 3)};
  auto const heading{static_cast<uint16_t>(encoded_heading & 0xfcff)};
  auto const edge{segment(type,route)};
  return {
    .position{point(type,route,cell,edge.length)},
    .heading{heading},
    .route{direction(type,route,heading)}
  };
}

tunnel_connection tunnel_network::connect(city_map const &cells, tunnel_connection const source, uint8_t const preferred_heading) const {
  /// D136/D159 cross a boundary and choose a connected segment by endpoint, height and heading error
  auto const type_at{[&](uint16_t const cell){
    auto const x{cell & 255}, y{cell >> 8};
    if(x >= 128 || y >= 128) throw std::out_of_range{"Tunnel connection leaves its map"};
    return cells[y*128+x].type;
  }};
  struct selection { tunnel_connection connection; uint8_t entry; };
  unsigned int visits{0};
  std::function<selection(tunnel_connection,int)> follow;
  follow = [&](tunnel_connection const current, int const current_offset)->selection {
    if(++visits > 200) throw std::invalid_argument{"Tunnel connection does not reach a matching boundary"};
    auto const edge{segment_at(current_offset)};
    auto const endpoint{current.route & 0x80 ? edge.second : edge.first};
    auto const height{current.route & 0x80 ? edge.heights >> 4 : edge.heights & 15};
    constexpr std::array<int,4> steps{-256,1,256,-1};
    auto const cell{static_cast<uint16_t>(current.cell + steps[(endpoint >> 5) & 3])};
    auto const entry{static_cast<uint8_t>(((endpoint & 0x20 ? 0xa0 : 0x60) - endpoint) & 127)};
    int first{segment_offset(type_at(cell),2)};
    bool const junction{first == 0xe8d1 - 0xe580};
    if(junction) first = 0xecc1 - 0xe580 - ((entry & 0x60) >> 1);
    int best_score{127}, best_angle{255}, best_offset{-1};
    uint8_t best_route{0};
    for(unsigned int i{0}; i < 3; ++i) {
      auto const candidate{segment_at(first + static_cast<int>(i)*5)};
      if(candidate.first == 0 && candidate.second == 0) continue;
      auto const first_delta{std::bit_cast<int8_t>(static_cast<uint8_t>((candidate.first - entry)*2))};
      auto const second_delta{std::bit_cast<int8_t>(static_cast<uint8_t>((candidate.second - entry)*2))};
      auto const first_score{std::abs(first_delta) + std::abs((candidate.heights & 15) - height)};
      auto const second_score{std::abs(second_delta) + std::abs((candidate.heights >> 4) - height)};
      bool const first_end{first_score < second_score};
      auto const score{first_end ? first_score : second_score};
      if(best_score < score) continue;
      auto const delta{first_end ? first_delta : second_delta};
      auto const heading{static_cast<uint8_t>(candidate.heading - (score == 0 ? 0 : delta < 0 ? -1 : 1))};
      auto const angle{std::abs(std::bit_cast<int8_t>(static_cast<uint8_t>(heading - preferred_heading)))};
      if(best_score == score && best_angle < angle) continue;
      best_score = score;
      best_angle = angle;
      best_offset = first + static_cast<int>(i)*5;
      best_route = static_cast<uint8_t>((first_end ? 0x80 : 0) | (2 - i));
    }
    if(best_offset < 0) throw std::invalid_argument{"Tunnel cell has no eligible connected segment"};
    auto const chosen{segment_at(best_offset)};
    auto const chosen_end{best_route & 0x80 ? chosen.first : chosen.second};
    selection result{{cell,best_route},entry};
    if((chosen_end ^ entry) & 0x60) result = follow({cell,static_cast<uint8_t>(best_route ^ 0x80)},best_offset);
    if(junction) result.connection.route |= static_cast<uint8_t>(0x10 | ((result.entry & 0x60) >> 3));
    return result;
  };
  return follow(source,segment_offset(type_at(source.cell),source.route)).connection;
}

std::optional<tunnel_trace> tunnel_network::trace(city_map const &cells, tunnel_connection source,
  maths::world_position const position, uint16_t const lookahead, uint8_t const preferred_heading) const {
  /// D284 projects onto the current edge, corrects crossings and samples a target through successive connected edges
  if(source.route & 0x40) return std::nullopt;
  auto const type_at{[&](uint16_t const cell){
    auto const x{cell & 255}, y{cell >> 8};
    if(x >= 128 || y >= 128) throw std::out_of_range{"Tunnel projection leaves its map"};
    return cells[y*128+x].type;
  }};
  unsigned int visits{0};
  int progress{0};
  for(;;) {
    if(++visits > 200) throw std::invalid_argument{"Tunnel projection does not reach a containing segment"};
    auto const edge{segment(type_at(source.cell),source.route)};
    if(edge.length == 0) throw std::invalid_argument{"Cannot project onto a zero-length tunnel segment"};
    bool const reverse{(source.route & 0x80) != 0};
    auto const from{boundary(reverse ? edge.second : edge.first)}, to{boundary(reverse ? edge.first : edge.second)};
    auto const x{static_cast<uint16_t>((source.cell & 255)*256 + from[0]*8)};
    auto const y{static_cast<uint16_t>((source.cell >> 8)*256 + from[1]*8)};
    auto const dx{std::bit_cast<int16_t>(static_cast<uint16_t>(position.column - x))};
    auto const dy{std::bit_cast<int16_t>(static_cast<uint16_t>(position.row - y))};
    progress = (dx*(to[0] - from[0])*8 + dy*(to[1] - from[1])*8) / (edge.length*2);
    if(progress >= 0) {
      if(progress > edge.length*2) progress = edge.length*2;
      break;
    }
    source = connect(cells,source,preferred_heading);
  }
  tunnel_trace result{
    .connection{source},
    .progress{static_cast<uint16_t>(progress)}
  };
  int distance{progress - lookahead};
  while(distance < 0) {
    if(++visits > 200) throw std::invalid_argument{"Tunnel lookahead exceeds connected route traversal"};
    source = connect(cells,source,preferred_heading);
    distance += segment(type_at(source.cell),source.route).length*2;
  }
  result.target = point(type_at(source.cell),source.route,source.cell,static_cast<uint16_t>(distance));
  return result;
}

std::optional<tunnel_connection> tunnel_network::reacquire(city_map const &cells, tunnel_connection const source,
  maths::world_position const position, uint8_t const preferred_heading) const {
  /// D39C searches the current cell and two axial neighbours for a sufficiently close route after free flight
  uint16_t const cell{static_cast<uint16_t>((position.column >> 8) | (position.row & 0xff00))};
  uint16_t best{64};
  uint8_t route{0};
  auto const search{[&](int const x_offset, int const y_offset){
    auto const column{static_cast<uint8_t>(cell + x_offset)}, row{static_cast<uint8_t>((cell >> 8) + y_offset)};
    if(row >= 128) return;
    auto const type{cells[row*128 + (column & 127)].type};
    if(type == 0) return;
    for(uint8_t i{0}; i < 3; ++i) {
      auto const edge{segment(type,i)};
      if(edge.first == 0) continue;
      auto const score{proximity(edge,static_cast<uint16_t>((position.column & 255) + x_offset*256),
        static_cast<uint16_t>((position.row & 255) + y_offset*256),static_cast<uint8_t>(position.height >> 6))};
      if(score < best) { best = score; route = i; }
    }
  }};
  search(0,0);
  if(best < 64) {
    if(cell == source.cell || ((route ^ source.route) & 31) == 0) return std::nullopt;
    return tunnel_connection{cell,route};
  }
  int const x_step{(position.column & 255) < 128 ? -1 : 1};
  int const y_step{(position.row & 255) < 128 ? -1 : 1};
  search(x_step,0);
  auto const horizontal{best};
  search(0,y_step);
  if(best >= 64) return std::nullopt;
  bool const vertical{best < horizontal};
  tunnel_connection const result{static_cast<uint16_t>(static_cast<uint8_t>(cell + (vertical ? 0 : x_step))
    | (static_cast<uint8_t>((cell >> 8) + (vertical ? y_step : 0))*256)),route};
  auto const next{connect(cells,result,preferred_heading)};
  if(result.cell == next.cell || ((result.route ^ next.route) & 31) == 0) return std::nullopt;
  return result;
}

} // namespace darker::game
