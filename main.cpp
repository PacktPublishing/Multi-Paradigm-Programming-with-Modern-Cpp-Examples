#include <iostream>

namespace constexpr_math {
    constexpr auto pi = 3.14159265358979323846264;
    constexpr auto epsilon = 0.0001;

    constexpr double abs(double value){
        return value > 0 ? value : -value;
    }

    constexpr bool equal(double a, double b){
        return abs(a - b) < epsilon;
    }

    constexpr double rad(double deg){
        return deg * pi / 180.0;
    }

    static_assert(equal(rad(90), pi / 2));

    constexpr double cos(double rad){
        const auto n = 20; // higher n = better precision
        long double sum = 1;
        long double t = 1;

        // Approximate using Taylor series
        for (auto i = 1; i <= n; ++i){
            t *= -rad * rad / (2 * i * (2 * i - 1)); 
            sum += t;
        }

        return sum;
    }

    static_assert(equal(cos(0), 1));
    static_assert(equal(cos(rad(60)), 0.50));

    template<unsigned short precision_v>
    class fast_cos{
        public:
        static constexpr auto precision = precision_v;
        static constexpr auto nof_values = precision * 360;

        double cos_table[nof_values];

        constexpr fast_cos() : cos_table() {
            for (auto i = size_t{0}; i < nof_values; ++i){
                cos_table[i] = constexpr_math::cos(rad(static_cast<double>(i) / precision));
            }
        }

        constexpr double value_at(unsigned table_index) const {
            if (table_index / precision > 360)
                throw std::runtime_error ("Angle must be under 360 degrees");
            
            return cos_table[table_index];
        }

        constexpr double cos(double deg) const {
            // todo: error checking!
            return value_at(static_cast<unsigned>(deg * precision));
        }

    };

    static_assert(equal(fast_cos<10>{}.cos(60), 0.5));
}

int main(int, char**) {
    constexpr int precision = 10;
    auto c = constexpr_math::fast_cos<precision>{};
    std::cout << c.cos(60.1) << std::endl;
}
