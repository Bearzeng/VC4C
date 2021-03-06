/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef UNITS_H
#define UNITS_H

#include <cstdint>
#include <iostream>

namespace vc4c
{
    struct Word;

    struct Byte
    {
    public:
        using type = uint64_t;
        constexpr explicit Byte(uint64_t val) : value(val) {}

        constexpr explicit operator uint64_t() const
        {
            return value;
        }

        constexpr uint64_t getValue() const
        {
            return value;
        }

        uint64_t getPaddingTo(uint64_t numBytes) const
        {
            if((value % numBytes) == 0)
                return 0;
            return numBytes - (value % numBytes);
        }

        constexpr Byte operator+(const Byte& other) const
        {
            return Byte(value + other.value);
        }

        constexpr Byte operator-(const Byte& other) const
        {
            return Byte(value - other.value);
        }

    private:
        uint64_t value;
        friend struct Word;

        // hidden friend operators to not take part in overload resolution for any other type
        friend std::ostream& operator<<(std::ostream& s, const Byte& b)
        {
            return s << static_cast<uint64_t>(b);
        }

        friend std::wostream& operator<<(std::wostream& s, const Byte& b)
        {
            return s << static_cast<uint64_t>(b);
        }
    };

    struct Word
    {
    public:
        using type = uint64_t;
        constexpr explicit Word(uint64_t val) : value(val) {}
        constexpr explicit Word(Byte bytes) : value(bytes.value / sizeof(uint64_t)) {}

        constexpr explicit operator uint64_t() const
        {
            return value;
        }

        constexpr uint64_t getValue() const
        {
            return value;
        }

        constexpr Byte toBytes() const
        {
            return Byte(value * sizeof(uint64_t));
        }

        constexpr Word operator+(const Word& other) const
        {
            return Word(value + other.value);
        }

        constexpr Word operator-(const Word& other) const
        {
            return Word(value - other.value);
        }

    private:
        uint64_t value;

        // hidden friend operators to not take part in overload resolution for any other type
        friend std::ostream& operator<<(std::ostream& s, const Word& word)
        {
            return s << static_cast<uint64_t>(word);
        }

        friend std::wostream& operator<<(std::wostream& s, const Word& word)
        {
            return s << static_cast<uint64_t>(word);
        }
    };

} /* namespace vc4c */
#endif /* UNITS_H */
