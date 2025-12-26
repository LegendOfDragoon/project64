#include "RspRegState.h"

CRspRegState::CRspRegState()
{
    for (size_t i = 0; i < (size_t)RspFlags::MaxFlags; i++)
    {
        m_FlagIsZero[i] = false;
    }
}

CRspRegState::~CRspRegState()
{
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
    for (size_t i = 0; i < (size_t)RspFlags::MaxFlags; i++)
    {
        m_FlagIsZero[i] = false;
    }
}
