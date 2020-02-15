#pragma once

#include <cstdlib> // for size_t
#include <stdexcept>
#include <type_traits>

namespace constexpr_math {
    template<typename T>
    struct enable_exact_compare {
        constexpr static bool value = false;
    };

    template<typename T>
    concept SupportsExactCompare = 
              std::is_integral<T>::value ||
              enable_exact_compare<T>::value;

    template<typename T>
    concept HasRoundingErrors = 
              std::is_floating_point<T>::value ||
              !enable_exact_compare<T>::value;

    template<typename T>
    concept Arithmetic = requires(T a, T b){
        a + b; a - b; a += b; a -= b;
        a * b; a / b; a *= b; a /= b;
    };

    template<typename T>
    concept Comparable = requires(T a, T b){
        a < b; a > b;
    };

    template<typename T>
    concept Number = Comparable<T> && Arithmetic<T>;

    constexpr HasRoundingErrors auto pi = 3.14159265358979323846264;
    constexpr HasRoundingErrors auto epsilon = 0.0001;

    template<Number number_t>
    constexpr number_t abs(const number_t &value){
        return value > number_t{0} ? value : -value;
    }

    template<HasRoundingErrors number_t>
    constexpr bool equal(const number_t &a, const number_t &b){
        return abs(a - b) < epsilon;
    }

    template<SupportsExactCompare integral_t>
    constexpr bool equal(const integral_t &a, const integral_t &b){
        return a == b;
    }

    template<Number number_t>
    constexpr number_t rad(const number_t &deg){
        return deg * number_t{pi} / number_t{180.0};
    }

    static_assert(equal(rad(90.0), pi / 2));

    template<Number number_t>
    constexpr number_t cos(const number_t &rad){
        const auto n = 20; // higher n = better precision
        number_t sum = 1;
        number_t t = 1;

        // Approximate using Taylor series
        for (auto i = 1; i <= n; ++i){
            t *= -rad * rad / (2 * i * (2 * i - 1)); 
            sum += t;
        }

        return sum;
    }

    template<typename T>
    concept Angle = requires(T t){
        cos(t);
    };

    static_assert(equal(cos(0.0), 1.0));
    static_assert(equal(cos(rad(60.0)), 0.50));

    template<unsigned short precision_v, typename number_t>
    class fast_cos{
        public:
        static constexpr auto precision = precision_v;
        static constexpr auto nof_values = precision * 360;

        number_t cos_table[nof_values];

        constexpr fast_cos() : cos_table() {
            for (auto i = size_t{0}; i < nof_values; ++i){
                Number auto deg = static_cast<number_t>(i) / precision;
                cos_table[i] = constexpr_math::cos(rad(deg));
            }
        }

        constexpr number_t value_at(unsigned table_index) const {
            if (table_index / precision > 360)
                throw std::runtime_error ("Angle must be under 360 degrees");
            
            return cos_table[table_index];
        }

        constexpr number_t cos(const number_t &deg) const {
            // todo: error checking!
            return value_at(static_cast<unsigned>(deg * precision));
        }
    };

    static_assert(equal(fast_cos<10, double>{}.cos(60), 0.5));

    template<Number number_t>
    constexpr decltype(auto) pow(const number_t &value, unsigned exp){
        number_t result = 1;
        for (auto i = unsigned{0}; i < exp; ++i){
            result *= value;
        }
        return result;
    }
}
