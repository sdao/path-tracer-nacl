#pragma once

#include <mutex>
#include "ppapi/cpp/image_data.h"

class SyncedImage {
  pp::InstanceHandle owner;
  pp::ImageData rawImage;
  pp::ImageData screenImage;
  pp::Size screenSize;
  std::mutex rawImageMutex;
  std::atomic<bool> screenImageNeedsUpdate;
  std::atomic<int> counter;

public:
  SyncedImage(pp::InstanceHandle o, pp::Size raw, pp::Size screen);
  void AcquireLock();
  void ReleaseLock();
  void Notify();
  void IncrementCounter();
  pp::ImageData* GetRawData();
  pp::ImageData* GetScreenData(int* counterOut);
  void SetScreenSize(pp::Size newSize);
};
