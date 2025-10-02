#ifndef __MATHS_HPP__
#define __MATHS_HPP__

#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "../PicoDefs.hpp"
#include <Arduino.h>


constexpr float LinearMap(float value, float n, float m)
{
    // Clamp input to [0..1] range
    value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return n + value * (m - n);
}

constexpr float ExponentialMap(float value, float fmin, float fmax)
{
    // Clamp input to [0..1] range
    value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    // Exponential mapping: fmin * (fmax/fmin)^value
    return fmin * powf(fmax / fmin, value);
}

// Round frequency to nearest semitone in equal temperament (A4 = 440Hz)
inline float roundToNearestSemitone(float frequency) {
    if (frequency <= 0.0f) return 0.0f;

    // Convert frequency to semitone number relative to A4 (440Hz)
    float semitone = 12.0f * log2f(frequency / 440.0f);

    // Round to nearest integer semitone
    int roundedSemitone = static_cast<int>(semitone + (semitone >= 0 ? 0.5f : -0.5f));

    // Convert back to frequency
    return 440.0f * powf(2.0f, roundedSemitone / 12.0f);
}


__force_inline float MapToSeries(float value, const std::vector<float>& series) {
    if (series.empty()) return 0.0f;
    if (series.size() == 1) return series[0];

    // Clamp value to [0, 1]
    value = fmaxf(0.0f, fminf(1.0f, value));

    // Calculate which segment we're in
    float segment_size = 1.0f / static_cast<float>(series.size());
    size_t index = static_cast<size_t>(value / segment_size);

    // Handle edge case where value == 1.0
    if (index >= series.size()) {
        index = series.size() - 1;
    }

    return series[index];
}


inline float medianAbsoluteDeviation(std::vector<float>& data) noexcept {
    const size_t N = data.size();

    // Find median
    const size_t mid = N >> 1;
    std::nth_element(data.begin(), data.begin() + mid, data.end());

    float median;
    if (!(N & 0x1)) {
        // Even size: need both middle elements
        std::nth_element(data.begin(), data.begin() + mid - 1, data.begin() + mid);
        median = (data[mid - 1] + data[mid]) * 0.5f;
    } else {
        // Odd size: just the middle element
        median = data[mid];
    }

    // Convert to absolute deviations in-place
    for (auto& val : data) {
        val = std::abs(val - median);
    }

    // Find median of absolute deviations
    std::nth_element(data.begin(), data.begin() + mid, data.end());

    if (!(N & 0x1)) {
        std::nth_element(data.begin(), data.begin() + mid - 1, data.begin() + mid);
        return (data[mid - 1] + data[mid]) * 0.5f;
    } else {
        return data[mid];
    }
}

inline int __attribute__((always_inline)) partition(float* arr, int left, int right) {
    float pivot = arr[right];
    int i = left;

    for (int j = left; j < right; j++) {
        if (arr[j] <= pivot) {
            float temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
            i++;
        }
    }

    float temp = arr[i];
    arr[i] = arr[right];
    arr[right] = temp;

    return i;
}

// Quick select median helper function - O(n) average time complexity
inline float __attribute__((always_inline)) quickSelectMedian(float* arr, int n) {
    if (n % 2 == 1) {
        // Odd size - find the middle element
        int k = n / 2;
        int left = 0;
        int right = n - 1;

        while (left < right) {
            int pivotIndex = partition(arr, left, right);
            if (pivotIndex == k) {
                break;
            } else if (pivotIndex < k) {
                left = pivotIndex + 1;
            } else {
                right = pivotIndex - 1;
            }
        }
        return arr[k];
    } else {
        // Even size - need to find two middle elements and average them
        int k1 = n / 2 - 1;
        int k2 = n / 2;

        // Find k1-th element (lower middle)
        int left = 0;
        int right = n - 1;
        while (left < right) {
            int pivotIndex = partition(arr, left, right);
            if (pivotIndex == k1) {
                break;
            } else if (pivotIndex < k1) {
                left = pivotIndex + 1;
            } else {
                right = pivotIndex - 1;
            }
        }
        float val1 = arr[k1];

        // Find k2-th element (upper middle) in the remaining array
        left = k1 + 1;
        right = n - 1;
        float val2 = arr[k2];
        for (int i = left; i <= right; i++) {
            if (arr[i] < val2) {
                val2 = arr[i];
            }
        }

        return (val1 + val2) * 0.5f;
    }
}

inline float __attribute__((always_inline)) __not_in_flash_func(medianAbsoluteDeviationFast32)(const float* data) {
    float __attribute__((aligned(16))) work[32];

    // Copy data to work buffer
    memcpy(work, data, 32 * sizeof(float));

    // Use median-of-medians approach for O(n) selection
    float median = quickSelectMedian(work, 32);

    // Calculate absolute deviations
    for (int i = 0; i < 32; i++) {
        work[i] = fabsf(data[i] - median);
    }

    // Find median of absolute deviations
    return quickSelectMedian(work, 32);
}

// Efficient Mean Absolute Deviation function - O(n) complexity
inline float __attribute__((always_inline)) __not_in_flash_func(meanAbsoluteDeviation)(const float* data, int size) {
    // Calculate mean in first pass
    float sum = 0.0f;

    // Unroll loop for better performance on RP2350
    int i = 0;
    for (; i < size - 3; i += 4) {
        sum += data[i] + data[i + 1] + data[i + 2] + data[i + 3];
    }

    // Handle remaining elements
    for (; i < size; i++) {
        sum += data[i];
    }

    float mean = sum / size;

    // Calculate mean of absolute deviations in second pass
    sum = 0.0f;
    i = 0;
    for (; i < size - 3; i += 4) {
        sum += fabsf(data[i] - mean) + fabsf(data[i + 1] - mean) +
               fabsf(data[i + 2] - mean) + fabsf(data[i + 3] - mean);
    }

    // Handle remaining elements
    for (; i < size; i++) {
        sum += fabsf(data[i] - mean);
    }

    return sum / size;
}

// Linear mapping function: maps value from [0..1] to [n..m]
inline float __attribute__((always_inline)) linearMap(float value, float n, float m) {
    // Clamp input to [0..1] range
    value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    return n + value * (m - n);
}

// Exponential mapping function: maps value from [0..1] to [n..m] with exponential curve
// exponent > 1.0 creates a curve that starts slow and accelerates
// exponent < 1.0 creates a curve that starts fast and decelerates
inline float __attribute__((always_inline)) exponentialMap(float value, float n, float m, float exponent = 2.0f) {
    // Clamp input to [0..1] range
    value = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    float exponentialValue = powf(value, exponent);
    return n + exponentialValue * (m - n);
}

int where(const std::vector<size_t>& integers, size_t integer);


namespace Tests {

bool testMedianAbsoluteDeviation();
bool testMeanAbsoluteDeviation();

} // namespace Tests

#endif  // __MATHS_HPP__
