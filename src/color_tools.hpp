#pragma once
#include "image.hpp"
#include <array>
namespace paint {
enum class PickerSpace { RGB, OKHSL };
enum class SampleMode { Exact, Average, Representative };
struct ColorCoordinates {
    double hue = 0, saturation = 0, level = 0;
};
Color picker_color(PickerSpace space, ColorCoordinates coordinates);
ColorCoordinates picker_coordinates(PickerSpace space, Color color);
Color sample_color(const Image& image, Point point, SampleMode mode, int radius = 2);
// Fixed, deterministic Lloyd centroids in the selected picker domain. Selection
// uses nearest OKLab distance: every point belongs to one perceptual Voronoi cell.
class ColorMosaic {
  public:
    explicit ColorMosaic(PickerSpace space);
    Color nearest(Color color) const;
    const std::array<Color, 96>& colors() const;

  private:
    std::array<Color, 96> colors_{};
    std::array<Lab, 96> centers_{};
};
} // namespace paint
