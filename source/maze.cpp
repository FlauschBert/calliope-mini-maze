
#include <vector>
#include <tuple>
#include <string>

#include "maze.h"
#include "melody.h"

#include <MicroBit.h>

extern MicroBit uBit;

// TODO:
// - rein pusten: map zentriert auf aktuelle position
// - play victory melody
// - use setDisplayMode(DISPLAY_MODE_GREYSCALE) and show walls interior with
//   less intensity

namespace
{
uint8_t constexpr sDI = 255u;
uint8_t constexpr sRGB = 30u;
float constexpr sSlowestPulse = 1200.f /*ms*/;
float constexpr sMinPulseResolution = 20.f /*ms*/;

// Array row, column (y, x)
// 9: blocking wall
// 8: non-blocking secret wall
// 0: normal floor
// 1: secret floor, color
//
// visibility:
// 0 - 4: no wall visible
// 5 - 9: wall visible
//
using Row = std::vector<int8_t>;
using Maze = std::vector<Row>;
Maze const sMaze = {
// 0  1  2  3  4  5  6  7  8  9  A  B
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, // 0
  {9, 0, 0, 0, 0, 0, 8, 8, 9, 9, 9, 9}, // 1
  {9, 0, 9, 9, 9, 0, 9, 1, 9, 9, 9, 9}, // 2
  {9, 0, 9, 0, 9, 0, 9, 1, 9, 9, 9, 9}, // 3
  {9, 0, 9, 0, 0, 0, 9, 1, 9, 9, 9, 9}, // 4
  {9, 0, 9, 0, 0, 9, 9, 1, 9, 9, 9, 9}, // 5
  {9, 0, 0, 0, 0, 9, 9, 1, 9, 9, 9, 9}, // 6
  {9, 0, 0, 0, 9, 9, 9, 1, 9, 9, 9, 9}, // 7
  {9, 9, 0, 0, 0, 9, 9, 1, 9, 9, 9, 9}, // 8
  {9, 9, 0, 9, 0, 0, 8, 1, 9, 9, 9, 9}, // 9
  {9, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9}, // A
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}  // B
};

// North means looking to row zero
enum Direction {
  North = 0, East, South, West
};

struct Game {
  int32_t sx = 5;
  int32_t sy = 9;
  Direction sd = West;
  int32_t ex = 5;
  int32_t ey = 1;
} const sGame;

using Colorf = std::tuple<float, float, float>;
struct Player {
  int32_t px = 1;
  int32_t py = 1;
  Direction di = North;

  // distance pulse rgb handling - defaults to largest distance
  Colorf rgb = std::make_tuple (0.f, 0.f, 0.f);
  float pulse = 1.f;
  unsigned long lastPulseStart = 0ul;
} sPlayer;

struct MazePart {
  // visibility
  bool front = false;
  bool left = false;
  bool right = false;
  // movability
  bool blocked = false;
};

MicroBitImage sScreen;
bool sAnimationActive = false;

MazePart
getMazePart (Maze const &maze, Player const &player)
{
  MazePart part;
  switch (player.di)
  {
  case North:
    part.blocked = 8 < maze[player.py - 1][player.px + 0];
    part.front = 4 < maze[player.py - 1][player.px + 0];
    part.left = 4 < maze[player.py + 0][player.px - 1];
    part.right = 4 < maze[player.py + 0][player.px + 1];
    break;
  case South:
    part.blocked = 8 < maze[player.py + 1][player.px + 0];
    part.front = 4 < maze[player.py + 1][player.px + 0];
    part.left = 4 < maze[player.py + 0][player.px + 1];
    part.right = 4 < maze[player.py + 0][player.px - 1];
    break;
  case West:
    part.blocked = 8 < maze[player.py + 0][player.px - 1];
    part.front = 4 < maze[player.py + 0][player.px - 1];
    part.left = 4 < maze[player.py + 1][player.px + 0];
    part.right = 4 < maze[player.py - 1][player.px + 0];
    break;
  case East:
    part.blocked = 8 < maze[player.py + 0][player.px + 1];
    part.front = 4 < maze[player.py + 0][player.px + 1];
    part.left = 4 < maze[player.py - 1][player.px + 0];
    part.right = 4 < maze[player.py + 1][player.px + 0];
    break;
  default:
    part.blocked = true;
    break;
  }
  return part;
}

// manhattan distance between player position and goal normalized by maximum manhattan distance
float getDistanceNorm (Maze const &maze, Player const &player)
{
  auto const max = (maze.front ().size () - 2) + (maze.size () - 2);
  auto const manhattan = std::abs (sGame.ex - player.px) + std::abs (sGame.ey - player.py);

  return static_cast<float> (manhattan) / static_cast<float> (max);
}

Colorf getFloorColor (Maze const &maze, Player const &player)
{
  auto const floor = maze[player.py][player.px];
  switch (floor)
  {
  case 0:
    return std::make_tuple (0.f, 1.f, 1.f);
  case 1:
  case 8:
    return std::make_tuple (1.f, 1.f, 0.f);
  default:
    return std::make_tuple (1.f, 0.f, 0.f);
  }
}

using Color = std::tuple<uint8_t, uint8_t, uint8_t>;

Color getScaled (Colorf const &color, float const intensity, uint8_t const rgbMax)
{
  return std::make_tuple (
    static_cast<uint8_t> (std::get<0> (color) * intensity * static_cast<float> (rgbMax)),
    static_cast<uint8_t> (std::get<1> (color) * intensity * static_cast<float> (rgbMax)),
    static_cast<uint8_t> (std::get<2> (color) * intensity * static_cast<float> (rgbMax))
  );
}

void updateDistanceColor (Player &player, Maze const &maze)
{
  player.rgb = getFloorColor (maze, player);
  // relative time between two rgb led pulses: the shorter the nearer to the goal, can be 0.f
  player.pulse = getDistanceNorm (maze, player);
}

void updatePulse (Player &player)
{
  uint8_t r, g, b;
  auto const maxPulse = player.pulse * sSlowestPulse;

  if (maxPulse < sMinPulseResolution)
  {
    player.lastPulseStart = uBit.systemTime ();
    r = g = b = sRGB;
  }
  else
  {
    auto const time = uBit.systemTime ();
    if (static_cast<float> (time - player.lastPulseStart) > maxPulse)
      player.lastPulseStart = time;

    auto const intensity = (1.f - player.pulse) * static_cast<float> (time - player.lastPulseStart) / maxPulse;
    std::tie (r, g, b) = getScaled (player.rgb, intensity, sRGB);
  }

  uBit.rgb.setColour (r, g, b, 0);
}

void move (Player &player)
{
  switch (player.di)
  {
  case North:
    player.py -= 1;
    break;
  case South:
    player.py += 1;
    break;
  case West:
    player.px -= 1;
    break;
  case East:
    player.px += 1;
    break;
  }
}

void tilt (MicroBitImage &image)
{
  for (int i = 0; i < 3; ++i)
  {
    uBit.sleep (25);
    uBit.display.clear ();
    uBit.sleep (25);
    uBit.display.print (image);
  }
}

void setLeft (MicroBitImage &img, bool fill)
{
  img.setPixelValue (0, 0, sDI);
  img.setPixelValue (0, 4, sDI);
  if (fill)
    for (int i = 1; i < 4; ++i)
      img.setPixelValue (0, i, sDI);
}

void setRight (MicroBitImage &img, bool fill)
{
  img.setPixelValue (4, 0, sDI);
  img.setPixelValue (4, 4, sDI);
  if (fill)
    for (int i = 1; i < 4; ++i)
      img.setPixelValue (4, i, sDI);
}

void setMiddle (MicroBitImage &img, bool fill)
{
  for (int i = 1; i < 4; ++i)
  {
    img.setPixelValue (i, 1, sDI);
    img.setPixelValue (i, 3, sDI);
  }
  img.setPixelValue (1, 2, sDI);
  img.setPixelValue (3, 2, sDI);
  if (fill)
    img.setPixelValue (2, 2, sDI);
}

void updateImage (
  MicroBitImage &image,
  Maze const &maze,
  Player const &player)
{
  image.clear ();
  auto const part = getMazePart (maze, player);
  setLeft (image, part.left);
  setMiddle (image, part.front);
  setRight (image, part.right);
}

int modulo4 (int v)
{
  int constexpr mod = 4;
  if (v >= mod)
    return v - mod;
  else if (v < 0)
    return v + mod;
  else
    return v;
}

void playForwardSound ()
{
  uBit.soundmotor.soundOn (70);
  uBit.sleep (50);
  uBit.soundmotor.soundOff ();
  uBit.sleep (50);
  uBit.soundmotor.soundOn (70);
  uBit.sleep (50);
  uBit.soundmotor.soundOff ();
}

void playTurnAroundSound ()
{
  uBit.soundmotor.soundOn (100);
  uBit.sleep (150);
  uBit.soundmotor.soundOff ();
}

void left (MicroBitEvent)
{
  sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di - 1));

  updateImage (
    sScreen,
    sMaze,
    sPlayer
  );

  playTurnAroundSound ();
  uBit.display.print (sScreen);
  updateDistanceColor (sPlayer, sMaze);
}

void right (MicroBitEvent)
{
  sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di + 1));

  updateImage (
    sScreen,
    sMaze,
    sPlayer
  );

  playTurnAroundSound ();
  uBit.display.print (sScreen);
  updateDistanceColor (sPlayer, sMaze);
}

void forward (MicroBitEvent)
{
  auto const mazePart = getMazePart (sMaze, sPlayer);
  if (mazePart.blocked)
  {
    tilt (sScreen);
    return;
  }

  move (sPlayer);

  updateImage (
    sScreen,
    sMaze,
    sPlayer
  );

  playForwardSound ();
  uBit.display.print (sScreen);
  updateDistanceColor (sPlayer, sMaze);
}

void init ()
{
  // button A == left
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_A,
    MICROBIT_BUTTON_EVT_CLICK,
    left
  );
  // button B == left
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_B,
    MICROBIT_BUTTON_EVT_CLICK,
    right
  );
  // button AB == forward
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_AB,
    MICROBIT_BUTTON_EVT_CLICK,
    forward
  );
}

void cleanup ()
{
  // button A == left
  uBit.messageBus.ignore (
    MICROBIT_ID_BUTTON_A,
    MICROBIT_BUTTON_EVT_CLICK,
    left
  );
  // button B == left
  uBit.messageBus.ignore (
    MICROBIT_ID_BUTTON_B,
    MICROBIT_BUTTON_EVT_CLICK,
    right
  );
  // button AB == forward
  uBit.messageBus.ignore (
    MICROBIT_ID_BUTTON_AB,
    MICROBIT_BUTTON_EVT_CLICK,
    forward
  );
}

bool isTheEnd (Game const& game, Player const& player)
{
  return game.ex == player.px &&
         game.ey == player.py;
}

void startScrolling (bool& active, std::string const& text, int const delay)
{
  active = true;
  uBit.display.setBrightness (30);
  uBit.display.scrollAsync (text.c_str (), delay);
}

void animationDone (MicroBitEvent)
{
  sAnimationActive = false;
}

void waitForScrolling (bool& active)
{
  // display scrolling done
  uBit.messageBus.listen (
    MICROBIT_ID_DISPLAY,
    MICROBIT_DISPLAY_EVT_ANIMATION_COMPLETE,
    animationDone
  );

  while (active)
  {
    uBit.sleep (100);
  }

  uBit.messageBus.ignore (
    MICROBIT_ID_DISPLAY,
    MICROBIT_DISPLAY_EVT_ANIMATION_COMPLETE,
    animationDone
  );
}

void playTitleMelody ()
{
  using namespace maze::melody;

  Melody m = {
    { c1,  t1 },
    { c1,  t1 },
    { br,  t8 },
    { c1,  t4 },
    { g,   t4 },
    { c1,  t4 },
    { g1,  t1 },
    { br,  t4 },
    { c2,  t1 },
    { br,  t8 },
    { c2,  t2 },
    { h1,  t2 },
    { br,  t8 },
    { g1,  t2 },
    { a1,  t1 }
  };

  play (uBit, m);
}

}

namespace maze
{

void run ()
{
  startScrolling (sAnimationActive, "MiniMaze0.7", 100);
  playTitleMelody ();
  waitForScrolling (sAnimationActive);

  // Initialize player position and direction
  sPlayer.px = sGame.sx;
  sPlayer.py = sGame.sy;
  sPlayer.di = sGame.sd;
  sPlayer.lastPulseStart = uBit.systemTime ();

  sScreen = MicroBitImage (5, 5);

  updateImage (
    sScreen,
    sMaze,
    sPlayer
  );

  uBit.display.setBrightness (15);
  uBit.display.print (sScreen);
  updateDistanceColor (sPlayer, sMaze);

  init ();

  while (!isTheEnd(sGame, sPlayer))
  {
    updatePulse (sPlayer);
    uBit.sleep (sMinPulseResolution /*ms*/);
  }

  uBit.sleep (500 /*ms*/);
  uBit.rgb.off ();

  startScrolling (sAnimationActive, "TheEnd!", 200);
  waitForScrolling (sAnimationActive);

  uBit.display.clear ();

  cleanup ();
}

}
