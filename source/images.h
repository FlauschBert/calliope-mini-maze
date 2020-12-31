#pragma once

#include "MicroBitImage.h"

namespace maze
{

enum Image {
  ImageSmiley,
  ImageSadly,
  ImageHeart,
  ImageArrowLeft,
  ImageArrowRight,
  ImageArrowLeftRight,
  ImageFull,
  ImageDot,
  ImageSmallRect,
  ImageLargeRect,
  ImageDoubleRow,
  ImageTick,
  ImageRock,
  ImageScissors,
  ImageWell,
  ImageFlash,
  ImageWave,
  ImageMultiplier
};

MicroBitImage *image (Image index);

}
