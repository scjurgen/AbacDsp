#include <complex>
#include <concepts>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numbers>
#include <span>
#include <valarray>
#include <vector>

#include "HeatMaps.h"
#include "WindowFunctions.h"

/**
 * @brief Simple Fast Fourier Transform (FFT) for real-valued data.
 *
 * Compute the magnitude spectrum of real-valued input data using the FFT.
 *
 * Usage:
 * @code
 * std::vector<float> input(4096);  // Input size must be a power of 2
 * std::vector<float> magnitudes;
 * auto window = WindowFunctions::hannWindow<float>(input.size());  // Generate a Hann window
 * BasicFFT::realDataToMagnitude(input, magnitudes, window);  // Apply Hann window
 * @endcode
 *
 * Restrictions:
 * - Input size must be a power of 2 and contains only real values.
 * - Supported data types: float, double, long double.
 *
 * Window Functions:
 * - The window functions are in WindowFunctions.h
 * - Available window functions include:
 *   - hannWindow
 *   - hammingWindow
 *   - flatTopWindow
 *   - rectangleWindow (no effect)
 *
 * Custom window functions can be generated using the `WindowFunctions` class and passed to `realDataToMagnitude`.
 * The output spectrum size is N/2, representing frequencies up to the Nyquist frequency.
 */
class BasicFFT
{
  public:
    template <std::floating_point T, typename Container>
    static void realDataToMagnitude(const Container& in, std::vector<T>& magnitudes, const std::vector<T>& window)
        requires std::ranges::contiguous_range<Container> && std::is_same_v<std::ranges::range_value_t<Container>, T>
    {
        auto N = std::ranges::size(in);
        if (!((N != 0) && ((N & (N - 1)) == 0))) // check power of 2
        {
            std::cerr << " FFT " << __FUNCTION__ << " container size must be a multiple of 2^n" << std::endl;
            return;
        }
        std::vector<std::complex<T>> inData(N);
        std::transform(std::ranges::begin(in), std::ranges::end(in), window.begin(), inData.begin(),
                       [](T val, T win) { return std::complex<T>(val * win, 0); });

        fft<T>(inData);
        magnitudes.resize(N / 2);
        std::transform(inData.begin(), inData.begin() + N / 2, magnitudes.begin(),
                       [N](const auto& c) { return std::abs(c) / (N / 2); });
    }

  private:
    // Cooley–Tukey FFT (in-place, divide-and-conquer)
    template <std::floating_point T, typename Container>
    static void fft(Container& x)
        requires std::ranges::contiguous_range<Container> &&
                 std::is_same_v<std::ranges::range_value_t<Container>, std::complex<T>>
    {
        const size_t N = std::ranges::size(x);
        if (N <= 1)
            return;

        std::vector<std::complex<T>> even(N / 2), odd(N / 2);
        for (size_t i = 0; i < N / 2; ++i)
        {
            even[i] = x[2 * i];
            odd[i] = x[2 * i + 1];
        }

        fft<T>(even);
        fft<T>(odd);

        for (size_t k = 0; k < N / 2; ++k)
        {
            T angle = -T(2) * std::numbers::pi_v<T> * static_cast<T>(k) / static_cast<T>(N);
            std::complex<T> t = std::polar(T(1), angle) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }
};


class Spectrogram
{
  public:
    using Colormap = HeatMaps::Colormap;
    struct RGB
    {
        unsigned char r, g, b;
    };
    Spectrogram() = default;

    void generate(const std::string& filename, const std::vector<float>& signal, const Colormap colormap,
                  const size_t width = 16384, const double advance = 0.5, const size_t height = 1024)
    {
        const size_t stepSize = static_cast<size_t>(width * advance);
        const size_t numSlices = (signal.size() - width) / stepSize;
        const auto window = WindowFunctions::hannWindow<float>(width);
        std::vector<std::vector<float>> spectrogramData(numSlices, std::vector<float>(height, HeatMaps::minDbVal));

        for (size_t i = 0; i < numSlices; ++i)
        {
            const std::vector<float> slice(signal.begin() + i * stepSize, signal.begin() + i * stepSize + width);
            std::vector<float> magnitudeSignal(width / 2); // Only need half of the FFT output
            BasicFFT::realDataToMagnitude(slice, magnitudeSignal, window);

            for (size_t j = 0; j < height; ++j)
            {
                size_t startIdx = j * (width / 2) / height;
                size_t endIdx = (j + 1) * (width / 2) / height;
                float sum = 0.0f;
                size_t count = 0;

                for (size_t k = startIdx; k < endIdx; ++k)
                {
                    sum += magnitudeSignal[k];
                    count++;
                }

                float average = (count > 0) ? sum / count : 0.0f;
                spectrogramData[i][j] = 20.0f * std::log10(std::max(average, 1e-6f));
            }
        }

        colormapLookup = HeatMaps::getColormapLookup(colormap);
        prepareImage(spectrogramData);
        applyXGrid(200, 10, 1.6, 1.2);
        applyYGrid(256, 8, 1.6, 1.2);
        saveP6Image(filename + ".ppm");
    }


    void combine(const std::string& filename, const std::vector<std::vector<float>>& top,
                 const std::vector<std::vector<float>>& bottom, const Colormap colormap, const size_t height = 1024)
    {
        if (top.empty() || bottom.empty() || top[0].empty() || bottom[0].empty())
        {
            throw std::runtime_error("Input vectors cannot be empty");
        }

        const size_t numSlices = top.size();
        if (bottom.size() != numSlices)
        {
            throw std::runtime_error("Top and bottom vectors must have the same number of slices");
        }

        const size_t topHeight = height / 2;
        const size_t bottomHeight = height - topHeight;
        const size_t topWidth = top[0].size();
        const size_t bottomWidth = bottom[0].size();

        std::vector<std::vector<float>> spectrogramData(numSlices, std::vector<float>(height, HeatMaps::minDbVal));

        for (size_t i = 0; i < numSlices; ++i)
        {
            // Process top part
            for (size_t j = 0; j < topHeight; ++j)
            {
                size_t startIdx = j * topWidth / topHeight;
                size_t endIdx = (j + 1) * topWidth / topHeight;
                float sum = 0.0f;
                size_t count = 0;

                for (size_t k = startIdx; k < endIdx; ++k)
                {
                    sum += top[i][k];
                    count++;
                }

                float average = (count > 0) ? sum / count : 0.0f;
                spectrogramData[i][j] = 20.0f * std::log10(std::max(average, 1e-6f));
            }

            // Process bottom part
            for (size_t j = 0; j < bottomHeight; ++j)
            {
                size_t startIdx = j * bottomWidth / bottomHeight;
                size_t endIdx = (j + 1) * bottomWidth / bottomHeight;
                float sum = 0.0f;
                size_t count = 0;

                for (size_t k = startIdx; k < endIdx; ++k)
                {
                    sum += bottom[i][k];
                    count++;
                }

                float average = (count > 0) ? sum / count : 0.0f;
                spectrogramData[i][topHeight + j] = 20.0f * std::log10(std::max(average, 1e-6f));
            }
        }

        colormapLookup = HeatMaps::getColormapLookup(colormap);
        prepareImage(spectrogramData);
        applyXGrid(500, 10, 0.8, 0.7);
        applyYGrid(128, 4, 0.8, 0.7);
        saveP6Image(filename + ".ppm");
    }

    void saveP6Image(const std::string& filename)
    {
        std::ofstream file(filename, std::ios::binary);

        file << "P6\n" << width << " " << height << "\n255\n";

        for (const auto& row : intermediateImage)
        {
            for (const auto& pixel : row)
            {
                file.write(reinterpret_cast<const char*>(&pixel), 3);
            }
        }
        file.close();
    }
    void prepareImage(const std::vector<std::vector<float>>& data)
    {
        width = data.size();
        height = data[0].size();
        intermediateImage.resize(height, std::vector<RGB>(width));

        for (size_t y = 0; y < height; ++y)
        {
            for (size_t x = 0; x < width; ++x)
            {
                const float value =
                    std::clamp((data[x][height - y - 1] - HeatMaps::minDbVal) / (0.f - HeatMaps::minDbVal), 0.0f, 1.0f);
                const auto [r, g, b] = colormapLookup[static_cast<int>(value * static_cast<float>(lookUpSize - 1))];
                intermediateImage[y][x] = {r, g, b};
            }
        }
    }
    void applyXGrid(int mainGridWidth, int subGridDivision, float mainFactor, float subFactor)
    {
        applyGrid(mainGridWidth, subGridDivision, mainFactor, subFactor, [](size_t x, size_t) { return x; });
    }

    void applyYGrid(int mainGridHeight, int subGridDivision, float mainFactor, float subFactor)
    {
        applyGrid(mainGridHeight, subGridDivision, mainFactor, subFactor, [](size_t, size_t y) { return y; });
    }

  private:
    template <typename CoordGetter>
    void applyGrid(int mainGridSize, int subGridDivision, float mainFactor, float subFactor, CoordGetter coordGetter)
    {
        int subGridSize = mainGridSize / subGridDivision;

        for (size_t y = 0; y < height; ++y)
        {
            for (size_t x = 0; x < width; ++x)
            {
                size_t coord = coordGetter(x, y);
                if (coord % mainGridSize == 0)
                {
                    intermediateImage[y][x] = applyColorFactor(intermediateImage[y][x], mainFactor);
                }
                else if (coord % subGridSize == 0)
                {
                    intermediateImage[y][x] = applyColorFactor(intermediateImage[y][x], subFactor);
                }
            }
        }
    }

    RGB applyColorFactor(const RGB& color, float factor)
    {
        return {static_cast<unsigned char>(std::clamp(color.r * factor, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.g * factor, 0.0f, 255.0f)),
                static_cast<unsigned char>(std::clamp(color.b * factor, 0.0f, 255.0f))};
    }
    std::vector<std::vector<RGB>> intermediateImage;
    size_t width, height;
    std::vector<std::tuple<unsigned char, unsigned char, unsigned char>> colormapLookup;
    static constexpr size_t lookUpSize{1024};
};
