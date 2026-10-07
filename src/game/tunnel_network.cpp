#include "game/tunnel_network.h"
#include <bit>
#include <stdexcept>
#include "maths/direction.h"

namespace darker::game {
namespace {

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
  int const index{type*16 - 5 - (route & (route & 0x10 ? 15 : 31))*5 + (route & 0x10 ? 1008 : 0)};
  if(index < 0 || static_cast<size_t>(index) + 5 > data.size()) throw std::out_of_range{"Tunnel segment leaves the prepared route table"};
  auto const read{[&](size_t const offset){ return std::to_integer<uint8_t>(data[static_cast<size_t>(index) + offset]); }};
  return {read(0),read(1),read(2),read(3),read(4)};
}

std::array<uint16_t,3> tunnel_network::point(uint8_t const type, uint8_t const route, uint16_t const cell, uint16_t const distance) const {
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
  return {.position{point(type,route,cell,edge.length)},.heading{heading},.route{direction(type,route,heading)}};
}

} // namespace darker::game
