#pragma once
#include <cstdint>

namespace SimFlags
{
    inline constexpr uint8_t ACTIVE               = 0b0000'0001;
    inline constexpr uint8_t SCHEDULED            = 0b0000'0010;
    inline constexpr uint8_t REPEATER_DELAY_MASK  = 0b0000'1100;
    inline constexpr int     REPEATER_DELAY_SHIFT = 2;
    inline constexpr uint8_t STRONG_POWERED       = 0b0001'0000;
    inline constexpr uint8_t LIT                  = 0b0010'0000;
    inline constexpr uint8_t LOCKED               = 0b0100'0000;
    inline constexpr uint8_t COMPARATOR_SUBTRACT  = 0b1000'0000; // [SIM] 比较器减法模式

    inline uint8_t setRepeaterDelay(uint8_t flags, uint8_t delay)
    {
        return (flags & ~REPEATER_DELAY_MASK)
             | ((delay << REPEATER_DELAY_SHIFT) & REPEATER_DELAY_MASK);
    }
    inline uint8_t getRepeaterDelay(uint8_t flags)
    {
        return (flags & REPEATER_DELAY_MASK) >> REPEATER_DELAY_SHIFT;
    }

    // [SIM] 比较器模式辅助
    inline bool isSubtractMode(uint8_t flags)
    {
        return (flags & COMPARATOR_SUBTRACT) != 0;
    }
    inline uint8_t toggleComparatorMode(uint8_t flags)
    {
        return flags ^ COMPARATOR_SUBTRACT;
    }
}
