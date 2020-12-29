#pragma once
#include <MicroBit.h>

namespace maze { namespace melody {

struct Tone
{
  uint16_t hertz;
  uint32_t period;
};
using Melody = std::vector<Tone>;

uint16_t const br =  0 /* break */;
uint16_t const g  =  196 /* hz */;
uint16_t const h  =  247;
uint16_t const c1 =  262;
uint16_t const g1 =  392;
uint16_t const a1 =  440;
uint16_t const h1 =  494;
uint16_t const c2 =  523;

uint32_t const t16 = 50 /* ms */;
uint32_t const t8  = 100;
uint32_t const t4  = 200;
uint32_t const t2  = 400;
uint32_t const t1  = 800;

inline void
play (MicroBit& uBit, Melody const& m)
{
  for (auto const& tone : m)
  {
    if (br != tone.hertz)
      uBit.soundmotor.soundOn (tone.hertz);
    else
      uBit.soundmotor.soundOff ();
    uBit.sleep (tone.period);

    uBit.soundmotor.soundOff ();
    uBit.sleep (30);
  }
}

}}
