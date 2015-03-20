#include "synced_image.h"
#include <iostream>

SyncedImage::SyncedImage(
  pp::InstanceHandle o,
  pp::Size raw,
  pp::Size screen)
  : owner(o),
    rawImage(owner, PP_IMAGEDATAFORMAT_RGBA_PREMUL, raw, false),
    screenImage(owner, PP_IMAGEDATAFORMAT_RGBA_PREMUL, screen, false),
    screenSize(screen),
    rawImageMutex(),
    screenImageNeedsUpdate(false),
    counter(0) {}

void SyncedImage::AcquireLock() {
  rawImageMutex.lock();
}

void SyncedImage::ReleaseLock() {
  rawImageMutex.unlock();
}

void SyncedImage::Notify() {
  screenImageNeedsUpdate = true;
}

void SyncedImage::IncrementCounter() {
  counter++;
}

pp::ImageData* SyncedImage::GetRawData() {
  return &rawImage;
}

pp::ImageData* SyncedImage::GetScreenData(int* counterOut) {
  if (screenImageNeedsUpdate.load()) {
    AcquireLock();
    screenImage = pp::ImageData(
      owner,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL,
      screenSize,
      false
    );

    int dstHeight = screenImage.size().height();
    int srcHeight = rawImage.size().height();
    int dstWidth = screenImage.size().width();
    int srcWidth = rawImage.size().width();

    for (int y = 0; y < dstHeight; ++y) {
      for (int x = 0; x < dstWidth; ++x) {
        uint32_t* pxAddr = screenImage.GetAddr32(pp::Point(x, y));
        if (y >= srcHeight || x >= srcWidth) {
          *pxAddr = 0xFFFFFFFF; // Opaque white.
        } else {
          *pxAddr = *(rawImage.GetAddr32(pp::Point(x, y)));
        }
      }
    }

    screenImageNeedsUpdate = false;
    ReleaseLock();
  }

  if (counterOut) {
    *counterOut = counter.load();
  }

  return &screenImage;
}

void SyncedImage::SetScreenSize(pp::Size newSize) {
  AcquireLock();
  screenSize = newSize;
  Notify();
  ReleaseLock();
}
