#include "camera.h"
#include <iostream>
#include <chrono>
#include "light.h"

using std::max;
using std::min;
namespace chrono = std::chrono;

Camera::Camera(
  const Transform& xform,
  const std::vector<const Geom*>& objs,
  long ww,
  long hh,
  float fov,
  float len,
  float fStop
) : accel(objs), focalLength(len),
    lensRadius((len / fStop) * 0.5f), // Diameter = focalLength / fStop.
    camToWorldXform(xform),
    masterRng(), rowSeeds(size_t(hh)), img(ww, hh), iters(0),
    pool(MAX_THREADS)
{
  // Calculate ray-tracing vectors.
  float halfFocalPlaneUp;
  float halfFocalPlaneRight;

  if (img.w > img.h) {
    halfFocalPlaneUp = focalLength * tanf(0.5f * fov);
    halfFocalPlaneRight = halfFocalPlaneUp * float(img.w) / float(img.h);
  } else {
    halfFocalPlaneRight = focalLength * tanf(0.5f * fov);
    halfFocalPlaneUp = halfFocalPlaneRight * float(img.h) / float(img.w);
  }

  focalPlaneUp = -2.0f * halfFocalPlaneUp;
  focalPlaneRight = 2.0f * halfFocalPlaneRight;
  focalPlaneOrigin = Vec(-halfFocalPlaneRight, halfFocalPlaneUp, -focalLength);

  // Refine emitters so we can compute direct illumination.
  for (const Geom* g : objs) {
    if (g->light) {
      g->refine(emitters);
    }
  }
}

Camera::Camera(const Node& n)
  : Camera(math::rotationThenTranslation(
             n.getFloat("rotateAngle"),
             n.getVec("rotateAxis"),
             n.getVec("translate")
           ),
           n.getGeometryList("objects"),
           n.getInt("width"), n.getInt("height"),
           n.getFloat("fov"), n.getFloat("focalLength"),
           n.getFloat("fStop")) {}

void Camera::renderOnce(SyncedImage* buffer) {
  // Increment iteration count and begin timer.
  iters++;
  std::cout << "Iteration " << iters;
  chrono::steady_clock::time_point startTime = chrono::steady_clock::now();

  // Seed the per-row RNGs.
  for (long y = 0; y < img.h; ++y) {
    rowSeeds[size_t(y)] = masterRng.nextUnsigned();
  }

  // Trace paths in parallel.
  pool.Dispatch(img.h, &Camera::renderWorkFunc, this);

  // Process and write the output file at the end of this iteration.
  img.commitSamples();
  img.writeToNaClImage(buffer);

  // End timer.
  chrono::steady_clock::time_point endTime = chrono::steady_clock::now();
  chrono::duration<float> runTime =
    chrono::duration_cast<chrono::duration<float>>(endTime - startTime);
  std::cout << " [" << runTime.count() << " seconds]\n";
}

void Camera::renderMultiple(SyncedImage* buffer, int iterations) {
  if (iterations < 0) {
    // Run forever.
    std::cout << "Rendering infinitely, press Ctrl-c to terminate program\n";

    while (true) {
      renderOnce(buffer);
    }
  } else {
    // Run finite iterations.
    std::cout << "Rendering " << iterations << " iterations\n";

    for (int i = 0; i < iterations; ++i) {
      renderOnce(buffer);
    }
  }
}

Vec Camera::trace(
  LightRay r,
  Randomness& rng
) const {
  Vec L(0, 0, 0);
  bool didDirectIlluminate = false;

  for (int depth = 0; ; ++depth) {
    // Do Russian Roulette if this path is "old".
    if (depth >= RUSSIAN_ROULETTE_DEPTH_1 || r.isBlack()) {
      float rv = rng.nextUnitFloat();

      float probLive;
      if (depth >= RUSSIAN_ROULETTE_DEPTH_2) {
        // More aggressive ray killing when ray is very old.
        probLive = math::clampedLerp(0.25f, 0.75f, r.luminance());
      } else {
        // Less aggressive ray killing.
        probLive = math::clampedLerp(0.25f, 1.00f, r.luminance());
      }

      if (rv < probLive) {
        // The ray lives (more energy = more likely to live).
        // Increase its energy to balance out probabilities.
        r.color = r.color / probLive;
      } else {
        // The ray dies.
        break;
      }
    }

    // Bounce ray and kill if nothing hit.
    Intersection isect;
    const Geom* g = accel.intersect(r, &isect);
    if (!g) {
      // End path in empty space.
      break;
    }

    // Check for lighting.
    if (g->light && !didDirectIlluminate) {
      // Accumulate emission normally.
      L += r.color.cwiseProduct(g->light->emit(r, isect));
    } else if (g->light && didDirectIlluminate) {
      // Skip emission accumulation because it was accumulated already
      // in a direct lighting calculation. We don't want to double-count.
    }

    // Check for scattering (reflection/transmission).
    if (!g->mat) {
      // Cannot continue path without a material.
      break;
    } else if (g->mat && !g->mat->shouldDirectIlluminate()) {
      // Continue path normally.
      r = g->mat->scatter(rng, r, isect);
      didDirectIlluminate = false;
    } else if (g->mat && g->mat->shouldDirectIlluminate()) {
#ifndef NO_DIRECT_ILLUM
      // Sample direct lighting and then continue path.
      L += r.color.cwiseProduct(
        uniformSampleOneLight(rng, r, isect, g->mat)
      );
      r = g->mat->scatter(rng, r, isect);
      didDirectIlluminate = true;
#else
      // Continue path normally.
      r = g->mat->scatter(rng, r, isect);
      didDirectIlluminate = false;
#endif
    }
  }

  L[0] = math::clamp(L[0], 0.0f, BIASED_RADIANCE_CLAMPING);
  L[1] = math::clamp(L[1], 0.0f, BIASED_RADIANCE_CLAMPING);
  L[2] = math::clamp(L[2], 0.0f, BIASED_RADIANCE_CLAMPING);

  return L;
}

Vec Camera::uniformSampleOneLight(
  Randomness& rng,
  const LightRay& incoming,
  const Intersection& isect,
  const Material* mat
) const {
  size_t numLights = emitters.size();
  if (numLights == 0) {
    return Vec(0, 0, 0);
  }

  size_t lightIdx = size_t(floorf(rng.nextUnitFloat() * numLights));
  const Geom* emitter = emitters[min(lightIdx, numLights - 1)];
  const AreaLight* areaLight = emitter->light;

  // P[this light] = 1 / numLights, so 1 / P[this light] = numLights.
  return float(numLights) * areaLight->directIlluminate(
    rng, incoming, isect, mat, emitter, &accel
  );
}

void Camera::renderWorkFunc(int task_index, void* data) {
  Camera* c = reinterpret_cast<Camera*>(data);
  const long y = task_index;

  Randomness rng(c->rowSeeds[size_t(y)]);
  for (long x = 0; x < c->img.w; ++x) {
    for (long samp = 0; samp < c->img.samplesPerPixel; ++samp) {
      float offsetY = rng.nextFloat(-c->img.filterWidth, c->img.filterWidth);
      float offsetX = rng.nextFloat(-c->img.filterWidth, c->img.filterWidth);

      float posY = float(y) + offsetY;
      float posX = float(x) + offsetX;

      float fracY = posY / (float(c->img.h) - 1.0f);
      float fracX = posX / (float(c->img.w) - 1.0f);

      // Implement depth of field by jittering the eye.
      Vec offset(c->focalPlaneRight * fracX, c->focalPlaneUp * fracY, 0);
      Vec lookAt = c->focalPlaneOrigin + offset;

      Vec eye(0, 0, 0);
      math::areaSampleDisk(rng, &eye[0], &eye[1]);
      eye = eye * c->lensRadius;

      Vec eyeWorld = c->camToWorldXform * eye;
      Vec lookAtWorld = c->camToWorldXform * lookAt;
      Vec dir = (lookAtWorld - eyeWorld).normalized();

      Vec L = c->trace(LightRay(eyeWorld, dir), rng);
      c->img.setSample(x, y, posX, posY, samp, L);
    }
  }
}

pp::Size Camera::GetSize() const {
  return pp::Size(img.w, img.h);
}
