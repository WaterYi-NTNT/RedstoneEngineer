#pragma once
#include <cstdint>

namespace SimFlags
{
    
    inline constexpr uint16_t ACTIVE               = 0x0001; 
    inline constexpr uint16_t SCHEDULED            = 0x0002; 
    inline constexpr uint16_t REPEATER_DELAY_MASK  = 0x000C; 
    inline constexpr int      REPEATER_DELAY_SHIFT = 2;
    inline constexpr uint16_t STRONG_POWERED       = 0x0010; 
    inline constexpr uint16_t LIT                  = 0x0020; 
    inline constexpr uint16_t LOCKED               = 0x0040; 
    inline constexpr uint16_t COMPARATOR_SUBTRACT  = 0x0080; 

    inline constexpr uint16_t WEAK_POWERED         = 0x0100;
    
    inline uint16_t setRepeaterDelay(uint16_t flags, uint8_t delay)
    {
        return (flags & ~REPEATER_DELAY_MASK)
             | ((static_cast<uint16_t>(delay) << REPEATER_DELAY_SHIFT)
                & REPEATER_DELAY_MASK);
    }

    inline uint8_t getRepeaterDelay(uint16_t flags)
    {
        return static_cast<uint8_t>(
            (flags & REPEATER_DELAY_MASK) >> REPEATER_DELAY_SHIFT);
    }

    inline bool isSubtractMode(uint16_t flags)
    {
        return (flags & COMPARATOR_SUBTRACT) != 0;
    }

    inline uint16_t toggleComparatorMode(uint16_t flags)
    {
        return flags ^ COMPARATOR_SUBTRACT;
    }
}