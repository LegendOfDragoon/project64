#pragma once

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

    bool IsFlagZero(RspFlags flag) const;
    void SetFlagZero(RspFlags flag);
    void SetFlagUnknown(RspFlags flag);

    void WriteBackRegisters();

private:
    bool m_FlagIsZero[static_cast<size_t>(RspFlags::MaxFlags)];
};
