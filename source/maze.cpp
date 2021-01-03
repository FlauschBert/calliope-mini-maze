
#include <vector>
#include <tuple>
#include <string>

#include "maze.h"
#include "melody.h"
#include "images.h"

#include <MicroBit.h>

extern MicroBit uBit;

// TODO:
// - play victory melody
// - show floor and ceiling hole for up down

namespace
{
uint8_t constexpr sDI = 255;
uint8_t constexpr sRGB = 25;
float constexpr sSlowestPulse = 1200.f /*ms*/;
float constexpr sMinPulseResolution = 50.f /*ms*/;

// Array row, column (y, x)
// 9: blocking wall
// 8: non-blocking secret wall, yellow rgb led
// 0: normal floor
// 1: traps: death
// 2: dark floor, no rgb led, display brightness 1
// 3: twister: random direction
//
// visibility:
// 0 - 4: no wall visible
// 5 - 9: wall visible
//
using Row = std::vector<uint8_t>;
using Maze = std::vector<Row>;
Maze const sMaze = {
// 0  1  2  3  4  5  6  7  8  9  A  B
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}, // 0
  {9, 0, 0, 2, 1, 0, 8, 3, 9, 9, 9, 9}, // 1
  {9, 0, 9, 9, 9, 1, 9, 0, 9, 9, 9, 9}, // 2
  {9, 0, 9, 0, 9, 2, 9, 3, 9, 9, 9, 9}, // 3
  {9, 0, 9, 0, 0, 0, 9, 0, 8, 0, 9, 9}, // 4
  {9, 0, 9, 0, 0, 9, 9, 8, 9, 0, 9, 9}, // 5
  {9, 0, 0, 0, 0, 9, 9, 0, 9, 0, 9, 9}, // 6
  {9, 9, 0, 0, 9, 8, 0, 2, 0, 8, 9, 9}, // 7
  {9, 9, 0, 0, 9, 8, 9, 0, 9, 0, 0, 9}, // 8
  {9, 9, 3, 9, 3, 0, 9, 9, 9, 0, 0, 9}, // 9
  {9, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 9}, // A
  {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9}  // B
};

// North means looking to row zero
enum Direction {
  North = 0, East, South, West
};

enum Mode {
  Floor = 0, Map
};

struct Game {
  int32_t sx = 5;
  int32_t sy = 9;
  Direction sd = West;
  int32_t ex = 5;
  int32_t ey = 1;
} const sGame;

using Colorf = std::tuple<float, float, float>;
struct Floor {
  uint8_t brightness = 15;
  
  // distance pulse rgb handling - defaults to largest distance
  Colorf rgb = std::make_tuple (0.f, 0.f, 0.f);
  float pulse = 1.f;
  unsigned long lastPulseStart = 0ul;
} sFloor;

struct Player {
  int32_t px = 1;
  int32_t py = 1;
  Direction di = North;

  // view mode: floor or map
  Mode mode = Floor;
} sPlayer;

struct MazePart {
  // visibility
  bool front = false;
  bool left = false;
  bool right = false;
  // mobility
  bool blocked = false;
};

MicroBitImage sScreen;
MicroBitImage sMap;
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

void
updateFloor (struct Floor& floor, Player& player, Maze const &maze)
{
  // default brightness
  floor.brightness = 15;

  switch (maze [player.py][player.px])
  {
  case 2:
    floor.brightness = 1;
    break;
  case 3:
    Direction newDi;
    do
    {
      newDi = static_cast<Direction> (microbit_random (4));
    }
    while (newDi == player.di);
    player.di = newDi;
    break;
  }

  switch (player.di)
  {
  case North:
    // Earth - green / coins
    floor.rgb = std::make_tuple (0.f, 1.f, 0.f);
    break;
  case East:
    // Air - yellow / swords
    floor.rgb = std::make_tuple (1.f, 1.f, 0.f);
    break;
  case South:
    // Fire - red / wands
    floor.rgb = std::make_tuple (1.f, 0.f, 0.f);
    break;
  case West:
    // Water - blue / cups
    floor.rgb = std::make_tuple (0.f, 0.f, 1.f);
    break;
  }
  
  // relative time between two rgb led pulses: the shorter the nearer to the goal, can be 0.f
  floor.pulse = getDistanceNorm (maze, player);
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

void
updatePulse (struct Floor& floor, float const pulseResolution)
{
  uint8_t r, g, b;
  auto const maxPulse = floor.pulse * sSlowestPulse;

  if (maxPulse < pulseResolution)
  {
    floor.lastPulseStart = uBit.systemTime ();
    r = g = b = sRGB;
  }
  else
  {
    auto const time = uBit.systemTime ();
    if (static_cast<float> (time - floor.lastPulseStart) > maxPulse)
      floor.lastPulseStart = time;

    auto const intensity = (1.f - floor.pulse) * static_cast<float> (time - floor.lastPulseStart) / maxPulse;
    std::tie (r, g, b) = getScaled (floor.rgb, intensity, sRGB);
  }

  uBit.rgb.setColour (r, g, b, 0);
  uBit.sleep (pulseResolution /*ms*/);
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
  MicroBitImage& image,
  Maze const& maze,
  Player const& player)
{
  image.clear ();
  auto const part = getMazePart (maze, player);
  setLeft (image, part.left);
  setMiddle (image, part.front);
  setRight (image, part.right);
}

void
updateVisuals (MicroBitImage& image,
               struct Floor& floor,
               Player& player,
               Maze const& maze)
{
  updateFloor (floor, player, maze);
  updateImage (
    image,
    maze,
    player
  );

  uBit.display.setBrightness (floor.brightness);
  uBit.display.print (image);
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
  if (Floor != sPlayer.mode)
    return;

  sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di - 1));

  playTurnAroundSound ();
  updateVisuals (sScreen, sFloor, sPlayer, sMaze);
}

void right (MicroBitEvent)
{
  if (Floor != sPlayer.mode)
    return;

  sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di + 1));

  playTurnAroundSound ();
  updateVisuals (sScreen, sFloor, sPlayer, sMaze);
}

void forward (MicroBitEvent)
{
  if (Floor != sPlayer.mode)
    return;

  auto const mazePart = getMazePart (sMaze, sPlayer);
  if (mazePart.blocked)
  {
    tilt (sScreen);
    return;
  }

  move (sPlayer);

  playForwardSound ();
  updateVisuals (sScreen, sFloor, sPlayer, sMaze);
}

MicroBitImage
getMap (Maze const& maze)
{
  auto const mazeSizeX = maze.front ().size ();
  auto const mazeSizeY = maze.size ();

  // Clear new map with 1
  // increase by 1 in each direction to show walls on the display outside
  // the play area to not confuse the player
  std::vector<uint8_t> const data ((mazeSizeX + 2) * (mazeSizeY * 2), sDI);
  MicroBitImage map (
    mazeSizeX + 2,
    mazeSizeY + 2,
    data.data ()
  );

  for (size_t x = 0; x < maze.front ().size (); ++x)
    for (size_t y = 0; y < maze.size (); ++y)
      if (maze [y][x] < 5)
        map.setPixelValue (x + 1, y + 1, 0);

  return map;
}

void printMap (MicroBitImage const& map, Player const& player)
{
  // Do not use copy constructor or copy operator here
  // No copy is generated in this case and the original
  // image is shifted
  MicroBitImage tmp = const_cast<MicroBitImage*> (&map)->clone ();

  // shift left and up
  // +2 because of border,
  // -2 because of display center
  // -1 because sMaze is one smaller in each direction than map
  tmp.shiftLeft (player.px + 2 - 2 - 1);
  tmp.shiftUp (player.py + 2 - 2 - 1);

  uBit.display.print (tmp);
}

void toggleMap (MicroBitEvent)
{
  if (Floor == sPlayer.mode)
  {
    sPlayer.mode = Map;
    printMap (sMap, sPlayer);
  }
  else
  {
    sPlayer.mode = Floor;
    uBit.display.print (sScreen);
  }
}

void init ()
{
  // button A == left
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_A,
    MICROBIT_BUTTON_EVT_CLICK,
    left,
    MESSAGE_BUS_LISTENER_DROP_IF_BUSY
  );
  // button B == left
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_B,
    MICROBIT_BUTTON_EVT_CLICK,
    right,
    MESSAGE_BUS_LISTENER_DROP_IF_BUSY
  );
  // button AB == forward
  uBit.messageBus.listen (
    MICROBIT_ID_BUTTON_AB,
    MICROBIT_BUTTON_EVT_CLICK,
    forward,
    MESSAGE_BUS_LISTENER_DROP_IF_BUSY
  );
  // map mode
  uBit.messageBus.listen (
    MICROBIT_ID_GESTURE,
    MICROBIT_ACCELEROMETER_EVT_SHAKE,
    toggleMap,
    MESSAGE_BUS_LISTENER_DROP_IF_BUSY
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
  // map mode
  uBit.messageBus.ignore (
    MICROBIT_ID_GESTURE,
    MICROBIT_ACCELEROMETER_EVT_SHAKE,
    toggleMap
  );
}

// 0: no end
// 1: victory
// 2: death
uint8_t
isTheEnd (Game const& game, Player const& player, Maze const& maze)
{
  auto const field = maze [player.py][player.px];
  if (1 == field)
    return 2;
  if (game.ex == player.px &&
      game.ey == player.py)
    return 1;
  return 0;
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
    animationDone,
    MESSAGE_BUS_LISTENER_IMMEDIATE
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
    { c1, t1 },
    { c1, t1 },
    { br, t8 },
    { c1, t4 },
    { g,  t4 },
    { c1, t4 },
    { g1, t1 },
    { br, t4 },
    { c2, t1 },
    { br, t8 },
    { c2, t2 },
    { h1, t2 },
    { br, t8 },
    { g1, t2 },
    { c1, t1 }
  };

  play (uBit, m);
}


}

namespace maze
{

bool titleActive = false;

MicroBitImage createImage (std::string const& text)
{
  MicroBitImage img (text.size () * 5, 5);
  int x = 0;
  for (auto const c : text)
  {
    img.print (c, x, 0);
    x += 5;
  }

  return img;
}

MicroBitImage createBackground (uint8_t const brightness)
{
  std::vector<uint8_t> const data (5 * 5, brightness);
  return MicroBitImage (5, 5, data.data ());
}

void showTitle ()
{
  auto textImage = createImage (" MiniMaze0.9 ");
  uBit.display.setDisplayMode (DISPLAY_MODE_GREYSCALE);

  uint8_t brightness = 0;
  for (int stride = 0; stride < textImage.getWidth (); ++stride)
  {
    textImage.shiftLeft (1);

    for (int i = 0; i < 2; ++i)
    {
      auto background = createBackground (brightness);
      background.paste (textImage, 0, 0, 1);

      uBit.display.print (background);
      uBit.sleep (62);

      brightness += 1;
      if (brightness > 15)
        brightness = 0;
    }
  }

  titleActive = false;
}

void run ()
{
  uBit.rgb.off ();

  titleActive = true;
  create_fiber (showTitle);
  playTitleMelody ();
  while (titleActive)
    uBit.sleep (50);

  // Initialize player position and direction
  sPlayer.px = sGame.sx;
  sPlayer.py = sGame.sy;
  sPlayer.di = sGame.sd;

  // Initialize floor led pulsing
  sFloor.lastPulseStart = uBit.systemTime ();

  sScreen = MicroBitImage (5, 5);
  sMap = getMap (sMaze);

  uBit.display.setDisplayMode (DISPLAY_MODE_BLACK_AND_WHITE);
  updateVisuals (sScreen, sFloor, sPlayer, sMaze);

  init ();

  uint8_t end;
  do
  {
    updatePulse (sFloor, sMinPulseResolution);
    end = isTheEnd (sGame, sPlayer, sMaze);
  }
  while (0 == end);

  uBit.sleep (500 /*ms*/);
  uBit.rgb.off ();

  uBit.display.clear ();
  uBit.display.print ((1 == end) ? *image (ImageSmiley) : *image (ImageSadly));
  uBit.sleep (800 /*ms*/);

  switch (end)
  {
  case 1:
    startScrolling (sAnimationActive, "Victory!", 150);
    break;
  case 2:
    startScrolling (sAnimationActive, "Trap!", 150);
    break;
  }
  waitForScrolling (sAnimationActive);

  uBit.display.clear ();
  cleanup ();
}

}
