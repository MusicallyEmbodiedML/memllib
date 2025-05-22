#ifndef MEMLLIB_UTILS_CIRCULAR_BUFFER_HPP
#define MEMLLIB_UTILS_CIRCULAR_BUFFER_HPP

#include <array>
#include <vector>

template<typename T, size_t N>
class CircularBuffer {
private:
    static_assert((N & (N - 1)) == 0, "Buffer size must be a power of 2");
    std::array<T, N> buffer;
    size_t head = 0;

public:
    // Initialize buffer with zeros
    CircularBuffer() : buffer() {
        buffer.fill(T{});  // Zero-initialize all elements
    }

    // O(1) push operation - always overwrites oldest value
    void push(T value) {
        buffer[head] = value;
        head = (head + 1) & (N - 1);
    }

    // Copy contents to external array - no allocations
    void copyTo(std::array<T, N>& dest) const {
        for (size_t i = 0; i < N; i++) {
            dest[i] = buffer[(head + i) & (N - 1)];
        }
    }

    // Direct array-like access
    T& operator[](size_t index) {
        return buffer[(head + index) & (N - 1)];
    }

    const T& operator[](size_t index) const {
        return buffer[(head + index) & (N - 1)];
    }

    // Always returns N since buffer is always full
    constexpr size_t size() const {
        return N;
    }
};

#endif // MEMLLIB_UTILS_CIRCULAR_BUFFER_HPP
