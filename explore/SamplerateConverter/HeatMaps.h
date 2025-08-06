#pragma once

#include <vector>
#include <string>
#include <tuple>
#include <algorithm>
#include <map>

class HeatMaps
{
  public:
    static constexpr float minDbVal{-120.f};

    enum class Colormap
    {
        Viridis,
        Inferno,
        Magma,
        Plasma,
        Cividis,
        Turbo,
        Jet,
        Cubehelix,
        Cubehelix2,
        Parula
    };

    static std::vector<std::tuple<unsigned char, unsigned char, unsigned char>> getColormapLookup(
        Colormap colormap, size_t lookUpSize = 1024)
    {
        const std::vector<float> dbWayPoints{minDbVal, -92.f, -60.f, -36.f, -18.f, 0.f};

        const std::vector<std::vector<std::string>> colormapPalettes{
            {"#440154", "#3b528b", "#21918c", "#5ec962", "#fde725", "#fde725"}, // Viridis (good for colorblindness)
            {"#000004", "#721f81", "#b63679", "#fc8961", "#fcffa4", "#fcffa4"}, // Inferno
            {"#000004", "#6a00a8", "#cc4678", "#f79d4a", "#fbd524", "#fbd524"}, // Magma
            {"#0d0887", "#7e03a8", "#cc4778", "#f89540", "#f0f921", "#f0f921"}, // Plasma
            {"#00204c", "#2b4b7f", "#4a79a4", "#76a1aa", "#a9d18f", "#ffea46"}, // Cividis
            {"#30123b", "#3a70a2", "#73dc5c", "#f3ff20", "#f18f1c", "#981b1e"}, // Turbo
            {"#000080", "#0000ff", "#00ffff", "#ffff00", "#ff0000", "#800000"}, // Jet
            {"#000000", "#424242", "#808080", "#bebebe", "#eeeeee", "#ffffff"}, // Cubehelix
            {"#000000", "#424242", "#808080", "#bebebe", "#eeeeee", "#ffffff"}, // Cubehelix2
            {"#352a87", "#0f8cff", "#52ef92", "#f8e622", "#ec0f1a", "#9a2515"}  // Parula
        };

        const auto& palette = colormapPalettes[static_cast<size_t>(colormap)];

        std::vector<std::tuple<unsigned char, unsigned char, unsigned char>> colormapLookup(lookUpSize);

        for (size_t i = 0; i < lookUpSize; ++i)
        {
            const float value =
                std::clamp(minDbVal + i / static_cast<float>(lookUpSize - 1) * (0.f - minDbVal), minDbVal, 0.0f);
            colormapLookup[i] = interpolateColor(palette, dbWayPoints, value);
        }

        return colormapLookup;
    }

  private:
    static std::tuple<unsigned char, unsigned char, unsigned char> interpolateColor(
        const std::vector<std::string>& palette, const std::vector<float>& dbWayPoints, float value)
    {
        auto lower = std::lower_bound(dbWayPoints.begin(), dbWayPoints.end(), value);
        if (lower == dbWayPoints.end())
        {
            return hexToRGB(palette.back());
        }
        if (lower == dbWayPoints.begin())
        {
            return hexToRGB(palette.front());
        }
        const size_t idx = std::distance(dbWayPoints.begin(), lower);
        const auto upper = lower--;
        const auto fraction = (value - *lower) / (*upper - *lower);

        const auto [r1, g1, b1] = hexToRGB(palette[idx - 1]);
        const auto [r2, g2, b2] = hexToRGB(palette[idx]);

        return {static_cast<unsigned char>(r1 + fraction * (r2 - r1)),
                static_cast<unsigned char>(g1 + fraction * (g2 - g1)),
                static_cast<unsigned char>(b1 + fraction * (b2 - b1))};
    }

    static std::tuple<unsigned char, unsigned char, unsigned char> hexToRGB(const std::string& hex)
    {
        const unsigned rgb = std::stoul(hex.substr(1), nullptr, 16);
        return {static_cast<unsigned char>((rgb >> 16) & 0xFF), static_cast<unsigned char>((rgb >> 8) & 0xFF),
                static_cast<unsigned char>(rgb & 0xFF)};
    }
};
