
#include <MicroBit.h>
#include "maze.h"

MicroBit uBit;

int
main ()
{
  uBit.init ();
  maze::run ();
  release_fiber ();
}