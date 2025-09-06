#include "RspCodeBlock.h"
#include <Common/StdString.h>
#include <Project64-rsp-core/cpu/RspSystem.h>

RspCodeBlock::RspCodeBlock(CRSPSystem & System, uint32_t StartAddress, RspCodeType type, uint32_t EndBlockAddress, RspCodeBlocks & Functions) :
    m_Functions(Functions),
    m_EndBlockAddress(EndBlockAddress),
    m_System(System),
    m_StartAddress(StartAddress),
    m_CodeType(type),
    m_CompiledLoction(nullptr),
    m_Valid(true)
{
    Analyze();
}

const RspCodeBlock::Addresses & RspCodeBlock::GetBranchTargets() const
{
    return m_BranchTargets;
}

void * RspCodeBlock::GetCompiledLocation() const
{
    return m_CompiledLoction;
}

uint32_t RspCodeBlock::GetEndBlockAddress() const
{
    return m_EndBlockAddress;
}

const RspCodeBlock::Addresses & RspCodeBlock::GetFunctionCalls() const
{
    return m_FunctionCalls;
}

const RSPInstructions & RspCodeBlock::GetInstructions() const
{
    return m_Instructions;
}

const RspCodeBlock * RspCodeBlock::GetFunctionBlock(uint32_t Address) const
{
    RspCodeBlocks::const_iterator itr = m_Functions.find(Address);
    if (itr != m_Functions.end())
    {
        return itr->second.get();
    }
    return nullptr;
}

uint32_t RspCodeBlock::GetStartAddress() const
{
    return m_StartAddress;
}

void RspCodeBlock::SetCompiledLocation(void * CompiledLoction)
{
    m_CompiledLoction = CompiledLoction;
}

RspCodeType RspCodeBlock::CodeType() const
{
    return m_CodeType;
}

bool RspCodeBlock::IsEnd(uint32_t Address) const
{
    return m_End.find(Address) != m_End.end();
}

bool RspCodeBlock::IsValid() const
{
    return m_Valid;
}

void RspCodeBlock::Analyze(void)
{
    uint32_t Address = m_StartAddress;
    uint8_t * IMEM = m_System.m_IMEM;

    bool FoundEnd = false;
    for (;;)
    {
        RSPInstruction Instruction(Address, *(uint32_t *)(IMEM + (Address & 0xFFF)));
        if (Instruction.IsJump())
        {
            uint32_t target = Instruction.JumpTarget();
            if (target == m_EndBlockAddress)
            {
                if (std::find(m_End.begin(), m_End.end(), Address) == m_End.end())
                {
                    m_End.insert(Address);
                }
            }
            else
            {
                if (std::find(m_BranchTargets.begin(), m_BranchTargets.end(), target) == m_BranchTargets.end())
                {
                    m_BranchTargets.insert(target);
                }
            }
        }
        else if (Instruction.IsConditionalBranch())
        {
            uint32_t target = Instruction.ConditionalBranchTarget();
            if (target != m_EndBlockAddress)
            {
                if (std::find(m_BranchTargets.begin(), m_BranchTargets.end(), target) == m_BranchTargets.end())
                {
                    m_BranchTargets.insert(target);
                }
            }
        }
        else if (Instruction.IsStaticCall())
        {
            uint32_t target = Instruction.StaticCallTarget();
            if (std::find(m_FunctionCalls.begin(), m_FunctionCalls.end(), target) == m_FunctionCalls.end())
            {
                m_FunctionCalls.insert(target);
            }
        }

        m_Instructions.push_back(Instruction);
        if (FoundEnd)
        {
            break;
        }
        if ((m_CodeType == RspCodeType_SUBROUTINE && Instruction.IsJumpReturn()) ||
            (m_CodeType == RspCodeType_TASK && Instruction.IsJump() && Instruction.JumpTarget() == m_EndBlockAddress))
        {
            bool JumpBeyond = false;
            for (Addresses::iterator itr = m_BranchTargets.begin(); itr != m_BranchTargets.end(); itr++)
            {
                if (*itr > Address)
                {
                    JumpBeyond = true;
                    break;
                }
            }
            FoundEnd = !JumpBeyond;
        }
        Address += 4;
        if (Address == 0x2000)
        {
            m_Valid = false;
            return;
        }
    }
    for (Addresses::iterator itr = m_BranchTargets.begin(); itr != m_BranchTargets.end();)
    {
        if (*itr < m_StartAddress || *itr > Address)
        {
            if (m_CodeType == RspCodeType_SUBROUTINE)
            {
                uint32_t target = *itr;
                if (std::find(m_FunctionCalls.begin(), m_FunctionCalls.end(), target) == m_FunctionCalls.end())
                {
                    m_FunctionCalls.insert(target);
                }
                itr = m_BranchTargets.erase(itr);
            }
            else
            {
                m_Valid = false;
                return;
            }
        }
        else
        {
            itr++;
        }
    }
    for (Addresses::iterator itr = m_FunctionCalls.begin(); itr != m_FunctionCalls.end(); itr++)
    {
        if (m_Functions.find(*itr) == m_Functions.end())
        {
            RspCodeBlockPtr FunctionCall = std::make_unique<RspCodeBlock>(m_System, *itr, RspCodeType_SUBROUTINE, m_EndBlockAddress, m_Functions);
            if (!FunctionCall->IsValid())
            {
                m_Valid = false;
                return;
            }
            m_Functions[*itr] = std::move(FunctionCall);
        }
    }
}
