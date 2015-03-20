#include "image.h"

Image::Image(long ww, long hh, long spp, float fw)
  : currentIteration(boost::extents[hh][ww][spp]),
    rawData(boost::extents[hh][ww]),
    lock(), counter(0),
    w(ww), h(hh), samplesPerPixel(spp), filterWidth(fw)
{
  // Clear the data array.
  for (long y = 0; y < h; ++y) {
    for (long x = 0; x < w; ++x) {
      rawData[y][x] = Vec4(0, 0, 0, 0);
    }
  }
}

uint32_t Image::MakeRgbaColor(float r, float g, float b) {
  uint8_t rr = uint8_t(std::max(0.0f, std::min(1.0f, r)) * 255.0f);
  uint8_t gg = uint8_t(std::max(0.0f, std::min(1.0f, g)) * 255.0f);
  uint8_t bb = uint8_t(std::max(0.0f, std::min(1.0f, b)) * 255.0f);
  return (255 << 24) | (bb << 16) | (gg << 8) | rr;
}

void Image::setSample(
  long x,
  long y,
  float ptX,
  float ptY,
  long idx,
  const Vec& color
) {
  Sample& s = currentIteration[y][x][idx];
  s.position = Vec2(ptX, ptY);
  s.color = color;
}

void Image::commitSamples() {
  lock.lock();

  for (const auto& row : currentIteration) {
    for (const auto& col : row) {
      for (const Sample& s : col) {
        float posX = s.position.x();
        float posY = s.position.y();

        long minX = math::clampAny(long(ceilf(posX - filterWidth)), 0l, w - 1);
        long maxX = math::clampAny(long(floorf(posX + filterWidth)), 0l, w - 1);
        long minY = math::clampAny(long(ceilf(posY - filterWidth)), 0l, h - 1);
        long maxY = math::clampAny(long(floorf(posY + filterWidth)), 0l, h - 1);

        for (long yy = minY; yy <= maxY; ++yy) {
          for (long xx = minX; xx <= maxX; ++xx) {
            Vec4& px = rawData[yy][xx];

            float weight = math::mitchellFilter(
              posX - float(xx),
              posY - float(yy),
              filterWidth
            );

            px[0] += s.color[0] * weight;
            px[1] += s.color[1] * weight;
            px[2] += s.color[2] * weight;
            px[3] += weight;
          }
        }
      }
    }
  }

  lock.unlock();

  counter++;
}

void Image::writeToNaClImage(pp::ImageData* buffer, int* counterOut) {
  lock.lock();

  int dstWidth = buffer->size().width();
  int dstHeight = buffer->size().height();

  for (int y = 0; y < dstHeight; ++y) {
    for (int x = 0; x != dstWidth; ++x) {
      uint32_t* pxAddr = buffer->GetAddr32(pp::Point(x, y));
      if (x >= w || y >= h) {
        *pxAddr = 0xFF000000;
      } else {
        Vec4& px = rawData[y][x];
        *pxAddr = MakeRgbaColor(
          px.x() / px.w(),
          px.y() / px.w(),
          px.z() / px.w()
        );
      }
    }
  }

  lock.unlock();

  if (counterOut) {
    *counterOut = counter.load();
  }
}
