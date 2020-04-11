#pragma once

#include "constexpr_math.h"

namespace fp_math {
    template<unsigned short precision_v = 4>
    class number {
        public:
        using storage_type = long long;
        using same_precision_number = number<precision_v>;

        constexpr static auto precision = precision_v;
        constexpr static auto offset = constexpr_math::pow(storage_type{10}, precision_v);



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

        constexpr number() = default;

        constexpr auto operator==(const same_precision_number &other) const noexcept{
            return value_ == other.value_;
        }

        constexpr auto operator <(const same_precision_number &rhs) const noexcept {
            return value_ < rhs.value_;
        } 

        constexpr auto operator >(const same_precision_number &rhs) const noexcept {
            return value_ > rhs.value_;
        } 
        
        
        constexpr auto operator -() const noexcept{
            same_precision_number n;
            n.value_ = -value_;
            return n;
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

namespace fp_math::test {
    using n4 = number<4>;
    static_assert(n4::offset == 10000);
    static_assert(n4{-33.42}.int_part() == -33);
    static_assert(n4{-33.42}.frac_part() == -4200);

    static_assert(constexpr_math::abs(n4{-42}) == n4{42});

    using n9 = number<9>;
    static_assert(constexpr_math::equal(n9{42}, n9{42.0000001}));
    static_assert(n9{42} == n9{42.0000001});
}
