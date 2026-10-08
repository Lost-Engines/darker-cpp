#include "presentation/player.h"
#include <algorithm>
#include <bit>
#include <format>
#include <stdexcept>

namespace darker::presentation {

std::vector<animation_frame> decode_animation(std::span<std::byte const> const data) {
  /// DF36 reads positioned scanline literals, transparent skips and row-end controls
  std::vector<animation_frame> frames;
  size_t p{0};
  auto const byte{[&]{ if(p == data.size()) throw std::invalid_argument{"Truncated animation"}; return std::to_integer<uint8_t>(data[p++]); }};
  auto const word{[&]{ auto const low{byte()}; return static_cast<uint16_t>(low | byte() * 256); }};
  while(p < data.size()) {
    auto const origin{word()};
    unsigned int x{origin}, y{byte()};
    auto const length{word()};
    if(length == 0 || length > data.size() - p) throw std::invalid_argument{"Invalid animation frame length"};
    auto const end{p + length};
    animation_frame frame;
    while(p < end) {
      auto const op{byte()};
      if(op == 128) { x = origin; ++y; }
      else if(op > 128) x += op & 127;
      else {
        if(static_cast<size_t>(op + 1) > end - p || x + op >= 1024 || y >= 256) throw std::invalid_argument{"Animation literal exceeds bounds"};
        for(unsigned int i{0}; i <= op; ++i) frame.push_back({static_cast<uint16_t>(x++),static_cast<uint16_t>(y),byte()});
      }
    }
    if(data[end - 1] != std::byte{128}) throw std::invalid_argument{"Animation frame lacks a row terminator"};
    frames.push_back(std::move(frame));
  }
  return frames;
}

player::player(resources::archive_set const &archives, resources::font_resource const &font,
  resources::scenario_resource const &scenario, size_t const record, uint8_t const completed_objects, bool const continued_mission) : archives{archives}, font{font},
  program{scenario.bytes(scenario.records()[record].shared)}, text{scenario.language(record, resources::scenario_language::english)},
  cursor{scenario.records()[record].entry_offset - scenario.records()[record].shared.offset}, interval{scenario.records()[record].time_multiplier}, object_counter{completed_objects}, continued_mission{continued_mission} {
  /// Keep the original record's shared program and language cursors separate
  if(interval == 0) throw std::invalid_argument{"Presentation interval is zero"};
  execute();
}

uint8_t player::byte() {
  /// Reject a script escaping its owning shared section
  if(cursor >= program.size()) throw std::invalid_argument{"Presentation script exceeds its section"};
  return std::to_integer<uint8_t>(program[cursor++]);
}

uint16_t player::word() {
  /// Script words are little-endian regardless of host architecture
  auto const low{byte()};
  return static_cast<uint16_t>(low | byte() * 256);
}

void player::load_image(resources::resource_id const id) {
  /// C07A loads palette and pixel data without copying the image into the displayed background
  auto const bytes{archives.load(id)};
  auto const decoded{graphics::decode_palette(bytes, colours)};
  colours = decoded.palette;
  auto const pixels{std::span{bytes}.subspan(decoded.bytes_consumed)};
  image_pixels.assign(pixels.begin(),pixels.end());
}

void player::draw_image(unsigned int const width, unsigned int const height, unsigned int const x, unsigned int const y) {
  /// C0B4 copies the retained image using the current scene layout
  if(image_pixels.empty()) return;
  if(image_pixels.size() != width * height || x + width > 320 || y + height > 240) throw std::invalid_argument{"Presentation image/layout mismatch"};
  for(size_t row{0}; row < height; ++row) for(size_t column{0}; column < width; ++column) {
    background.pixels[(row + y) * 320 + column + x] = std::to_integer<uint8_t>(image_pixels[row * width + column]);
  }
}

void player::image(resources::resource_id const id, unsigned int const width, unsigned int const height, unsigned int const x, unsigned int const y) {
  /// C09E combines loading with display; palette-only resources leave the background intact
  load_image(id);
  draw_image(width,height,x,y);
}

void player::execute() {
  /// Dispatch the original presentation subset, retaining chained scene pointers and independent animation pairs
  for(unsigned int instructions{0}; !stopped && ticks >= deadline; ++instructions) {
    if(instructions == 4096) throw std::runtime_error{"Presentation failed to yield"};
    auto const op{byte()};
    if(op >= 128) { selected = op & 127; if(selected >= pairs.size()) throw std::invalid_argument{"Invalid animation pair"}; continue; }
    switch(op) {
    case 0x0c:
    case 0x0d:
    case 0x0e: {
      auto const duration{byte()}, delay{byte()};
      if(text_cursor >= text.size()) throw std::invalid_argument{"Presentation caption exceeds its language section"};
      auto const width{std::to_integer<uint8_t>(text[text_cursor++])};
      if(!width) break;
      if(text_cursor >= text.size()) throw std::invalid_argument{"Presentation caption has no length"};
      auto const length{std::to_integer<uint8_t>(text[text_cursor++])};
      if(length > text.size()-text_cursor) throw std::invalid_argument{"Presentation caption is truncated"};
      auto const x{op == 0x0d ? 12 : op == 0x0e ? 308-width : (321-width-(width < caption_width_extension ? 256 : 0))/2};
      captions[op-0x0c] = {text.subspan(text_cursor,length),ticks+duration*interval,static_cast<int16_t>(x),static_cast<uint16_t>(width+(op == 0x0c && width < caption_width_extension ? 256 : 0))};
      text_cursor += length;
      deadline += delay*interval;
      break;
    }
    case 0x0f: caption_width_extension = byte(); break;
    case 0x10:
      caption_y = byte();
      caption_colours = word();
      caption_width_extension = 0;
      captions = {};
      break;
    case 0x1a: checkpoint = cursor; break;
    case 0x1f:
      if(byte() > object_counter) {
        cursor = checkpoint;
        deadline += 8 * interval;
      }
      break;
    case 0x22: deadline += byte() * interval; break;
    case 0x23: stopped = true; break;
    case 0x28: {
      auto const heading{byte()};
      auto const site{word()};
      entry = landing_entry{site,heading};
      departure_destination = site;
      break;
    }
    case 0x29: departure_destination = word(); break;
    case 0x2c:
      // C2BC distinguishes a fresh briefing from continuation through the language displacement.
      if(text_cursor + 2 > text.size()) throw std::invalid_argument{"Presentation message branch exceeds its language section"};
      if(continued_mission) {
        // C2BC follows the language displacement and stops this presentation on a nonzero prior outcome.
        auto const displacement{std::to_integer<uint8_t>(text[text_cursor]) | (std::to_integer<uint8_t>(text[text_cursor+1]) << 8)};
        if(static_cast<size_t>(displacement) > text.size()-text_cursor) throw std::invalid_argument{"Presentation message branch exceeds its language section"};
        text_cursor += displacement;
        input_policy = 0;
        stopped = true;
        return;
      }
      text_cursor += 2;
      break;
    case 0x30: {
      auto const range{byte()};
      auto const mask{static_cast<uint16_t>(static_cast<int16_t>(0x8000) >> (range & 15))};
      weapon_toggles ^= std::rotl(mask,range >> 4);
      break;
    }
    case 0x36: difficulty = byte(); break;
    case 0x37: score = byte(); break;
    case 0x3a:
      text_y = byte(); image_height = byte();
      if(image_height) { image_width = word(); image_y = byte(); image_x = word(); }
      break;
    case 0x3b:
      page.glyphs.clear(); // BFE4 clears DA30 even when the background resource is already selected.
      image({0,static_cast<unsigned int>(22 + byte())},320,240,0,0);
      face = resources::font_face::wide;
      break;
    case 0x3c: input_policy = byte(); break;
    case 0x3d: page.glyphs.clear(); background.pixels.fill(0); face = resources::font_face::compact; break;
    case 0x3e:
      page = graphics::lay_out_text(text.subspan(text_cursor),font,face,
        {.x{static_cast<uint16_t>(page.cursor.margin + 16)}, .y{text_y}, .colour{0xfffe},
         .margin{page.cursor.margin}, .line_step{page.cursor.line_step}});
      text_cursor += page.consumed;
      break;
    case 0x3f: { auto const displacement{std::bit_cast<int16_t>(word())}; next = cursor + displacement; break; }
    case 0x40: music = byte(); break;
    case 0x41: load_image({3,static_cast<uint8_t>(47 + byte())}); break;
    case 0x43: draw_image(image_width,image_height,image_x,image_y); break;
    case 0x42: image({3,static_cast<uint8_t>(47 + byte())},image_width,image_height,image_x,image_y); break;
    case 0x44:
    case 0x45: {
      auto const id{byte()};
      auto frames{decode_animation(archives.load({static_cast<unsigned int>(id >> 6), static_cast<unsigned int>(id & 63)}))};
      auto &destination{animations[op - 0x44]};
      auto &count{animation_counts[op - 0x44]};
      for(auto &frame : frames) {
        if(count == destination.size()) destination.push_back(std::move(frame));
        else destination[count] = std::move(frame);
        ++count;
      }
      break;
    }
    case 0x46:
    case 0x47: animation_counts.fill(0); break; // DADA resets append descriptors, retaining existing frame-table entries.
    case 0x48: { auto const first{byte()}; pairs[selected] = {first,byte()}; break; }
    case 0x49: { auto const frame{byte()}; pairs[selected] = {frame,frame}; break; }
    case 0x4a: deadline += (std::abs(pairs[selected].current - pairs[selected].target) + 1) * interval; break;
    case 0x4b: pairs[selected] = {}; break;
    case 0x4c: {
      auto const duration{byte()};
      repeat_delay = !repeat_delay;
      if(repeat_delay) { cursor -= 2; deadline += duration * interval; }
      break;
    }
    default: throw std::invalid_argument{std::format("Unsupported presentation opcode {:02x} at shared offset {:04x}",op,cursor-1)};
    }
  }
}

void player::advance(uint32_t const elapsed_ticks) {
  /// D938 paces presentation iterations by the record multiplier, independently of flight frames
  pending += elapsed_ticks;
  while(pending >= interval) {
    pending -= interval;
    ticks += interval;
    for(auto &pair : pairs) {
      if(pair.current != 160 && pair.current != pair.target) pair.current = static_cast<uint8_t>(pair.current + (pair.current < pair.target ? 1 : -1));
    }
    execute();
  }
}

bool player::continue_page() {
  /// DA0B follows the saved scene continuation while retaining the language cursor and loaded frame tables
  if(!next) return false;
  cursor = *next;
  next.reset();
  stopped = false;
  deadline = ticks;
  execute();
  return true;
}

bool player::finished() const noexcept {
  /// A stopped script without another scene is the end of this presentation record
  return stopped && !next;
}

size_t player::consumed_text() const noexcept {
  /// Supply the post-briefing cursor to the in-flight counted-message interpreter
  return text_cursor;
}

void player::draw(framework::render::cockpit_framebuffer &output, std::array<int,2> const pointer) const {
  /// Composite the retained background, descending animation channels and formatted text before palette expansion
  auto frame{background};
  for(auto i{pairs.rbegin()}; i != pairs.rend(); ++i) {
    if(i->current == 160) continue;
    auto const channel{i->current < 161 ? 0 : 1};
    auto const index{i->current < 161 ? i->current : i->current - 161};
    if(static_cast<size_t>(index) >= animations[channel].size()) throw std::invalid_argument{"Animation reference exceeds loaded frames"};
    for(auto const &pixel : animations[channel][index]) {
      auto const y{pixel.y + image_y}; // BFD3 patches DABA: the frame Y is relative to the scene image origin.
      if(pixel.x < 320 && y < 240) frame.pixels[y * 320 + pixel.x] = pixel.colour;
    }
  }
  for(auto const &glyph : page.glyphs) graphics::draw_glyph(frame,font,face,glyph.code,glyph.position,
    {.ink{static_cast<uint8_t>(glyph.colour >> 8)},.edge{static_cast<uint8_t>(glyph.colour)}});
  // D8E6 places the three independent counted-message slots at row E5 for film subtitles.
  for(size_t const channel : {1u,0u,2u}) {
    auto const &caption{captions[channel]};
    if(ticks >= caption.expiry) continue;
    graphics::draw_message(frame,font,face,caption.text,{.x{caption.x},.y{caption_y}},caption.width,
      {.ink{static_cast<uint8_t>(caption_colours >> 8)},.edge{static_cast<uint8_t>(caption_colours)}});
  }
  // DA75 selects a hover bit below row 225, split at columns 284 and 302; DA48 selects its palette pair.
  auto const hover{pointer[1] >= 225 && pointer[0] >= 284 ? (pointer[0] < 302 ? 4 : 1) : 0};
  if(input_policy & 1) graphics::draw_glyph(frame,font,face,62,{.x{305},.y{226}},
    {.ink{static_cast<uint8_t>(hover == 1 ? 253 : 255)},.edge{static_cast<uint8_t>(hover == 1 ? 252 : 254)}});
  if(input_policy & 4) graphics::draw_glyph(frame,font,face,60,{.x{287},.y{226}},
    {.ink{static_cast<uint8_t>(hover == 4 ? 253 : 255)},.edge{static_cast<uint8_t>(hover == 4 ? 252 : 254)}});
  framework::render::expand_palette(frame,colours.colours,output);
}

} // namespace darker::presentation
