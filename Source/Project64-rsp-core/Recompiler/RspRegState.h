#pragma once
#include <stdint.h>

enum class RspFlags
{
    VCOL,
    VCOH,
    VCCL,
    VCCH,
    VCE,
    MaxFlags
};

class CRspRegState
{
public:
    CRspRegState();
    ~CRspRegState();

    bool IsGprConst(uint8_t gprReg) const;
    uint32_t GetGprConstValue(uint8_t gprReg) const;
    void SetGprConst(uint8_t gprReg, uint32_t value);
    void SetGprUnknown(uint8_t gprReg);

    bool IsFlagZero(RspFlags flag) const;
    void SetFlagZero(RspFlags flag);
    void SetFlagUnknown(RspFlags flag);

    void WriteBackRegisters();

private:
    bool m_GprIsConst[32];
    uint32_t m_GprConstValue[32];
    bool m_FlagIsZero[static_cast<size_t>(RspFlags::MaxFlags)];
};
