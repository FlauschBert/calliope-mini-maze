/**
 * This is a simple template for use with Calliope mini.
 *
 * @copyright (c) Calliope gGmbH.
 * @author Matthias L. Jugel.
 *
 * Licensed under the Apache License 2.0
 */

#include <MicroBit.h>
#include "maze.h"

MicroBit uBit;

int
main()
{
  uBit.init();
  maze::run ();
}