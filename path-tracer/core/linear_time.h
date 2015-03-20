#pragma once
#include "accelerator.h"

class Geom;

/**
 * A linear-time (unaccelerated) data structure for looking up ray-object
 * intersections.
 */
class LinearTime : public Accelerator {
  std::vector<const Geom*> objs;

public:
  LinearTime(const std::vector<const Geom*>& o);

  virtual const Geom* intersect(
    const Ray& r,
    Intersection* isectOut
  ) const override;
  virtual bool intersectShadow(const Ray& r, float maxDist) const override;
};
