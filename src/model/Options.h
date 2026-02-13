#pragma once
#include <cstdint>

enum class Gain
{
    G_30_mV_fC,
    G_20_mV_fC,
    G_4_mV_fC
};
class GainStrings {
public:
    static const char* toString(Gain gain) {
        switch (gain) {
            case Gain::G_30_mV_fC: return "30 mV/fC";
            case Gain::G_20_mV_fC: return "20 mV/fC";
            case Gain::G_4_mV_fC: return "4 mV/fC";
            default: return "Unknown Gain";
        }
    }
};

enum class Shaping
{
    SH_160_ns,
    SH_300_ns
};
class ShapingStrings {
public:
    static const char* toString(Shaping shaping) {
        switch (shaping) {
            case Shaping::SH_160_ns: return "160 ns";
            case Shaping::SH_300_ns: return "300 ns";
            default: return "Unknown Shaping";
        }
    }
};

enum class NumSamples
{
    N_16,
    N_32,
    N_64,
    N_128
};
class NumSamplesStrings {
public:
    static const char* toString(NumSamples numSamples) {
        switch (numSamples) {
            case NumSamples::N_16: return "16 samples";
            case NumSamples::N_32: return "32 samples";
            case NumSamples::N_64: return "64 samples";
            case NumSamples::N_128: return "128 samples";
            default: return "Unknown Number of Samples";
        }
    }
};

enum class TriggerType
{
    External,
    SelfTrigger, // New: maps to original PON bit (bit 1)
    KHz1,        // New: maps to bit 8
    MHz1         // New: maps to bit 9
};
class TriggerTypeStrings {
public:
    static const char* toString(TriggerType triggerType) {
        switch (triggerType) {
            case TriggerType::External: return "External";
            case TriggerType::SelfTrigger: return "Self-trigger";
            case TriggerType::KHz1: return "1 kHz";
            case TriggerType::MHz1: return "1 MHz";
            default: return "Unknown Trigger Type";
        }
    }
};

class PONConverter {
public:
    static uint32_t toPON_reg(bool powerOn, bool polarity, Gain gain, Shaping shaping, TriggerType triggerType, NumSamples numSamples, uint16_t triggerThreshold = 0, uint8_t pretrigger = 0, bool externalClkEnable = false) {
        uint32_t value = 0;
        value |= (powerOn ? 0x1 : 0);                     // bit 0: powerOn
        // Trigger type mapping:
        // External: bit 1 = 0
        // SelfTrigger: bit 1 = 1
        // 1 kHz: bit 8 = 1
        // 1 MHz: bit 9 = 1
        if (triggerType == TriggerType::External) {
            value |= 0;
        } else if (triggerType == TriggerType::SelfTrigger) {
            value |= 0x2; // bit 1
        } else if (triggerType == TriggerType::KHz1) {
            value |= (1 << 8); // bit 8
        } else if (triggerType == TriggerType::MHz1) {
            value |= (1 << 9); // bit 9
        }
        value += (numSamples == NumSamples::N_16 ? 0 : 
            (numSamples == NumSamples::N_32 ? 0x4 : 
                (numSamples == NumSamples::N_64 ? 0x8 : 0xC))); // bits 2-4: numSamples
        value += (gain == Gain::G_30_mV_fC ? 0x30: (gain == Gain::G_20_mV_fC ? 0x20 : 0)); // bits 5-6: gain
        value += (shaping == Shaping::SH_160_ns ? 0 : 0x40); // bit 7: shaping (0 for SH_160_ns, 1 for SH_300_ns)
        value += (polarity ? 0x80: 0);                       // bit 8: polarity
        // Add trigger threshold (bits 10-19)
        value |= ((triggerThreshold & 0x3FF) << 10);
        // Add pretrigger value (bits 20-21)
        value |= ((pretrigger & 0x3) << 20);
        // Add external clock enable (bit 22)
        value |= (externalClkEnable ? (1 << 22) : 0);
        return value;
    }

    static bool fromPON_regToPowerOn(uint32_t value) {
        return (value & 0x01) != 0;
    }

    static TriggerType fromPON_regToTriggerType(uint32_t value) {
        return static_cast<TriggerType>((value >> 1) & 0x01);
    }

    static Gain fromPON_regToGain(uint32_t value) {
        return static_cast<Gain>((value >> 5) & 0x03);
    }

    static Shaping fromPON_regToShaping(uint32_t value) {
        return static_cast<Shaping>((value >> 7) & 0x01);
    }

    static bool fromPON_regToPolarity(uint32_t value) {
        return (value & 0x10000000) == 0; // bit 8
    }

    static NumSamples fromPON_regToNumSamples(uint32_t value) {
        uint32_t numSamplesBits = (value >> 2) & 0x07; // bits 2-4
        switch (numSamplesBits) {
            case 0: return NumSamples::N_16;
            case 1: return NumSamples::N_32;
            case 2: return NumSamples::N_64;
            case 3: return NumSamples::N_128;
            default: return NumSamples::N_16; // Default case
        }
    }

    static uint16_t fromPON_regToTriggerThreshold(uint32_t value) {
        return (value >> 10) & 0x3FF;
    }

    static uint8_t fromPON_regToPretrigger(uint32_t value) {
        return (value >> 20) & 0x3;
    }

    static bool fromPON_regToExternalClkEnable(uint32_t value) {
        return (value & (1 << 22)) != 0;
    }
};