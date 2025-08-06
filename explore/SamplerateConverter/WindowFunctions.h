#include <vector>
#include <complex>
#include <cmath>
#include <numbers>
#include <span>

class WindowFunctions
{
  public:
    template <std::floating_point T>
    static std::vector<T> rectangleWindow(size_t N)
    {
        return std::vector<T>(N, T(1)); // No windowing
    }


    template <std::floating_point T>
    static std::vector<T> hannWindow(size_t N)
    {
        std::vector<T> window(N);
        for (size_t i = 0; i < N; ++i)
        {
            T w = static_cast<T>(i) / static_cast<T>(N - 1);
            window[i] = T(0.5) * (T(1) - std::cos(T(2.0) * std::numbers::pi_v<T> * w));
        }
        return window;
    }

    template <std::floating_point T>
    static std::vector<T> hammingWindow(size_t N)
    {
        std::vector<T> window(N);
        for (size_t i = 0; i < N; ++i)
        {
            T w = static_cast<T>(i) / static_cast<T>(N - 1);
            window[i] = T(0.54) - T(0.46) * std::cos(T(2.0) * std::numbers::pi_v<T> * w);
        }
        return window;
    }

    template <std::floating_point T>
    static std::vector<T> flatTopWindow(size_t N)
    {
        std::vector<T> window(N);
        for (size_t i = 0; i < N; ++i)
        {
            T w = static_cast<T>(i) / static_cast<T>(N - 1);
            window[i] = T(1) - T(1.93) * std::cos(T(2.0) * std::numbers::pi_v<T> * w) +
                        T(1.29) * std::cos(T(4.0) * std::numbers::pi_v<T> * w) -
                        T(0.388) * std::cos(T(6.0) * std::numbers::pi_v<T> * w) +
                        T(0.0322) * std::cos(T(8.0) * std::numbers::pi_v<T> * w);
        }
        return window;
    }
    template <std::floating_point T>
    static void blackmanHarrisWindow(std::span<std::complex<T>> x)
    {
        const T a0 = 0.35875;
        const T a1 = 0.48829;
        const T a2 = 0.14128;
        const T a3 = 0.01168;

        for (size_t i = 0; i < x.size(); i++)
        {
            T w = static_cast<T>(i) / static_cast<T>(x.size() - 1);
            x[i] *= a0 - a1 * std::cos(T(2.0) * std::numbers::pi_v<T> * w) +
                    a2 * std::cos(T(4.0) * std::numbers::pi_v<T> * w) +
                    a3 * std::cos(T(6.0) * std::numbers::pi_v<T> * w);
        }
    }
};
