#include "linear_time.h"
#include "core.h"
#include "geom.h"

LinearTime::LinearTime(const std::vector<const Geom*>& o) : objs(o) {}

const Geom* LinearTime::intersect(
  const Ray& r,
  Intersection* isectOut
) const {
  Intersection isect;
  const Geom* isectGeom = nullptr;

  for (const Geom* g : objs) {
    Intersection cur;
    if (g->intersect(r, &cur) && cur.distance < isect.distance) {
      isect = cur;
      isectGeom = g;
    }
  }

  if (isectGeom) {
    *isectOut = isect;
    return isectGeom;
  }

  return nullptr;
}

bool LinearTime::intersectShadow(const Ray& r, float maxDist) const {
  for (const Geom* g : objs) {
    if (g->intersectShadow(r, maxDist)) {
      return true;
    }
  }

  return false;
}
