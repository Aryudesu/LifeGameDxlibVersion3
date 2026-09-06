#include "PatternLibrary.h"
#include "RleParser.h"

#include <array>
#include <cassert>
#include <string>
#include <vector>

namespace {
constexpr std::array<PatternCell, 4> Block = {{{0,0},{1,0},{0,1},{1,1}}};
constexpr std::array<PatternCell, 6> Beehive = {{{1,0},{2,0},{0,1},{3,1},{1,2},{2,2}}};
constexpr std::array<PatternCell, 5> Boat = {{{0,0},{1,0},{0,1},{2,1},{1,2}}};
constexpr std::array<PatternCell, 6> Ship = {{{1,0},{2,0},{0,1},{2,1},{0,2},{1,2}}};
constexpr std::array<PatternCell, 8> Pond = {{{1,0},{2,0},{0,1},{3,1},{0,2},{3,2},{1,3},{2,3}}};
constexpr std::array<PatternCell, 4> Tub = {{{1,0},{0,1},{2,1},{1,2}}};
constexpr std::array<PatternCell, 6> AircraftCarrier = {{{0,0},{1,0},{0,1},{3,1},{2,2},{3,2}}};
constexpr std::array<PatternCell, 6> Snake = {{{0,0},{1,0},{3,0},{0,1},{2,1},{3,1}}};
constexpr std::array<PatternCell, 7> Loaf = {{{1,0},{0,1},{2,1},{0,2},{3,2},{1,3},{2,3}}};
constexpr std::array<PatternCell, 7> Eater = {{{0,0},{1,0},{1,1},{1,2},{3,2},{2,3},{3,3}}};
constexpr std::array<PatternCell, 7> LongBoat = {{{2,0},{1,1},{3,1},{0,2},{2,2},{0,3},{1,3}}};
constexpr std::array<PatternCell, 8> Mango = {{{1,0},{2,0},{0,1},{3,1},{1,2},{4,2},{2,3},{3,3}}};
constexpr std::array<PatternCell, 8> HookWithTail = {{{0,0},{1,0},{1,1},{1,2},{3,2},{4,2},{2,3},{4,3}}};
constexpr std::array<PatternCell, 8> Shillelagh = {{{0,0},{1,0},{0,1},{3,1},{4,1},{1,2},{2,2},{4,2}}};
constexpr std::array<PatternCell, 8> TubWithTail = {{{1,0},{0,1},{2,1},{1,2},{3,2},{3,3},{3,4},{4,4}}};
constexpr std::array<PatternCell, 8> Canoe = {{{0,0},{1,0},{0,1},{1,2},{2,3},{4,3},{3,4},{4,4}}};
constexpr std::array<PatternCell, 3> Blinker = {{{0,0},{0,1},{0,2}}};
constexpr std::array<PatternCell, 12> TrafficLight = {{{4,0},{4,1},{4,2},{4,6},{4,7},{4,8},{0,4},{1,4},{2,4},{6,4},{7,4},{8,4}}};
constexpr std::array<PatternCell, 6> Toad = {{{0,0},{0,1},{0,2},{1,1},{1,2},{1,3}}};
constexpr std::array<PatternCell, 6> Beacon = {{{0,0},{1,0},{0,1},{3,2},{2,3},{3,3}}};
constexpr std::array<PatternCell, 6> Clock = {{{1,0},{1,1},{3,1},{0,2},{2,2},{2,3}}};
constexpr std::array<PatternCell, 48> Pulsar = {{{2,0},{3,0},{4,0},{8,0},{9,0},{10,0},{0,2},{5,2},{7,2},{12,2},{0,3},{5,3},{7,3},{12,3},{0,4},{5,4},{7,4},{12,4},{2,5},{3,5},{4,5},{8,5},{9,5},{10,5},{2,7},{3,7},{4,7},{8,7},{9,7},{10,7},{0,8},{5,8},{7,8},{12,8},{0,9},{5,9},{7,9},{12,9},{0,10},{5,10},{7,10},{12,10},{2,12},{3,12},{4,12},{8,12},{9,12},{10,12}}};
constexpr std::array<PatternCell, 35> ClockTwo = {{{6,0},{7,0},{6,1},{7,1},{4,3},{5,3},{6,3},{7,3},{0,4},{1,4},{3,4},{6,4},{8,4},{0,5},{1,5},{3,5},{5,5},{8,5},{3,6},{5,6},{8,6},{10,6},{11,6},{3,7},{8,7},{10,7},{11,7},{4,8},{5,8},{6,8},{7,8},{4,10},{5,10},{4,11},{5,11}}};
constexpr std::array<PatternCell, 35> Pinwheel = {{{6,0},{7,0},{6,1},{7,1},{4,3},{5,3},{6,3},{7,3},{0,4},{1,4},{3,4},{6,4},{8,4},{0,5},{1,5},{3,5},{4,5},{8,5},{3,6},{5,6},{8,6},{10,6},{11,6},{3,7},{8,7},{10,7},{11,7},{4,8},{5,8},{6,8},{7,8},{4,10},{5,10},{4,11},{5,11}}};
constexpr std::array<PatternCell, 41> HertzOscillatorOne = {{{5,0},{6,0},{5,1},{6,1},{0,3},{1,3},{5,3},{6,3},{7,3},{8,3},{12,3},{13,3},{0,4},{2,4},{4,4},{9,4},{11,4},{13,4},{2,5},{4,5},{5,5},{9,5},{11,5},{0,6},{2,6},{4,6},{9,6},{11,6},{13,6},{0,7},{1,7},{5,7},{6,7},{7,7},{8,7},{12,7},{13,7},{5,9},{6,9},{5,10},{6,10}}};
constexpr std::array<PatternCell, 35> HertzOscillatorTwo = {{{5,0},{6,0},{5,1},{6,1},{5,3},{6,3},{7,3},{8,3},{11,3},{12,3},{1,4},{2,4},{4,4},{9,4},{12,4},{2,5},{4,5},{5,5},{9,5},{11,5},{1,6},{4,6},{9,6},{11,6},{12,6},{1,7},{2,7},{5,7},{6,7},{7,7},{8,7},{5,9},{6,9},{5,10},{6,10}}};
constexpr std::array<PatternCell, 8> Pentadecathlon = {{{1,0},{6,0},{0,1},{1,1},{6,1},{7,1},{1,2},{6,2}}};
constexpr std::array<PatternCell, 9> LightweightSpaceship = {{{0,0},{3,0},{4,1},{0,2},{4,2},{1,3},{2,3},{3,3},{4,3}}};
constexpr std::array<PatternCell, 11> MiddleweightSpaceship = {{{2,0},{0,1},{4,1},{5,2},{0,3},{5,3},{1,4},{2,4},{3,4},{4,4},{5,4}}};
constexpr std::array<PatternCell, 13> HeavyweightSpaceship = {{{2,0},{3,0},{0,1},{5,1},{6,2},{0,3},{6,3},{1,4},{2,4},{3,4},{4,4},{5,4},{6,4}}};
constexpr std::array<PatternCell, 5> Glider = {{{1,0},{2,1},{0,2},{1,2},{2,2}}};
constexpr std::array<PatternCell, 36> GosperGliderGun = {{{24,0},{22,1},{24,1},{12,2},{13,2},{20,2},{21,2},{34,2},{35,2},{11,3},{15,3},{20,3},{21,3},{34,3},{35,3},{0,4},{1,4},{10,4},{16,4},{20,4},{21,4},{0,5},{1,5},{10,5},{14,5},{16,5},{17,5},{22,5},{24,5},{10,6},{16,6},{24,6},{11,7},{15,7},{12,8},{13,8}}};

std::vector<PatternCell> parseEmbeddedRle(const char* rle) {
    ParsedRlePattern parsed;
    std::string error;
    const bool ok = RleParser::parse(rle, parsed, error);
    assert(ok && "Embedded RLE must be valid");
    return ok ? std::move(parsed.cells) : std::vector<PatternCell>{};
}

const std::vector<PatternCell> RPentomino = parseEmbeddedRle("x = 3, y = 3, rule = B3/S23\nb2o$2o$bo!");
const std::vector<PatternCell> Acorn = parseEmbeddedRle("x = 7, y = 3, rule = B3/S23\nbo$3bo$2o2b3o!");
const std::vector<PatternCell> DieHard = parseEmbeddedRle("x = 8, y = 3, rule = B3/S23\n6bo$2o$bo3b3o!");
const std::vector<PatternCell> BHeptomino = parseEmbeddedRle("x = 4, y = 3, rule = B3/S23\nob2o$3o$bo!");
const std::vector<PatternCell> Thunderbird = parseEmbeddedRle("x = 3, y = 5, rule = B3/S23\n3o2$bo$bo$bo!");
const std::vector<PatternCell> IHeptomino = parseEmbeddedRle("x = 4, y = 4, rule = B3/S23\n2o$bo$b2o$2b2o!");
const std::vector<PatternCell> Bunnies = parseEmbeddedRle("x = 8, y = 4, rule = B3/S23\no5bo$2bo3bo$2bo2bobo$bobo!");
const std::vector<PatternCell> Lidka = parseEmbeddedRle("x = 9, y = 15, rule = B3/S23\nbo$obo$bo8$8bo$6bobo$5b2obo2$4b3o!");
const std::vector<PatternCell> FigureEight = parseEmbeddedRle("x = 6, y = 6, rule = B3/S23\n2o$2obo$4bo$bo$2bob2o$4b2o!");
const std::vector<PatternCell> KoksGalaxy = parseEmbeddedRle("x = 9, y = 9, rule = B3/S23\n2bo2bobob$2obob3ob$bo6bo$2o5bob2$bo5b2o$o6bob$b3obob2o$bobo2bo!");
const std::vector<PatternCell> Loafer = parseEmbeddedRle("x = 9, y = 9, rule = B3/S23\nb2o2bob2o$o2bo2b2o$bobo$2bo$8bo$6b3o$5bo$6bo$7b2o!");
const std::vector<PatternCell> Copperhead = parseEmbeddedRle("x = 8, y = 12, rule = B3/S23\nb2o2b2o$3b2o$3b2o$obo2bobo$o6bo2$o6bo$b2o2b2o$2b4o2$3b2o$3b2o!");
const std::vector<PatternCell> SimkinGliderGun = parseEmbeddedRle("x = 33, y = 14, rule = B3/S23\n2o5b2o$2o5b2o2$4b2o$4b2o5$22bo3bo$21bo4b2o$20b2o5b2o2b2o$21b2o4bo3b2o$22bo!");

const std::array<LifePattern, 46> Patterns = {{
    {"Cell", PatternCategory::Cell, {}},
    {"Block", PatternCategory::StillLife, Block}, {"Beehive", PatternCategory::StillLife, Beehive},
    {"Boat", PatternCategory::StillLife, Boat}, {"Ship", PatternCategory::StillLife, Ship},
    {"Pond", PatternCategory::StillLife, Pond}, {"Tub", PatternCategory::StillLife, Tub},
    {"Aircraft Carrier", PatternCategory::StillLife, AircraftCarrier}, {"Snake", PatternCategory::StillLife, Snake},
    {"Loaf", PatternCategory::StillLife, Loaf}, {"Eater 1", PatternCategory::StillLife, Eater},
    {"Long Boat", PatternCategory::StillLife, LongBoat}, {"Mango", PatternCategory::StillLife, Mango},
    {"Hook With Tail", PatternCategory::StillLife, HookWithTail}, {"Shillelagh", PatternCategory::StillLife, Shillelagh},
    {"Tub With Tail", PatternCategory::StillLife, TubWithTail}, {"Canoe", PatternCategory::StillLife, Canoe},
    {"Blinker", PatternCategory::Oscillator, Blinker}, {"Traffic Light", PatternCategory::Oscillator, TrafficLight},
    {"Toad", PatternCategory::Oscillator, Toad}, {"Beacon", PatternCategory::Oscillator, Beacon},
    {"Clock", PatternCategory::Oscillator, Clock}, {"Pulsar", PatternCategory::Oscillator, Pulsar},
    {"Clock 2", PatternCategory::Oscillator, ClockTwo}, {"Pinwheel", PatternCategory::Oscillator, Pinwheel},
    {"Hertz Oscillator 1", PatternCategory::Oscillator, HertzOscillatorOne},
    {"Hertz Oscillator 2", PatternCategory::Oscillator, HertzOscillatorTwo},
    {"Pentadecathlon", PatternCategory::Oscillator, Pentadecathlon},
    {"Figure Eight", PatternCategory::Oscillator, std::span<const PatternCell>{FigureEight}},
    {"Kok's Galaxy", PatternCategory::Oscillator, std::span<const PatternCell>{KoksGalaxy}},
    {"R-pentomino", PatternCategory::Methuselah, std::span<const PatternCell>{RPentomino}},
    {"Acorn", PatternCategory::Methuselah, std::span<const PatternCell>{Acorn}},
    {"Die Hard", PatternCategory::Methuselah, std::span<const PatternCell>{DieHard}},
    {"B-heptomino", PatternCategory::Methuselah, std::span<const PatternCell>{BHeptomino}},
    {"Thunderbird", PatternCategory::Methuselah, std::span<const PatternCell>{Thunderbird}},
    {"I-heptomino", PatternCategory::Methuselah, std::span<const PatternCell>{IHeptomino}},
    {"Bunnies", PatternCategory::Methuselah, std::span<const PatternCell>{Bunnies}},
    {"Lidka", PatternCategory::Methuselah, std::span<const PatternCell>{Lidka}},
    {"LWSS", PatternCategory::Spaceship, LightweightSpaceship}, {"MWSS", PatternCategory::Spaceship, MiddleweightSpaceship},
    {"HWSS", PatternCategory::Spaceship, HeavyweightSpaceship}, {"Glider", PatternCategory::Spaceship, Glider},
    {"Loafer", PatternCategory::Spaceship, std::span<const PatternCell>{Loafer}},
    {"Copperhead", PatternCategory::Spaceship, std::span<const PatternCell>{Copperhead}},
    {"Gosper Glider Gun", PatternCategory::Gun, GosperGliderGun},
    {"Simkin Glider Gun", PatternCategory::Gun, std::span<const PatternCell>{SimkinGliderGun}},
}};
} // namespace

namespace PatternLibrary {
std::span<const LifePattern> patterns() noexcept { return Patterns; }
const LifePattern& at(std::size_t index) noexcept { return Patterns[index % Patterns.size()]; }
std::size_t size() noexcept { return Patterns.size(); }
const char* categoryName(PatternCategory category) noexcept {
    switch (category) {
    case PatternCategory::Cell: return "Cell";
    case PatternCategory::StillLife: return "Still Life";
    case PatternCategory::Oscillator: return "Oscillator";
    case PatternCategory::Methuselah: return "Methuselah";
    case PatternCategory::Spaceship: return "Spaceship";
    case PatternCategory::Gun: return "Gun";
    }
    return "Unknown";
}
} // namespace PatternLibrary
