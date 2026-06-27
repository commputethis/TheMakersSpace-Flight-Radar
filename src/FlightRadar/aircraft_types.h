// ================================================================
// aircraft_types.h
// ----------------------------------------------------------------
// ICAO 4-letter aircraft type codes → short human name + category.
// Stored in flash (PROGMEM) so it costs us no RAM.
//
// This is an abbreviated list covering the most common civilian and
// general-aviation types seen in North American airspace. The
// complete database is several thousand entries and is maintained
// at the open-source tar1090-db project:
//
//     https://github.com/wiedehopf/tar1090-db
//
// The idea of bundling a code-to-name lookup on-device comes from
// the FlightScnr project by Yash Mulgaonkar — credit on our class
// GitHub readme:
//
//     https://github.com/yashmulgaonkar/FlightScnr
// ================================================================
#ifndef AIRCRAFT_TYPES_H
#define AIRCRAFT_TYPES_H

#include <Arduino.h>

// Categories drive the blip icon style on the radar
enum AircraftCategory : uint8_t {
  CAT_UNKNOWN  = 0,
  CAT_LIGHT    = 1,   // Cessna 172, Cirrus, etc. → small dot + wings
  CAT_MEDIUM   = 2,   // Regional jets, business jets → diamond
  CAT_HEAVY    = 3,   // Airliners → triangle
  CAT_HELI     = 4,   // Rotor symbol
  CAT_MILITARY = 5,   // Special highlight
  CAT_GLIDER   = 6,   // Lightweight outline
};

struct AircraftType {
  const char code[5];     // ICAO 4-letter code (always 4 chars + NUL)
  const char name[24];    // Short human-readable name
  uint8_t    category;    // AircraftCategory
};

// Keep entries SORTED by code — we binary-search them at runtime.
static const AircraftType AIRCRAFT_TYPES[] PROGMEM = {
  // ── Airbus ─────────────────────────────────────────────────
  { "A19N", "Airbus A319neo",        CAT_HEAVY },
  { "A20N", "Airbus A320neo",        CAT_HEAVY },
  { "A21N", "Airbus A321neo",        CAT_HEAVY },
  { "A306", "Airbus A300-600",       CAT_HEAVY },
  { "A30B", "Airbus A300B",          CAT_HEAVY },
  { "A310", "Airbus A310",           CAT_HEAVY },
  { "A318", "Airbus A318",           CAT_HEAVY },
  { "A319", "Airbus A319",           CAT_HEAVY },
  { "A320", "Airbus A320",           CAT_HEAVY },
  { "A321", "Airbus A321",           CAT_HEAVY },
  { "A332", "Airbus A330-200",       CAT_HEAVY },
  { "A333", "Airbus A330-300",       CAT_HEAVY },
  { "A339", "Airbus A330-900neo",    CAT_HEAVY },
  { "A340", "Airbus A340",           CAT_HEAVY },
  { "A359", "Airbus A350-900",       CAT_HEAVY },
  { "A35K", "Airbus A350-1000",      CAT_HEAVY },
  { "A388", "Airbus A380-800",       CAT_HEAVY },

  // ── Boeing ─────────────────────────────────────────────────
  { "B38M", "Boeing 737 MAX 8",      CAT_HEAVY },
  { "B39M", "Boeing 737 MAX 9",      CAT_HEAVY },
  { "B712", "Boeing 717",            CAT_HEAVY },
  { "B721", "Boeing 727-100",        CAT_HEAVY },
  { "B722", "Boeing 727-200",        CAT_HEAVY },
  { "B732", "Boeing 737-200",        CAT_HEAVY },
  { "B733", "Boeing 737-300",        CAT_HEAVY },
  { "B734", "Boeing 737-400",        CAT_HEAVY },
  { "B735", "Boeing 737-500",        CAT_HEAVY },
  { "B736", "Boeing 737-600",        CAT_HEAVY },
  { "B737", "Boeing 737-700",        CAT_HEAVY },
  { "B738", "Boeing 737-800",        CAT_HEAVY },
  { "B739", "Boeing 737-900",        CAT_HEAVY },
  { "B744", "Boeing 747-400",        CAT_HEAVY },
  { "B748", "Boeing 747-8",          CAT_HEAVY },
  { "B752", "Boeing 757-200",        CAT_HEAVY },
  { "B753", "Boeing 757-300",        CAT_HEAVY },
  { "B762", "Boeing 767-200",        CAT_HEAVY },
  { "B763", "Boeing 767-300",        CAT_HEAVY },
  { "B764", "Boeing 767-400",        CAT_HEAVY },
  { "B772", "Boeing 777-200",        CAT_HEAVY },
  { "B773", "Boeing 777-300",        CAT_HEAVY },
  { "B77L", "Boeing 777-200LR",      CAT_HEAVY },
  { "B77W", "Boeing 777-300ER",      CAT_HEAVY },
  { "B788", "Boeing 787-8",          CAT_HEAVY },
  { "B789", "Boeing 787-9",          CAT_HEAVY },
  { "B78X", "Boeing 787-10",         CAT_HEAVY },

  // ── Embraer / Bombardier regional ──────────────────────────
  { "BCS1", "Airbus A220-100",       CAT_HEAVY },
  { "BCS3", "Airbus A220-300",       CAT_HEAVY },
  { "CRJ2", "Bombardier CRJ-200",    CAT_MEDIUM },
  { "CRJ7", "Bombardier CRJ-700",    CAT_MEDIUM },
  { "CRJ9", "Bombardier CRJ-900",    CAT_MEDIUM },
  { "CRJX", "Bombardier CRJ-1000",   CAT_MEDIUM },
  { "E135", "Embraer ERJ-135",       CAT_MEDIUM },
  { "E145", "Embraer ERJ-145",       CAT_MEDIUM },
  { "E170", "Embraer E170",          CAT_MEDIUM },
  { "E190", "Embraer E190",          CAT_MEDIUM },
  { "E195", "Embraer E195",          CAT_MEDIUM },
  { "E75L", "Embraer E175 (long)",   CAT_MEDIUM },
  { "E75S", "Embraer E175 (short)",  CAT_MEDIUM },

  // ── Business jets ──────────────────────────────────────────
  { "BE40", "Beechjet 400",          CAT_MEDIUM },
  { "BE4A", "Beechjet 400A",         CAT_MEDIUM },
  { "C25A", "Cessna Citation CJ2",   CAT_MEDIUM },
  { "C25B", "Cessna Citation CJ3",   CAT_MEDIUM },
  { "C25C", "Cessna Citation CJ4",   CAT_MEDIUM },
  { "C510", "Cessna Citation Mustang", CAT_MEDIUM },
  { "C525", "Cessna CitationJet",    CAT_MEDIUM },
  { "C550", "Cessna Citation II",    CAT_MEDIUM },
  { "C560", "Cessna Citation V",     CAT_MEDIUM },
  { "C56X", "Cessna Citation Excel", CAT_MEDIUM },
  { "C680", "Cessna Citation Sov.",  CAT_MEDIUM },
  { "C700", "Cessna Citation Long.", CAT_MEDIUM },
  { "C750", "Cessna Citation X",     CAT_MEDIUM },
  { "CL30", "Challenger 300",        CAT_MEDIUM },
  { "CL35", "Challenger 350",        CAT_MEDIUM },
  { "CL60", "Challenger 600",        CAT_MEDIUM },
  { "E35L", "Embraer Legacy 600",    CAT_MEDIUM },
  { "E50L", "Embraer Legacy 450",    CAT_MEDIUM },
  { "E55L", "Embraer Legacy 500",    CAT_MEDIUM },
  { "E55P", "Embraer Phenom 300",    CAT_MEDIUM },
  { "F900", "Dassault Falcon 900",   CAT_MEDIUM },
  { "FA20", "Dassault Falcon 20",    CAT_MEDIUM },
  { "FA50", "Dassault Falcon 50",    CAT_MEDIUM },
  { "FA7X", "Dassault Falcon 7X",    CAT_MEDIUM },
  { "FA8X", "Dassault Falcon 8X",    CAT_MEDIUM },
  { "G280", "Gulfstream G280",       CAT_MEDIUM },
  { "GL5T", "Bombardier Global 5000", CAT_MEDIUM },
  { "GLF4", "Gulfstream G450",       CAT_MEDIUM },
  { "GLF5", "Gulfstream G550",       CAT_MEDIUM },
  { "GLF6", "Gulfstream G650",       CAT_MEDIUM },
  { "LJ35", "Learjet 35",            CAT_MEDIUM },
  { "LJ45", "Learjet 45",            CAT_MEDIUM },
  { "LJ60", "Learjet 60",            CAT_MEDIUM },
  { "PC24", "Pilatus PC-24",         CAT_MEDIUM },
  { "SF50", "Cirrus Vision Jet",     CAT_MEDIUM },

  // ── General aviation ───────────────────────────────────────
  { "B190", "Beech 1900",            CAT_LIGHT },
  { "B350", "Beech King Air 350",    CAT_LIGHT },
  { "BE20", "Beech King Air 200",    CAT_LIGHT },
  { "BE23", "Beech Musketeer",       CAT_LIGHT },
  { "BE24", "Beech Sundowner",       CAT_LIGHT },
  { "BE33", "Beech Debonair",        CAT_LIGHT },
  { "BE35", "Beech Bonanza 35",      CAT_LIGHT },
  { "BE36", "Beech Bonanza 36",      CAT_LIGHT },
  { "BE58", "Beech Baron 58",        CAT_LIGHT },
  { "BE9L", "Beech King Air 90",     CAT_LIGHT },
  { "C150", "Cessna 150",            CAT_LIGHT },
  { "C152", "Cessna 152",            CAT_LIGHT },
  { "C172", "Cessna 172 Skyhawk",    CAT_LIGHT },
  { "C180", "Cessna 180 Skywagon",   CAT_LIGHT },
  { "C182", "Cessna 182 Skylane",    CAT_LIGHT },
  { "C206", "Cessna 206",            CAT_LIGHT },
  { "C208", "Cessna Caravan",        CAT_LIGHT },
  { "C210", "Cessna 210",            CAT_LIGHT },
  { "C310", "Cessna 310 Twin",       CAT_LIGHT },
  { "C337", "Cessna 337 Skymaster",  CAT_LIGHT },
  { "C414", "Cessna 414 Chancellor", CAT_LIGHT },
  { "C421", "Cessna 421 Golden Eagle", CAT_LIGHT },
  { "DA40", "Diamond DA40",          CAT_LIGHT },
  { "DA42", "Diamond DA42",          CAT_LIGHT },
  { "DA62", "Diamond DA62",          CAT_LIGHT },
  { "DHC2", "DeHavilland Beaver",    CAT_LIGHT },
  { "DHC3", "DeHavilland Otter",     CAT_LIGHT },
  { "DHC6", "DeHavilland Twin Otter", CAT_LIGHT },
  { "DHC8", "DeHavilland Dash 8",    CAT_LIGHT },
  { "DH8D", "DeHavilland Dash 8 Q400", CAT_LIGHT },
  { "DV20", "Diamond DV20 Katana",   CAT_LIGHT },
  { "E110", "Embraer EMB-110",       CAT_LIGHT },
  { "E120", "Embraer EMB-120",       CAT_LIGHT },
  { "M20C", "Mooney M20C",           CAT_LIGHT },
  { "M20E", "Mooney Super 21",       CAT_LIGHT },
  { "M20J", "Mooney 201",            CAT_LIGHT },
  { "M20K", "Mooney 231",            CAT_LIGHT },
  { "M20M", "Mooney Bravo",          CAT_LIGHT },
  { "M20P", "Mooney M20",            CAT_LIGHT },
  { "M20R", "Mooney Ovation",        CAT_LIGHT },
  { "M20S", "Mooney Eagle",          CAT_LIGHT },
  { "M20T", "Mooney M20",            CAT_LIGHT },
  { "P28A", "Piper Cherokee",        CAT_LIGHT },
  { "P28R", "Piper Arrow",           CAT_LIGHT },
  { "P32R", "Piper Lance/Saratoga",  CAT_LIGHT },
  { "PA18", "Piper Super Cub",       CAT_LIGHT },
  { "PA22", "Piper Tri-Pacer",       CAT_LIGHT },
  { "PA24", "Piper Comanche",        CAT_LIGHT },
  { "PA28", "Piper Cherokee",        CAT_LIGHT },
  { "PA30", "Piper Twin Comanche",   CAT_LIGHT },
  { "PA31", "Piper Navajo",          CAT_LIGHT },
  { "PA32", "Piper Cherokee Six",    CAT_LIGHT },
  { "PA34", "Piper Seneca",          CAT_LIGHT },
  { "PA44", "Piper Seminole",        CAT_LIGHT },
  { "PA46", "Piper Malibu",          CAT_LIGHT },
  { "PA60", "Piper Aerostar",        CAT_LIGHT },
  { "PC12", "Pilatus PC-12",         CAT_LIGHT },
  { "RV4",  "Vans RV-4",             CAT_LIGHT },
  { "RV6",  "Vans RV-6",             CAT_LIGHT },
  { "RV7",  "Vans RV-7",             CAT_LIGHT },
  { "RV8",  "Vans RV-8",             CAT_LIGHT },
  { "RV9",  "Vans RV-9",             CAT_LIGHT },
  { "RV10", "Vans RV-10",            CAT_LIGHT },
  { "RV12", "Vans RV-12",            CAT_LIGHT },
  { "RV14", "Vans RV-14",            CAT_LIGHT },
  { "SR20", "Cirrus SR20",           CAT_LIGHT },
  { "SR22", "Cirrus SR22",           CAT_LIGHT },
  { "TBM7", "TBM 700",               CAT_LIGHT },
  { "TBM8", "TBM 850",               CAT_LIGHT },
  { "TBM9", "TBM 900",               CAT_LIGHT },

  // ── Helicopters ────────────────────────────────────────────
  { "AS50", "Eurocopter AS350",      CAT_HELI },
  { "B06",  "Bell 206",              CAT_HELI },
  { "B407", "Bell 407",              CAT_HELI },
  { "B412", "Bell 412",              CAT_HELI },
  { "B429", "Bell 429",              CAT_HELI },
  { "EC30", "Eurocopter EC130",      CAT_HELI },
  { "EC35", "Eurocopter EC135",      CAT_HELI },
  { "EC45", "Eurocopter EC145",      CAT_HELI },
  { "H60",  "Sikorsky H-60",         CAT_MILITARY },
  { "R22",  "Robinson R22",          CAT_HELI },
  { "R44",  "Robinson R44",          CAT_HELI },
  { "R66",  "Robinson R66",          CAT_HELI },
  { "S92",  "Sikorsky S-92",         CAT_HELI },

  // ── Military / special ─────────────────────────────────────
  { "A10",  "Fairchild A-10",        CAT_MILITARY },
  { "B52",  "Boeing B-52",           CAT_MILITARY },
  { "C130", "Lockheed C-130",        CAT_MILITARY },
  { "C17",  "Boeing C-17",           CAT_MILITARY },
  { "C5",   "Lockheed C-5",          CAT_MILITARY },
  { "F16",  "F-16 Fighting Falcon",  CAT_MILITARY },
  { "F18",  "F-18 Hornet",           CAT_MILITARY },
  { "F22",  "F-22 Raptor",           CAT_MILITARY },
  { "F35",  "F-35 Lightning II",     CAT_MILITARY },
  { "K35R", "KC-135 Stratotanker",   CAT_MILITARY },
  { "P8",   "Boeing P-8 Poseidon",   CAT_MILITARY },

  // ── Cargo / turboprop ──────────────────────────────────────
  { "AT72", "ATR 72",                CAT_MEDIUM },
  { "AT75", "ATR 72-500",            CAT_MEDIUM },
  { "AT76", "ATR 72-600",            CAT_MEDIUM },
  { "DH8A", "Dash 8-100",            CAT_MEDIUM },
  { "DH8B", "Dash 8-200",            CAT_MEDIUM },
  { "DH8C", "Dash 8-300",            CAT_MEDIUM },
  { "DH8D", "Dash 8 Q400",           CAT_MEDIUM },
  { "JS41", "BAe Jetstream 41",      CAT_LIGHT },
  { "MD11", "MD-11",                 CAT_HEAVY },
  { "MD80", "MD-80",                 CAT_HEAVY },
  { "MD82", "MD-82",                 CAT_HEAVY },
  { "MD83", "MD-83",                 CAT_HEAVY },
  { "MD88", "MD-88",                 CAT_HEAVY },
  { "MD90", "MD-90",                 CAT_HEAVY },
  { "SF34", "Saab 340",              CAT_MEDIUM },
  { "SB20", "Saab 2000",             CAT_MEDIUM },
  { "S340", "Saab 340",              CAT_MEDIUM },
  { "SH36", "Shorts 360",            CAT_LIGHT },
};

static const size_t AIRCRAFT_TYPES_COUNT =
    sizeof(AIRCRAFT_TYPES) / sizeof(AIRCRAFT_TYPES[0]);

/**
 * Look up aircraft type information by ICAO type code.
 * Performs a linear search through the PROGMEM aircraft type table.
 * 
 * @param code 4-character ICAO type code (e.g., "B738", "A320", "BE20")
 * @return Pointer to AircraftType struct if found, nullptr if not found
 */
inline const AircraftType* lookupAircraftType(const char* code) {
  // Validate input
  if (!code || strlen(code) == 0) return nullptr;
  
  // Linear search through aircraft type table (stored in PROGMEM)
  // Performance is acceptable for ~150 entries on ESP32-S3
  for (int i = 0; i < AIRCRAFT_TYPES_COUNT; i++) {
    char buf[5];
    // Read type code from PROGMEM into RAM buffer for comparison
    memcpy_P(buf, AIRCRAFT_TYPES[i].code, 5);
    // Compare first 4 characters (ICAO type codes are always 4 chars)
    if (strncmp(code, buf, 4) == 0) {
      return &AIRCRAFT_TYPES[i];
    }
  }
  
  // Type code not found in database
  return nullptr;
}

#endif // AIRCRAFT_TYPES_H