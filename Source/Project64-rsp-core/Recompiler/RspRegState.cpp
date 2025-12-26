#include "RspRegState.h"

CRspRegState::CRspRegState()
{
    for (int i = 0; i < 32; i++)
    {
        m_GprIsConst[i] = false;
        m_GprConstValue[i] = 0;
    }
    m_GprIsConst[0] = true;
    m_GprConstValue[0] = 0;

    for (size_t i = 0; i < (size_t)RspFlags::MaxFlags; i++)
    {
        m_FlagIsZero[i] = false;
    }
}

CRspRegState::~CRspRegState()
{
}

bool CRspRegState::IsGprConst(uint8_t gprReg) const
{
    return m_GprIsConst[gprReg];
}

uint32_t CRspRegState::GetGprConstValue(uint8_t gprReg) const
{
    return m_GprConstValue[gprReg];
}

bool CRspRegState::IsFlagZero(RspFlags flag) const
{
    return m_FlagIsZero[(size_t)flag];
}

void CRspRegState::SetFlagZero(RspFlags flag)
{
    m_FlagIsZero[(size_t)flag] = true;
}

void CRspRegState::SetFlagUnknown(RspFlags flag)
{
    m_FlagIsZero[(size_t)flag] = false;
}

void CRspRegState::WriteBackRegisters()
{
    for (int i = 0; i < 32; i++)
    {
        m_GprIsConst[i] = false;
        m_GprConstValue[i] = 0;
    }
    m_GprIsConst[0] = true;
    m_GprConstValue[0] = 0;

    for (size_t i = 0; i < (size_t)RspFlags::MaxFlags; i++)
    {
        m_FlagIsZero[i] = false;
    }
}
