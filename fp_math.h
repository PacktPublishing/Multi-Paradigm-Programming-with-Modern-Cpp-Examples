#pragma once

#include "constexpr_math.h"
#include <type_traits>

namespace fp_math {

    template<typename T>
    concept Integral = std::is_integral<T>::value;

    template<unsigned short precision_v>
    class number {
        public:
        using storage_type = long long;
        using same_precision_number = number<precision_v>;

        constexpr static auto precision = precision_v;
        constexpr static auto offset = constexpr_math::pow(storage_type{10}, precision_v);

        constexpr number() = default;

        template<typename fp_type>
        constexpr number(const fp_type &value) noexcept{
            auto dec_part = static_cast<storage_type>(value);
            value_ = dec_part * offset;

            auto frac_part = value - dec_part;
            value_ += frac_part * offset;
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

        constexpr auto operator += (const same_precision_number &rhs) noexcept {
            value_ += rhs.value_;
            return *this;
        }

        constexpr auto operator -= (const same_precision_number &rhs) noexcept {
            value_ -= rhs.value_;
            return *this;
        }

        //TODO: implement the operators
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wexceptions"
        constexpr auto operator * (const same_precision_number &rhs) const noexcept{
            throw std::logic_error("Not implemented");
            return *this; // for return type deduction
        }

        constexpr auto operator / (const same_precision_number &rhs) const noexcept{
            throw std::logic_error("Not implemented");
            return *this;
        }

        constexpr auto operator *= (const same_precision_number &rhs) noexcept {
            throw std::logic_error("Not implemented");
            return *this;
        }
        constexpr auto operator /= (const same_precision_number &rhs) noexcept {
            throw std::logic_error("Not implemented");
            return *this;
        }

        #pragma clang diagnostic pop

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

    //static_assert(constexpr_math::cos(n4{0}) == 1);

    static_assert(n4::offset == 10000);
    static_assert(n4{-33.42}.int_part() == -33);
    static_assert(n4{-33.42}.frac_part() == -4200);

    static_assert(n4{42}.int_part() == 42);
    static_assert(n4{42}.frac_part() == 0);

    static_assert(n4{42} == n4{42});
    static_assert(n4{42.1} < n4{42.2});

    static_assert(constexpr_math::abs(n4{-42}) == n4{42});
    static_assert(constexpr_math::equal(n4{42}, n4{42}));

    //static_assert(constexpr_math::fast_cos<10, n4>{}.cos(60) == n4{0.5});

    static_assert(!constexpr_math::equal(number<9>{1.0}, number<9>{1.0000001}));
}
