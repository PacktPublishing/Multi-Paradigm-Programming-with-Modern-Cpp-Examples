#pragma once

#include "constexpr_math.h"
#include <type_traits>

namespace fp_math {
    template<unsigned short precision_v = 4>
    class number {
        public:
        using storage_type = long long;
        using same_precision_number = number<precision_v>;

        constexpr static auto precision = precision_v;
        constexpr static auto offset = static_cast<storage_type>(
            constexpr_math::pow(decltype(precision){10}, precision_v));

        constexpr number() = default;

        template<typename fp_type>
        constexpr number(const fp_type &value) noexcept{
            value_ = static_cast<storage_type>(value) * offset;
            value_ += static_cast<storage_type>(
                (value - static_cast<storage_type>(value)) * offset);
        }

        constexpr storage_type int_part() const noexcept{
            return value_ / offset;
        }

        constexpr storage_type frac_part() const  noexcept{
            return value_ % (int_part() * offset);
        }

        constexpr auto operator==(const same_precision_number &other) const noexcept{
            return value_ == other.value_;
        }

        constexpr auto operator -() const noexcept{
            same_precision_number n;
            n.value_ = -value_;
            return n;
        }

        constexpr auto operator <(const same_precision_number &rhs) const noexcept {
            return value_ < rhs.value_;
        } 

        constexpr auto operator >(const same_precision_number &rhs) const noexcept {
            return value_ > rhs.value_;
        } 

        constexpr auto operator +(const same_precision_number &rhs) const noexcept{
            same_precision_number n;
            n.value_ = value_ + rhs.value_;
            return n;
        }

        constexpr auto operator -(const same_precision_number &rhs) const noexcept{
            same_precision_number n;
            n.value_ = value_ - rhs.value_;
            return n;
        }

        private:
        storage_type value_ = 0;
    };
}

namespace constexpr_math {
    template<unsigned short precision_v>
    struct enable_exact_compare<fp_math::number<precision_v>> {
        constexpr static auto value = true;
    };
}

namespace fp_math {


    using n4 = number<4>;

    static_assert(n4::offset == 10000);
    static_assert(n4{-33.42}.int_part() == -33);
    static_assert(n4{-33.42}.frac_part() == -4200);

    static_assert(n4{42}.int_part() == 42);
    static_assert(n4{42}.frac_part() == 0);

    static_assert(n4{42} == n4{42});
    static_assert(n4{42.1} < n4{42.2});

    static_assert(constexpr_math::abs(n4{-42}) == n4{42});
    static_assert(constexpr_math::equal(n4{42}, n4{42}));



    //static_assert(constexpr_math::equal(number<4>{1.0}, number<4>{1.0}));
}
