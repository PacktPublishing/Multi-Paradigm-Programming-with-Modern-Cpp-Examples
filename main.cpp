#include "constexpr_math.h"
#include "fp_math.h"
#include <iostream>

int main(int, char**) {
    constexpr int precision = 10;
    constexpr auto c = constexpr_math::fast_cos<precision, double>{};
    constexpr auto c2 = constexpr_math::fast_cos<precision, float>{};
    //constexpr auto c3 = constexpr_math::fast_cos<precision, int>{};
    std::cout << c.cos(60.1) << std::endl;
}
