// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <atomic>
#include <thread>
#include <sstream>

#include "ppapi/c/ppb_image_data.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/utility/completion_callback_factory.h"

#include "core/scene.h"
#include "core/camera.h"
#include "core/image.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

namespace {

const std::string kSceneDescription = R"(
  {
    "lights" : {
      "sunLight" : {
        "type" : "area",
        "color" : "10 10 10"
      },
      "skyLight" : {
        "type" : "area",
        "color" : "0.3 0.4 0.5"
      }
    },
    "materials" : {
      "ground" : {
        "type" : "lambert",
        "albedo" : "0.4 0.3 0.2"
      },
      "sky" : {
        "type" : "lambert",
        "albedo" : "0.4 0.5 0.6"
      },
      "dielectric" : {
        "type" : "dielectric",
        "ior" : 1.5,
        "color" : "1 1 1"
      },
      "dielectric-g" : {
        "type" : "dielectric",
        "ior" : 1.5,
        "color" : "0.8 1 0.9"
      },
      "dielectric-b" : {
        "type" : "dielectric",
        "ior" : 1.5,
        "color" : "0.8 0.9 1"
      },
      "dielectric-r" : {
        "type" : "dielectric",
        "ior" : 1.5,
        "color" : "1 0.8 0.9"
      }
    },
    "geometry" : {
      "worldSphere" : {
        "type" : "sphere",
        "mat" : "sky",
        "light" : "skyLight",
        "origin" : "0 0 0",
        "radius" : 2000.0,
        "inverted" : true
      },
      "bottom" : {
        "type" : "disc",
        "mat" : "ground",
        "light" : "",
        "origin" : "0 -20 -0",
        "normal" : "0 1 0",
        "radiusOuter" : 2001.0,
        "radiusInner" : 0.0
      },
      "lightSource" : {
        "type" : "sphere",
        "mat" : "",
        "light" : "sunLight",
        "origin" : "0 2500 0",
        "radius" : 1000.0,
        "inverted" : false
      },
      "sphere1" : {
        "type" : "sphere",
        "mat" : "dielectric",
        "light" : "",
        "origin" : "16 -10 -22",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere2" : {
        "type" : "sphere",
        "mat" : "dielectric-r",
        "light" : "",
        "origin" : "2 -10 -40",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere3" : {
        "type" : "sphere",
        "mat" : "dielectric-g",
        "light" : "",
        "origin" : "-12 -10 -58",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere4" : {
        "type" : "sphere",
        "mat" : "dielectric",
        "light" : "",
        "origin" : "-26 -10 -76",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere5" : {
        "type" : "sphere",
        "mat" : "dielectric-g",
        "light" : "",
        "origin" : "-40 -10 -94",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere6" : {
        "type" : "sphere",
        "mat" : "dielectric",
        "light" : "",
        "origin" : "-54 -10 -112",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere7" : {
        "type" : "sphere",
        "mat" : "dielectric-b",
        "light" : "",
        "origin" : "-68 -10 -130",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere8" : {
        "type" : "sphere",
        "mat" : "dielectric",
        "light" : "",
        "origin" : "-82 -10 -148",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere9" : {
        "type" : "sphere",
        "mat" : "dielectric-b",
        "light" : "",
        "origin" : "-96 -10 -166",
        "radius" : 10.0,
        "inverted" : false
      },
      "sphere10" : {
        "type" : "sphere",
        "mat" : "dielectric",
        "light" : "",
        "origin" : "-110 -10 -184",
        "radius" : 10.0,
        "inverted" : false
      }
    },
    "cameras" : {
      "default" : {
        "type" : "persp",
        "translate" : "-2 8 30",
        "rotateAngle" :  -0.26180,
        "rotateAxis" : "1 0 0",
        "objects" : [
          "worldSphere", "bottom", "lightSource", "sphere1", "sphere2",
          "sphere3", "sphere4", "sphere5", "sphere6", "sphere7", "sphere8",
          "sphere9", "sphere10"
        ],
        "width" : 512,
        "height" : 384,
        "fov" : 0.78540,
        "focalLength" : 88.0,
        "fStop" : 16.0
      }
    }
  }
)";

}  // namespace

class Graphics2DInstance : public pp::Instance {
 public:
  explicit Graphics2DInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        core_image_(NULL),
        device_scale_(1.0f) {}

  ~Graphics2DInstance() {}

  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) {
    pthread_t t;
    pthread_create(
      &t,
      NULL,
      &Graphics2DInstance::BackgroundRenderThread,
      this
    );

    return true;
  }

  static void* BackgroundRenderThread(void* data) {
    std::stringstream ss;
    ss << kSceneDescription;
    Scene scene(ss);

    Graphics2DInstance* g2d = reinterpret_cast<Graphics2DInstance*>(data);
    g2d->core_image_ = scene.defaultCamera()->getImagePtr();
    scene.defaultCamera()->renderMultiple(g2d->needs_paint_, -1);
    return NULL;
  }

  virtual void DidChangeView(const pp::View& view) {
    device_scale_ = view.GetDeviceScale();
    pp::Size new_size = pp::Size(view.GetRect().width() * device_scale_,
                                 view.GetRect().height() * device_scale_);

    // No new context if the view size is the same.
    if (size_ == new_size) {
      return;
    }

    if (!CreateContext(new_size))
      return;

    // When flush_context_ is null, it means there is no Flush callback in
    // flight. This may have happened if the context was not created
    // successfully, or if this is the first call to DidChangeView (when the
    // module first starts). In either case, start the main loop.
    if (flush_context_.is_null())
      MainLoop(0);
  }

 private:
  bool CreateContext(const pp::Size& new_size) {
    const bool kIsAlwaysOpaque = true;
    context_ = pp::Graphics2D(this, new_size, kIsAlwaysOpaque);
    // Call SetScale before BindGraphics so the image is scaled correctly on
    // HiDPI displays.
    context_.SetScale(1.0f / device_scale_);
    if (!BindGraphics(context_)) {
      fprintf(stderr, "Unable to bind 2d context!\n");
      context_ = pp::Graphics2D();
      return false;
    }

    // Force update screen image buffer.
    size_ = new_size;
    needs_paint_ = true;

    return true;
  }

  void Paint() {
    if (!needs_paint_.load()) {
      return;
    }

    needs_paint_ = false;

    if (core_image_) {
      pp::ImageData img(this, PP_IMAGEDATAFORMAT_RGBA_PREMUL, size_, false);
      int counter;
      core_image_->writeToNaClImage(&img, &counter);
      context_.ReplaceContents(&img);
      PostMessage(pp::Var(counter));
    }
  }

  void MainLoop(int32_t) {
    if (context_.is_null()) {
      // The current Graphics2D context is null, so updating and rendering is
      // pointless. Set flush_context_ to null as well, so if we get another
      // DidChangeView call, the main loop is started again.
      flush_context_ = context_;
      return;
    }

    Paint();

    // Store a reference to the context that is being flushed; this ensures
    // the callback is called, even if context_ changes before the flush
    // completes.
    flush_context_ = context_;
    context_.Flush(
        callback_factory_.NewCallback(&Graphics2DInstance::MainLoop));
  }

  pp::CompletionCallbackFactory<Graphics2DInstance> callback_factory_;
  pp::Graphics2D context_;
  pp::Graphics2D flush_context_;
  pp::Size size_;
  Image* core_image_;
  std::atomic<bool> needs_paint_;
  float device_scale_;
};

class Graphics2DModule : public pp::Module {
 public:
  Graphics2DModule() : pp::Module() {}
  virtual ~Graphics2DModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Graphics2DInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new Graphics2DModule(); }
}  // namespace pp
