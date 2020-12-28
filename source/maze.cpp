
#include <vector>
#include <tuple>

#include "maze.h"

#include <MicroBit.h>
extern MicroBit uBit;

// TODO:
// - rein pusten: map zentriert auf aktuelle position
// - pulsieren rgb led in abhängigkeit zur ziel distanz
// - different forward and turn around sounds

namespace {
  uint8_t constexpr sDI = 255u;
  uint8_t constexpr sRGB = 25u;

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
  //  0  1  2  3  4  5  6  7  8  9  A  B
    { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 }, // 0
    { 9, 0, 0, 0, 0, 0, 8, 8, 9, 9, 9, 9 }, // 1
    { 9, 0, 9, 9, 9, 0, 9, 1, 9, 9, 9, 9 }, // 2
    { 9, 0, 9, 0, 9, 0, 9, 1, 9, 9, 9, 9 }, // 3
    { 9, 0, 9, 0, 0, 0, 9, 1, 9, 9, 9, 9 }, // 4
    { 9, 0, 9, 0, 0, 9, 9, 1, 9, 9, 9, 9 }, // 5
    { 9, 0, 0, 0, 0, 9, 9, 1, 9, 9, 9, 9 }, // 6
    { 9, 0, 0, 0, 9, 9, 9, 1, 9, 9, 9, 9 }, // 7
    { 9, 9, 0, 0, 0, 9, 9, 1, 9, 9, 9, 9 }, // 8
    { 9, 9, 0, 9, 0, 0, 8, 1, 9, 9, 9, 9 }, // 9
    { 9, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9 }, // A
    { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 }  // B
  };

  // North means looking to row zero
  enum Direction { North = 0, East, South, West };

  struct Game {
    int32_t sx = 5;
    int32_t sy = 9;
    Direction sd = West;
    int32_t ex = 5;
    int32_t ey = 1;
  } const sGame;
  
  struct Player {
    int32_t px = 1;
    int32_t py = 1;
    Direction di = North;
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

  MazePart
  getMazePart (Maze const& maze, Player const& player)
  {
    MazePart part;
    switch (player.di) {
      case North:
        part.blocked = 8 < maze [player.py - 1][player.px + 0];
        part.front = 4 < maze [player.py - 1][player.px + 0];
        part.left = 4 < maze [player.py + 0][player.px - 1];
        part.right = 4 < maze [player.py + 0][player.px + 1];
        break;
      case South:
        part.blocked = 8 < maze [player.py + 1][player.px + 0];
        part.front = 4 < maze [player.py + 1][player.px + 0];
        part.left = 4 < maze [player.py + 0][player.px + 1];
        part.right = 4 < maze [player.py + 0][player.px - 1];
        break;
      case West:
        part.blocked = 8 < maze [player.py + 0][player.px - 1];
        part.front = 4 < maze [player.py + 0][player.px - 1];
        part.left = 4 < maze [player.py + 1][player.px + 0];
        part.right = 4 < maze [player.py - 1][player.px + 0];
        break;
      case East:
        part.blocked = 8 < maze [player.py + 0][player.px + 1];
        part.front = 4 < maze [player.py + 0][player.px + 1];
        part.left = 4 < maze [player.py - 1][player.px + 0];
        part.right = 4 < maze [player.py + 1][player.px + 0];
        break;
      default:
        part.blocked = true;
        break;
    }
    return part;
  }

  using Color = std::tuple<uint8_t,uint8_t,uint8_t>;
  Color getFloorColor (Maze const& maze, Player const& player)
  {
    auto const floor = maze [player.py][player.px];
    switch (floor)
    {
      case 0:
        return std::make_tuple(0,sRGB,sRGB);
      case 1:
      case 8:
        return std::make_tuple(sRGB,sRGB,0);
      default:
        return std::make_tuple(sRGB,0,0);
    }
  }

  void setFloorColor (Maze const& maze, Player const& player)
  {
    uint8_t r, g, b;
    std::tie (r, g, b) = getFloorColor (maze, player);
    uBit.rgb.setColour(r, g, b, sRGB);
  }

  void move (Player& player)
  {
    switch (player.di) {
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

  void tilt (MicroBitImage& image)
  {
    for (int i = 0; i < 3; ++i)
    {
      uBit.sleep (25);
      uBit.display.clear ();
      uBit.sleep (25);
      uBit.display.print (image);
    }
  }

  void setLeft (MicroBitImage& img, bool fill)
  {
    img.setPixelValue (0, 0, sDI);
    img.setPixelValue (0, 4, sDI);
    if (fill)
      for (int i = 1; i < 4; ++i)
        img.setPixelValue (0, i, sDI);
  }
  void setRight (MicroBitImage& img, bool fill)
  {
    img.setPixelValue (4, 0, sDI);
    img.setPixelValue (4, 4, sDI);
    if (fill)
      for (int i = 1; i < 4; ++i)
        img.setPixelValue (4, i, sDI);
  }
  void setMiddle (MicroBitImage& img, bool fill)
  {
    for (int i = 1; i < 4; ++i) {
      img.setPixelValue(i, 1, sDI);
      img.setPixelValue(i, 3, sDI);
    }
    img.setPixelValue(1, 2, sDI);
    img.setPixelValue(3, 2, sDI);
    if (fill)
      img.setPixelValue (2, 2, sDI);
  }

  void updateImage (
    MicroBitImage& image,
    Maze const& maze,
    Player const& player)
  {
    image.clear ();
    auto const part = getMazePart(maze, player);
    setLeft(image, part.left);
    setMiddle(image, part.front);
    setRight(image,part.right);
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

  void left (MicroBitEvent)
  {
    sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di - 1));

    updateImage (
      sScreen,
      sMaze,
      sPlayer
    );

    uBit.display.print (sScreen);
    setFloorColor(sMaze, sPlayer);
  }

  void right (MicroBitEvent)
  {
    sPlayer.di = static_cast<Direction> (modulo4 (sPlayer.di + 1));

    updateImage (
      sScreen,
      sMaze,
      sPlayer
    );

    uBit.display.print (sScreen);
    setFloorColor(sMaze, sPlayer);
  }

  void forward (MicroBitEvent)
  {
    auto const mazePart = getMazePart (sMaze, sPlayer);
    if (mazePart.blocked) {
      tilt (sScreen);
      return;
    }

    move (sPlayer);

    updateImage (
      sScreen,
      sMaze,
      sPlayer
    );

    uBit.display.print (sScreen);
    setFloorColor(sMaze, sPlayer);
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
}

namespace maze {

void run() {
  // Initialize player position and direction
  sPlayer.px = sGame.sx;
  sPlayer.py = sGame.sy;
  sPlayer.di = sGame.sd;

  sScreen = MicroBitImage (5,5);

  updateImage (
    sScreen,
    sMaze,
    sPlayer
  );

  uBit.display.setBrightness(15);
  uBit.display.print (sScreen);

  uBit.rgb.setColour(0,sRGB,0,sRGB);

  init ();

  while (true) {
    uBit.sleep(100);
  }

  uBit.rgb.off();
  uBit.display.clear();

  cleanup();
}

}
