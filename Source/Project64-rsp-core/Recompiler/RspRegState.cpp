#include "RspRegState.h"
#include "Recompiler/RspAssembler.h"
#include "Recompiler/RspRecompilerOps-x64.h"
#include <Common/StdString.h>
#include <Settings/Settings.h>

CRspRegState::CRspRegState(CRSPRecompilerOps & RecompilerOps) :
    m_RecompilerOps(RecompilerOps),
    m_Assembler(RecompilerOps.m_Assembler)
{
    for (int i = 0; i < 32; i++)
    {
        m_GprIsConst[i] = false;
        m_GprConstValue[i] = 0;
    }
    m_GprIsConst[0] = true;
    m_GprConstValue[0] = 0;

    for (int i = 0; i < 6; i++)
    {
        m_XmmState[i] = XmmState::Free;
    }
    for (int i = 6; i < 16; i++)
    {
        m_XmmState[i] = XmmState::Reserved;
    }
    for (int i = 0; i < 16; i++)
    {
        m_XmmRegMapped[i] = (uint8_t)~0;
        m_XmmProtected[i] = false;
    }
    for (size_t i = 0; i < (size_t)RspFlags::MaxFlags; i++)
    {
        m_FlagIsZero[i] = false;
    }
}

CRspRegState::~CRspRegState()
{
}

void CRspRegState::ResetRegProtection()
{
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        m_XmmProtected[i] = false;
    }
}

asmjit::x86::Xmm CRspRegState::MapXmmZero()
{
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Zero && !m_XmmProtected[i])
        {
            m_XmmProtected[i] = true;
            return asmjit::x86::Xmm(i);
        }
    }

    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Free)
        {
            m_Assembler->comment(stdstr_f(" regcache: allocate xmm%d as zero", i).c_str());
            m_XmmState[i] = XmmState::Zero;
            m_XmmProtected[i] = true;
            m_Assembler->pxor(asmjit::x86::Xmm(i), asmjit::x86::Xmm(i));
            return asmjit::x86::Xmm(i);
        }
    }
    g_Notify->BreakPoint(__FILE__, __LINE__);
    return asmjit::x86::Xmm();
}

asmjit::x86::Xmm CRspRegState::MapXmmReg(uint8_t vreg, uint8_t source)
{
    asmjit::x86::Xmm srcReg = VRegMapping(source);
    if (srcReg.isValid())
    {
        ProtectXmm(srcReg);
    }
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Mapped && m_XmmRegMapped[i] == vreg)
        {
            if (source != vreg)
            {
                asmjit::x86::Xmm srcReg = VRegMapping(source);
                if (srcReg.isValid())
                {
                    m_Assembler->movdqa(asmjit::x86::Xmm(i), srcReg);
                }
                else
                {
                    m_Assembler->movdqa(asmjit::x86::Xmm(i), asmjit::x86::ptr(asmjit::x86::r14, m_RecompilerOps.VectorOffset(source)));
                }
            }
            return asmjit::x86::Xmm(i);
        }
    }

    asmjit::x86::Xmm reg;
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] != XmmState::Free)
        {
            continue;
        }
        m_Assembler->comment(stdstr_f(" regcache: allocate xmm%d to V%d", i, vreg).c_str());
        m_XmmState[i] = XmmState::Mapped;
        m_XmmRegMapped[i] = vreg;
        m_XmmProtected[i] = true;
        reg = asmjit::x86::Xmm(i);
        break;
    }

    if (!reg.isValid())
    {
        for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
        {
            if (m_XmmState[i] != XmmState::Temp)
            {
                continue;
            }
            m_Assembler->comment(stdstr_f(" regcache: allocate xmm%d to V%d", i, vreg).c_str());
            m_XmmState[i] = XmmState::Mapped;
            m_XmmRegMapped[i] = vreg;
            m_XmmProtected[i] = true;
            reg = asmjit::x86::Xmm(i);
            break;
        }
    }

    if (reg.isValid())
    {
        if (srcReg.isValid())
        {
            m_Assembler->movdqa(reg, srcReg);
        }
        else
        {
            m_Assembler->movdqa(reg, asmjit::x86::ptr(asmjit::x86::r14, m_RecompilerOps.VectorOffset(source)));
        }
        return reg;
    }
    g_Notify->BreakPoint(__FILE__, __LINE__);
    return asmjit::x86::Xmm();
}

asmjit::x86::Xmm CRspRegState::VRegMapping(uint8_t vreg)
{
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Mapped && m_XmmRegMapped[i] == vreg)
        {
            return asmjit::x86::Xmm(i);
        }
    }
    return asmjit::x86::Xmm();
}

void CRspRegState::ProtectXmm(asmjit::x86::Xmm xmm)
{
    if (!xmm.isValid())
    {
        return;
    }

    int id = xmm.id();
    if (id >= 0 && id < 16)
    {
        m_XmmProtected[id] = true;
    }
}

void CRspRegState::UnprotectXmm(asmjit::x86::Xmm xmm)
{
    if (!xmm.isValid())
    {
        return;
    }

    int id = xmm.id();
    if (id >= 0 && id < 16)
    {
        m_XmmProtected[id] = false;
    }
}

asmjit::x86::Xmm CRspRegState::MapXmmTemp(bool loadReg, uint8_t vreg, uint8_t e)
{
    asmjit::x86::Xmm tempReg;
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Temp && !m_XmmProtected[i])
        {
            tempReg = asmjit::x86::Xmm(i);
            m_XmmProtected[i] = true;
            break;
        }
    }

    if (!tempReg.isValid())
    {
        for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
        {
            if (m_XmmState[i] == XmmState::Free)
            {
                tempReg = asmjit::x86::Xmm(i);
                m_Assembler->comment(stdstr_f(" regcache: allocate xmm%d as temp register", i).c_str());
                m_XmmState[i] = XmmState::Temp;
                m_XmmProtected[i] = true;
                break;
            }
        }
    }
    if (!tempReg.isValid())
    {
        for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
        {
            if (m_XmmState[i] == XmmState::Zero && !m_XmmProtected[i])
            {
                tempReg = asmjit::x86::Xmm(i);
                m_Assembler->comment(stdstr_f(" regcache: allocate xmm%d as temp register", i).c_str());
                m_XmmState[i] = XmmState::Temp;
                m_XmmProtected[i] = true;
                break;
            }
        }
    }

    if (!tempReg.isValid())
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
        return asmjit::x86::Xmm();
    }

    if (loadReg)
    {
        asmjit::x86::Xmm srcReg = VRegMapping(vreg);
        if (srcReg.isValid())
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
        else
        {
            m_RecompilerOps.LoadVectorRegister(tempReg, vreg, e);
        }
    }
    return tempReg;
}

bool CRspRegState::IsGprConst(uint8_t gprReg) const
{
    return m_GprIsConst[gprReg];
}

uint32_t CRspRegState::GetGprConstValue(uint8_t gprReg) const
{
    return m_GprConstValue[gprReg];
}

void CRspRegState::SetGprConst(uint8_t gprReg, uint32_t value)
{
    if (gprReg == 0)
    {
        return;
    }
    m_GprIsConst[gprReg] = true;
    m_GprConstValue[gprReg] = value;
}

void CRspRegState::SetGprUnknown(uint8_t gprReg)
{
    if (gprReg == 0)
    {
        return;
    }
    m_GprIsConst[gprReg] = false;
    m_GprConstValue[gprReg] = 0;
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
    for (int i = 0, n = sizeof(m_XmmState) / sizeof(m_XmmState[0]); i < n; i++)
    {
        if (m_XmmState[i] == XmmState::Zero)
        {
            m_Assembler->comment(stdstr_f(" regcache: deallocate xmm%d as zero", i).c_str());
            m_XmmState[i] = XmmState::Free;
        }
        else if (m_XmmState[i] == XmmState::Temp)
        {
            m_Assembler->comment(stdstr_f(" regcache: deallocate xmm%d as temp register", i).c_str());
            m_XmmState[i] = XmmState::Free;
        }
        else if (m_XmmState[i] == XmmState::Mapped)
        {
            m_Assembler->comment(stdstr_f(" regcache: deallocate xmm%d from V%d", i, m_XmmRegMapped[i]).c_str());
            m_Assembler->movdqa(asmjit::x86::ptr(asmjit::x86::r14, m_RecompilerOps.VectorOffset(m_XmmRegMapped[i])), asmjit::x86::Xmm(i));
            m_XmmState[i] = XmmState::Free;
            m_XmmRegMapped[i] = (uint8_t)~0;
        }
        else if (m_XmmState[i] != XmmState::Free && m_XmmState[i] != XmmState::Reserved)
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
    }
}
