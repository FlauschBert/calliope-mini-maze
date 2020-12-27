
#include <vector>

#include "maze.h"
#include "utils.h"

#include <MicroBit.h>
extern MicroBit uBit;

namespace {
  constexpr uint8_t sDI = 255u;

  // Array row, column (y, x)
  using Row = std::vector<int8_t>;
  using Maze = std::vector<Row>;
  Maze const sMaze = {
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // 0
    { 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1 }, // 1
    { 1, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1 }, // 2
    { 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1 }, // 3
    { 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1 }, // 4
    { 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1 }, // 5
    { 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }, // 6
    { 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 }, // 7
    { 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }, // 8
    { 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1 }, // 9
    { 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1 }, // A
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }  // B
  };

  // North means looking to row zero
  enum Direction { North = 0, East, South, West };

  struct Game {
    int32_t sx = 5;
    int32_t sy = 9;
    Direction sd = West;
    int32_t ex = 6;
    int32_t ey = 1;
  } const sGame;
  
  struct Player {
    int32_t px = 1;
    int32_t py = 1;
    Direction di = North;
  } sPlayer;
  
  struct MazePart {
    bool front = false;
    bool left = false;
    bool right = false;
    bool error = false;
  };

  MicroBitImage sScreen;

  MazePart
  getMazePart (Maze const& maze, Player const& player)
  {
    MazePart part;
    switch (player.di) {
      case North:
        part.front = 0 < maze [player.py - 1][player.px + 0];
        part.left = 0 < maze [player.py + 0][player.px - 1];
        part.right = 0 < maze [player.py + 0][player.px + 1];
        break;
      case South:
        part.front = 0 < maze [player.py + 1][player.px + 0];
        part.left = 0 < maze [player.py + 0][player.px + 1];
        part.right = 0 < maze [player.py + 0][player.px - 1];
        break;
      case West:
        part.front = 0 < maze [player.py + 0][player.px - 1];
        part.left = 0 < maze [player.py + 1][player.px + 0];
        part.right = 0 < maze [player.py - 1][player.px + 0];
        break;
      case East:
        part.front = 0 < maze [player.py + 0][player.px + 1];
        part.left = 0 < maze [player.py - 1][player.px + 0];
        part.right = 0 < maze [player.py + 1][player.px + 0];
        break;
      default:
        part.error = true;
        break;
    }
    return part;
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

  void animateMove ()
  {
    for (uint8_t i = 0; i < 25; i += 5) {
      uBit.sleep(5);
      uBit.rgb.setColour(0, i, 0, i);
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
  }

  void forward (MicroBitEvent)
  {
    auto const mazePart = getMazePart (sMaze, sPlayer);
    if (mazePart.front) {
      tilt (sScreen);
      return;
    }

    move (sPlayer);

    updateImage (
      sScreen,
      sMaze,
      sPlayer
    );

    animateMove ();
    uBit.display.print (sScreen);
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

  uBit.rgb.setColour(0,25,0,25);

  init ();

  while (true) {
    uBit.sleep(100);
  }

  uBit.rgb.off();
  uBit.display.clear();

  cleanup();
}

}
