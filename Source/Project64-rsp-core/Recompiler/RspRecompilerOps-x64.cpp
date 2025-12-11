#if defined(__amd64__) || defined(_M_X64)

#include "RspRecompilerOps-x64.h"
#include <Common/StdString.h>
#include <Project64-rsp-core/Recompiler/RspAssembler.h>
#include <Project64-rsp-core/Recompiler/RspCodeBlock.h>
#include <Project64-rsp-core/Recompiler/RspProfiling.h>
#include <Project64-rsp-core/cpu/RSPInstruction.h>
#include <Project64-rsp-core/cpu/RspSystem.h>
#include <Settings/Settings.h>

extern p_Recompfunc RSP_Recomp_RegImm[32];
extern p_Recompfunc RSP_Recomp_Special[64];
extern p_Recompfunc RSP_Recomp_Cop0[32];
extern p_Recompfunc RSP_Recomp_Cop2[32];
extern p_Recompfunc RSP_Recomp_Vector[64];
extern p_Recompfunc RSP_Recomp_Lc2[32];
extern p_Recompfunc RSP_Recomp_Sc2[32];

uint32_t BranchCompare = 0;

CRSPRecompilerOps::CRSPRecompilerOps(CRSPSystem & System, CRSPRecompiler & Recompiler) :
    m_System(System),
    m_Recompiler(Recompiler),
    m_OpCode(Recompiler.m_OpCode),
    m_CompilePC(Recompiler.m_CompilePC),
    m_CurrentBlock(Recompiler.m_CurrentBlock),
    m_NextInstruction(Recompiler.m_NextInstruction),
    m_Reg(System.m_Reg),
    m_GPR(System.m_Reg.m_GPR),
    m_Vect(System.m_Reg.m_Vect),
    m_ACCUM(System.m_Reg.m_ACCUM),
    m_VCOL(System.m_Reg.m_VCOL),
    m_VCOH(System.m_Reg.m_VCOH),
    m_VCCL(System.m_Reg.m_VCCL),
    m_VCCH(System.m_Reg.m_VCCH),
    m_VCE(System.m_Reg.m_VCE),
    m_Assembler(Recompiler.m_Assembler),
    m_DelayAffectBranch(false)
{
}

void CRSPRecompilerOps::Cheat_r4300iOpcode(RSPOp::Func FunctAddress, const char * FunctName)
{
    m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str()).c_str());
    if (SyncCPU)
    {
        m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", m_CompilePC);
    }
    m_Assembler->MoveConstToVariable(&m_System.m_OpCode.Value, "m_OpCode.Value", m_OpCode.Value);
    m_Assembler->CallThis(&RSPSystem.m_Op, AddressOf(FunctAddress), FunctName);
}

// Opcode functions

void CRSPRecompilerOps::SPECIAL(void)
{
    (this->*RSP_Recomp_Special[m_OpCode.funct])();
}

void CRSPRecompilerOps::REGIMM(void)
{
    (this->*RSP_Recomp_RegImm[m_OpCode.rt])();
}

void CRSPRecompilerOps::J(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str()).c_str());
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        uint32_t Target = (m_OpCode.target << 2) & 0x1FFC;
        asmjit::Label Jump;
        if (m_CurrentBlock->IsEnd(m_CompilePC) && m_CurrentBlock->CodeType() == RspCodeType_TASK)
        {
            m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", Target);
            ExitCodeBlock();
        }
        else if (m_Recompiler.FindBranchJump(Target, Jump))
        {
            m_Assembler->JmpLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("J error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::JAL(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str()).c_str());
        m_Assembler->MoveConstToVariable(&m_GPR[31].UW, "RA.W", (m_CompilePC + 8) & 0x1FFC);
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE || m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE_BRANCH_TARGET)
    {
        uint32_t Target = (m_OpCode.target << 2) & 0x1FFC;
        if (m_CurrentBlock->IsEnd(m_CompilePC) && m_CurrentBlock->CodeType() == RspCodeType_TASK)
        {
            m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", Target);
            ExitCodeBlock();
        }
        else
        {
            const RspCodeBlock * FunctionBlock = m_CurrentBlock ? m_CurrentBlock->GetFunctionBlock(Target) : nullptr;
            if (FunctionBlock != nullptr)
            {
                if (SyncCPU)
                {
                    m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", Target);
                    m_Assembler->mov(asmjit::x86::rdx, asmjit::imm(0x2000));
                    m_Assembler->mov(asmjit::x86::r8, asmjit::imm(Target & 0xFFF));
                    m_Assembler->CallThis(RSPSystem.SyncSystem(), AddressOf(&CRSPSystem::ExecuteOps), "CRSPSystem::ExecuteOps");
                    m_Assembler->CallThis(&RSPSystem, AddressOf(&CRSPSystem::BasicSyncCheck), "CRSPSystem::BasicSyncCheck");
                }
                m_Assembler->CallFunc(FunctionBlock->GetCompiledLocation(), stdstr_f("0x%X", Target).c_str());
            }
            else
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
        }

        if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE_BRANCH_TARGET)
        {
            asmjit::Label Jump = m_Assembler->newLabel();
            m_Assembler->JmpLabel(stdstr_f("0x%X_continue", m_CompilePC).c_str(), Jump);
            m_Recompiler.CompileOpcode((m_CompilePC + 4) & 0x1FFC);
            m_Assembler->bind(Jump);
        }
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
        //CompilerWarning(stdstr_f("J error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        //BreakPoint();
    }
}

void CRSPRecompilerOps::BEQ(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        RSPInstruction Instruction(m_CompilePC, m_OpCode.Value);
        m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, Instruction.NameAndParam().c_str()).c_str());
        m_DelayAffectBranch = Instruction.DelaySlotAffectBranch();
        if (!m_DelayAffectBranch)
        {
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        if (m_OpCode.rt == 0)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
        }
        else if (m_OpCode.rs == 0)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), 0);
        }
        else
        {
            m_Assembler->MoveVariableToX86reg(asmjit::x86::r11, &m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt));
            m_Assembler->CompX86regToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), asmjit::x86::r11);
        }
        m_Assembler->SetzVariable(&BranchCompare, "BranchCompare");
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0x1FFC;

        if (m_OpCode.rs == 0 && m_OpCode.rt == 0)
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            JmpLabel32("BranchToJump", 0);
            m_Recompiler.Branch_AddRef(Target, (uint32_t *)(RecompPos - 4));
            m_NextInstruction = RSPPIPELINE_FINISH_SUB_BLOCK;
#endif
            return;
        }

        if (m_OpCode.rs == 0 && m_OpCode.rt == 0)
        {
            return;
        }

        if (!m_DelayAffectBranch)
        {
            if (m_OpCode.rt == 0)
            {
                m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
            }
            else if (m_OpCode.rs == 0)
            {
                m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), 0);
            }
            else
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
                m_Assembler->MoveVariableToX86reg(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), x86_EAX);
                m_Assembler->CompX86regToVariable(x86_EAX, &m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs));
#endif
            }
            if (Target == m_CurrentBlock->GetDispatchAddress())
            {
                asmjit::Label ContinueLabel = m_Assembler->newLabel();
                m_Assembler->JneLabel(stdstr_f("Continue-%X", m_CompilePC).c_str(), ContinueLabel);
                m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", Target);
                ExitCodeBlock();
                m_Assembler->bind(ContinueLabel);
            }
            else
            {
                asmjit::Label Jump;
                if (!m_Recompiler.FindBranchJump(Target, Jump))
                {
                    g_Notify->BreakPoint(__FILE__, __LINE__);
                }
                m_Assembler->JeLabel(stdstr_f("0x%X", Target).c_str(), Jump);
            }
        }
        else
        {
            asmjit::Label Jump;
            if (!m_Recompiler.FindBranchJump(Target, Jump))
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
            m_Assembler->CompConstToVariable(&BranchCompare, "BranchCompare", true);
            m_Assembler->JeLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_EXIT_DONE)
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0xFFC;
        CompileBranchExit(Target, m_CompilePC + 8);
#endif
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("BNE error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::BNE(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        RSPInstruction Instruction(m_CompilePC, m_OpCode.Value);
        m_Recompiler.Log("  %X %s", m_CompilePC, Instruction.NameAndParam().c_str());
        m_DelayAffectBranch = Instruction.DelaySlotAffectBranch();
        if (!m_DelayAffectBranch)
        {
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        if (m_OpCode.rs == 0 && m_OpCode.rt == 0)
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            MoveConstByteToVariable(0, &BranchCompare, "BranchCompare");
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
#endif
            return;
        }

        if (m_OpCode.rt == 0)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
        }
        else if (m_OpCode.rs == 0)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), 0);
        }
        else
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            MoveVariableToX86reg(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), x86_EAX);
            CompX86regToVariable(x86_EAX, &m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs));
#endif
        }
        m_Assembler->SetnzVariable(&BranchCompare, "BranchCompare");
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0x1FFC;

        if (m_OpCode.rs == 0 && m_OpCode.rt == 0)
        {
            return;
        }

        if (!m_DelayAffectBranch)
        {
            if (m_OpCode.rt == 0)
            {
                m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
            }
            else if (m_OpCode.rs == 0)
            {
                m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), 0);
            }
            else
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
                MoveVariableToX86reg(&m_GPR[m_OpCode.rt].W, GPR_Name(m_OpCode.rt), x86_EAX);
                CompX86regToVariable(x86_EAX, &m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs));
#endif
            }
            asmjit::Label Jump;
            if (m_Recompiler.FindBranchJump(Target, Jump))
            {
                m_Assembler->JneLabel(stdstr_f("0x%X", Target).c_str(), Jump);
            }
            else
            {
                const RspCodeBlock * FunctionBlock = m_CurrentBlock ? m_CurrentBlock->GetFunctionBlock(Target) : nullptr;
                if (FunctionBlock != nullptr)
                {
                    asmjit::Label ContinuJump = m_Assembler->newLabel();
                    m_Assembler->JeLabel(stdstr_f("continue_0x%X", m_CompilePC).c_str(), ContinuJump);
                    m_Assembler->add(asmjit::x86::rsp, FunctionStackSize);
                    m_Assembler->JFunc(FunctionBlock->GetCompiledLocation(), stdstr_f("0x%X", Target).c_str());
                    m_Assembler->bind(ContinuJump);
                }
                else
                {
                    g_Notify->BreakPoint(__FILE__, __LINE__);
                }
            }
        }
        else
        {
            asmjit::Label Jump;
            if (!m_Recompiler.FindBranchJump(Target, Jump))
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
            m_Assembler->CompConstToVariable(&BranchCompare, "BranchCompare", true);
            m_Assembler->JeLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_EXIT_DONE)
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0xFFC;
        CompileBranchExit(Target, m_CompilePC + 8);
#endif
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("BNE error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::BLEZ(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        RSPInstruction Instruction(m_CompilePC, m_OpCode.Value);
        m_Recompiler.Log("  %X %s", m_CompilePC, Instruction.NameAndParam().c_str());
        if (m_OpCode.rs == 0)
        {
            m_DelayAffectBranch = false;
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        m_DelayAffectBranch = Instruction.DelaySlotAffectBranch();
        if (!m_DelayAffectBranch)
        {
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompConstToVariable(0, &m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs));
        SetleVariable(&BranchCompare, "BranchCompare");
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
#endif
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0x1FFC;

        if (m_OpCode.rs == 0)
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            JmpLabel32("BranchToJump", 0);
            m_Recompiler.Branch_AddRef(Target, (uint32_t *)(RecompPos - 4));
            m_NextInstruction = RSPPIPELINE_FINISH_SUB_BLOCK;
#endif
            return;
        }
        if (!m_DelayAffectBranch)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
            asmjit::Label Jump;
            if (!m_Recompiler.FindBranchJump(Target, Jump))
            {
                g_Notify->BreakPoint(__FILE__, __LINE__);
            }
            m_Assembler->JleLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
        else
        {
            // Take a look at the branch compare variable
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            CompConstToVariable(true, &BranchCompare, "BranchCompare");
            JeLabel32("BranchLessEqual", 0);
#endif
        }
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_EXIT_DONE)
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0xFFC;
        CompileBranchExit(Target, m_CompilePC + 8);
#endif
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("BGTZ error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::BGTZ(void)
{
    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        RSPInstruction Instruction(m_CompilePC, m_OpCode.Value);
        m_Recompiler.Log("  %X %s", m_CompilePC, Instruction.NameAndParam().c_str());
        if (m_OpCode.rs == 0)
        {
            m_DelayAffectBranch = false;
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        m_DelayAffectBranch = Instruction.DelaySlotAffectBranch();
        if (!m_DelayAffectBranch)
        {
            m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
            return;
        }
        m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
        m_Assembler->SetgVariable(&BranchCompare, "BranchCompare");
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0x1FFC;

        if (m_OpCode.rs == 0)
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
            m_NextInstruction = RSPPIPELINE_FINISH_SUB_BLOCK;
#endif
            return;
        }
        asmjit::Label Jump;
        if (!m_Recompiler.FindBranchJump(Target, Jump))
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
        if (!m_DelayAffectBranch)
        {
            m_Assembler->CompConstToVariable(&m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs), 0);
            m_Assembler->JgLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
        else
        {
            m_Assembler->CompConstToVariable(&BranchCompare, "BranchCompare", true);
            m_Assembler->JeLabel(stdstr_f("0x%X", Target).c_str(), Jump);
        }
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_EXIT_DONE)
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        uint32_t Target = (m_CompilePC + ((short)m_OpCode.offset << 2) + 4) & 0xFFC;
        CompileBranchExit(Target, m_CompilePC + 8);
#endif
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("BGTZ error\nWeird Delay Slot.\n\nNextInstruction = %X\nEmulation will now stop", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::ADDI(void)
{
    Cheat_r4300iOpcode(&RSPOp::ADDI, "RSPOp::ADDI");
}

void CRSPRecompilerOps::ADDIU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::SLTI(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::SLTIU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::ANDI(void)
{
    Cheat_r4300iOpcode(&RSPOp::ANDI, "RSPOp::ANDI");
}

void CRSPRecompilerOps::ORI(void)
{
    Cheat_r4300iOpcode(&RSPOp::ORI, "RSPOp::ORI");
}

void CRSPRecompilerOps::XORI(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::LUI(void)
{
    Cheat_r4300iOpcode(&RSPOp::LUI, "RSPOp::LUI");
}

void CRSPRecompilerOps::COP0(void)
{
    (this->*RSP_Recomp_Cop0[m_OpCode.rs])();
}

void CRSPRecompilerOps::COP2(void)
{
    (this->*RSP_Recomp_Cop2[m_OpCode.rs])();
}

void CRSPRecompilerOps::LB(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::LH(void)
{
    Cheat_r4300iOpcode(&RSPOp::LH, "RSPOp::LH");
}

void CRSPRecompilerOps::LW(void)
{
    Cheat_r4300iOpcode(&RSPOp::LW, "RSPOp::LW");
}

void CRSPRecompilerOps::LBU(void)
{
    Cheat_r4300iOpcode(&RSPOp::LBU, "RSPOp::LBU");
}

void CRSPRecompilerOps::LHU(void)
{
    Cheat_r4300iOpcode(&RSPOp::LHU, "RSPOp::LHU");
}

void CRSPRecompilerOps::LWU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::SB(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::SH(void)
{
    Cheat_r4300iOpcode(&RSPOp::SH, "RSPOp::SH");
}

void CRSPRecompilerOps::SW(void)
{
    Cheat_r4300iOpcode(&RSPOp::SW, "RSPOp::SW");
}

void CRSPRecompilerOps::LC2(void)
{
    (this->*RSP_Recomp_Lc2[m_OpCode.rd])();
}

void CRSPRecompilerOps::SC2(void)
{
    (this->*RSP_Recomp_Sc2[m_OpCode.rd])();
}

// R4300i Opcodes: Special

void CRSPRecompilerOps::Special_SLL(void)
{
    if (m_OpCode.rd == 0)
    {
        m_Recompiler.Log("  %X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str());
        return;
    }
    Cheat_r4300iOpcode(&RSPOp::Special_SLL, "RSPOp::Special_SLL");
}

void CRSPRecompilerOps::Special_SRL(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_SRL, "RSPOp::Special_SRL");
}

void CRSPRecompilerOps::Special_SRA(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_SLLV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_SRLV(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_SRLV, "RSPOp::Special_SRLV");
}

void CRSPRecompilerOps::Special_SRAV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_JR(void)
{
    //uint8_t * Jump = nullptr;

    if (m_NextInstruction == RSPPIPELINE_NORMAL)
    {
        m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str()).c_str());
        m_Assembler->MoveVariableToX86reg(asmjit::x86::eax, &m_GPR[m_OpCode.rs].W, GPR_Name(m_OpCode.rs));
        m_Assembler->and_(asmjit::x86::eax, 0x1FFC);
        m_Assembler->MoveX86regToVariable(m_System.m_SP_PC_REG, "RSP PC", asmjit::x86::eax);
        m_NextInstruction = RSPPIPELINE_DO_DELAY_SLOT;
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_DONE)
    {
        if (m_CurrentBlock && m_CurrentBlock->CodeType() == RspCodeType_SUBROUTINE)
        {
            ExitCodeBlock();
        }
        else
        {
            g_Notify->BreakPoint(__FILE__, __LINE__);
        }
#ifdef tofix
        MoveVariableToX86reg(m_System.m_SP_PC_REG, "RSP PC", x86_EAX);
        AddVariableToX86reg(x86_EAX, &JumpTable, "JumpTable");
        MoveX86regPointerToX86reg(x86_EAX, x86_EAX);

        TestX86RegToX86Reg(x86_EAX, x86_EAX);
        JeLabel8("Null", 0);
        Jump = RecompPos - 1;
        JumpX86Reg(x86_EAX);

        x86_SetBranch8b(Jump, RecompPos);
        CPU_Message(" Null:");
        if (CRSPSettings::CPUMethod() == RSPCpuMethod::HighLevelEmulation)
        {
            BreakPoint();
        }
        Ret();
        ChangedPC = false;
        m_NextInstruction = RSPPIPELINE_FINISH_SUB_BLOCK;
#endif
    }
    else if (m_NextInstruction == RSPPIPELINE_DELAY_SLOT_EXIT_DONE)
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        m_NextInstruction = RSPPIPELINE_FINISH_SUB_BLOCK;
        Ret();
#endif
    }
    else
    {
        g_Notify->BreakPoint(__FILE__, __LINE__);
#ifdef tofix
        CompilerWarning(stdstr_f("WTF\n\nJR\nNextInstruction = %X", m_NextInstruction).c_str());
        BreakPoint();
#endif
    }
}

void CRSPRecompilerOps::Special_JALR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_BREAK(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_ADD(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_ADD, "RSPOp::Special_ADD");
}

void CRSPRecompilerOps::Special_ADDU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_SUB(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_SUB, "RSPOp::Special_SUB");
}

void CRSPRecompilerOps::Special_SUBU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_AND(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_AND, "RSPOp::Special_AND");
}

void CRSPRecompilerOps::Special_OR(void)
{
    Cheat_r4300iOpcode(&RSPOp::Special_OR, "RSPOp::Special_OR");
}

void CRSPRecompilerOps::Special_XOR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_NOR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_SLT(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Special_SLTU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

// R4300i Opcodes: RegImm
void CRSPRecompilerOps::RegImm_BLTZ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::RegImm_BGEZ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::RegImm_BLTZAL(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::RegImm_BGEZAL(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

// COP0 functions

void CRSPRecompilerOps::Cop0_MF(void)
{
    Cheat_r4300iOpcode(&RSPOp::Cop0_MF, "RSPOp::Cop0_MF");
}

void CRSPRecompilerOps::Cop0_MT(void)
{
    Cheat_r4300iOpcode(&RSPOp::Cop0_MT, "RSPOp::Cop0_MT");
}

// COP2 functions

void CRSPRecompilerOps::Cop2_MF(void)
{
    Cheat_r4300iOpcode(&RSPOp::Cop2_MF, "RSPOp::Cop2_MF");
}

void CRSPRecompilerOps::Cop2_CF(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Cop2_MT(void)
{
    Cheat_r4300iOpcode(&RSPOp::Cop2_MT, "RSPOp::Cop2_MT");
}

void CRSPRecompilerOps::Cop2_CT(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::COP2_VECTOR(void)
{
    (this->*RSP_Recomp_Vector[m_OpCode.funct])();
}

// Vector functions

void CRSPRecompilerOps::Vector_VMULF(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMULF, "RSPOp::Vector_VMULF");
}

void CRSPRecompilerOps::Vector_VMULU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRNDN(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRNDP(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMULQ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMUDL(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMUDL, "RSPOp::Vector_VMUDL");
}

void CRSPRecompilerOps::Vector_VMUDM(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMUDM, "RSPOp::Vector_VMUDM");
}

void CRSPRecompilerOps::Vector_VMUDN(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMUDN, "RSPOp::Vector_VMUDN");
}

void CRSPRecompilerOps::Vector_VMUDH(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMUDH, "RSPOp::Vector_VMUDH");
}

void CRSPRecompilerOps::Vector_VMACF(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMACF, "RSPOp::Vector_VMACF");
}

void CRSPRecompilerOps::Vector_VMACU(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMACQ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMADL(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMADM(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMADM, "RSPOp::Vector_VMADM");
}

void CRSPRecompilerOps::Vector_VMADN(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMADN, "RSPOp::Vector_VMADN");
}

void CRSPRecompilerOps::Vector_VMADH(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VMADH, "RSPOp::Vector_VMADH");
}

void CRSPRecompilerOps::Vector_VADD(void)
{
    m_Assembler->MoveConstToVariable(m_System.m_SP_PC_REG, "RSP PC", (m_CompilePC & 0xFFF));
    m_Assembler->MoveConstToVariable(&m_System.m_OpCode.Value, "m_OpCode.Value", m_OpCode.Value);
    m_Assembler->comment(stdstr_f("%X %s", m_CompilePC, RSPInstruction(m_CompilePC, m_OpCode.Value).NameAndParam().c_str()).c_str());
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)&m_Vect[m_OpCode.vs].u64(0));
    m_Assembler->movdqa(asmjit::x86::xmm0, asmjit::x86::ptr(asmjit::x86::r11));
    LoadVectorRegister(asmjit::x86::xmm1, m_OpCode.vt, m_OpCode.e);
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)m_VCOL.Value());
    m_Assembler->movdqa(asmjit::x86::xmm2, asmjit::x86::ptr(asmjit::x86::r11));
    m_Assembler->movdqa(asmjit::x86::xmm3, asmjit::x86::xmm0);
    m_Assembler->paddw(asmjit::x86::xmm3, asmjit::x86::xmm1);
    m_Assembler->paddw(asmjit::x86::xmm3, asmjit::x86::xmm2);
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)&m_ACCUM.Low(0));
    m_Assembler->movdqa(asmjit::x86::ptr(asmjit::x86::r11), asmjit::x86::xmm3);
    m_Assembler->paddsw(asmjit::x86::xmm0, asmjit::x86::xmm1);
    m_Assembler->paddsw(asmjit::x86::xmm0, asmjit::x86::xmm2);
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)&m_Vect[m_OpCode.vd].u64(0));
    m_Assembler->movdqa(asmjit::x86::ptr(asmjit::x86::r11), asmjit::x86::xmm0);
    m_Assembler->pxor(asmjit::x86::xmm0, asmjit::x86::xmm0);
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)m_VCOL.Value());
    m_Assembler->movdqa(asmjit::x86::ptr(asmjit::x86::r11), asmjit::x86::xmm0);
    m_Assembler->mov(asmjit::x86::r11, (uint64_t)m_VCOH.Value());
    m_Assembler->movdqa(asmjit::x86::ptr(asmjit::x86::r11), asmjit::x86::xmm0);
}

void CRSPRecompilerOps::Vector_VSUB(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VSUB, "RSPOp::Vector_VSUB");
}

void CRSPRecompilerOps::Vector_VABS(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VADDC(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VADDC, "RSPOp::Vector_VADDC");
}

void CRSPRecompilerOps::Vector_VSUBC(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VSUBC, "RSPOp::Vector_VSUBC");
}

void CRSPRecompilerOps::Vector_VSAW(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VSAW, "RSPOp::Vector_VSAW");
}

void CRSPRecompilerOps::Vector_VLT(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VEQ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VNE(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VGE(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VGE, "RSPOp::Vector_VGE");
}

void CRSPRecompilerOps::Vector_VCL(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VCL, "RSPOp::Vector_VCL");
}

void CRSPRecompilerOps::Vector_VCH(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VCR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMRG(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VAND(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VAND, "RSPOp::Vector_VAND");
}

void CRSPRecompilerOps::Vector_VNAND(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VOR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VNOR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VXOR(void)
{
    Cheat_r4300iOpcode(&RSPOp::Vector_VXOR, "RSPOp::Vector_VXOR");
}

void CRSPRecompilerOps::Vector_VNXOR(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRCP(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRCPL(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRCPH(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VMOV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRSQ(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRSQL(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VRSQH(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Vector_VNOOP(void)
{
}

void CRSPRecompilerOps::Vector_Reserved(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

// LC2 functions

void CRSPRecompilerOps::Opcode_LBV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LSV(void)
{
    Cheat_r4300iOpcode(&RSPOp::LSV, "RSPOp::LSV");
}

void CRSPRecompilerOps::Opcode_LLV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LDV(void)
{
    Cheat_r4300iOpcode(&RSPOp::LDV, "RSPOp::LDV");
}

void CRSPRecompilerOps::Opcode_LQV(void)
{
    Cheat_r4300iOpcode(&RSPOp::LQV, "RSPOp::LQV");
}

void CRSPRecompilerOps::Opcode_LRV(void)
{
    Cheat_r4300iOpcode(&RSPOp::LRV, "RSPOp::LRV");
}

void CRSPRecompilerOps::Opcode_LPV(void)
{
    Cheat_r4300iOpcode(&RSPOp::LPV, "RSPOp::LPV");
}

void CRSPRecompilerOps::Opcode_LUV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LHV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LFV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LWV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_LTV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

// SC2 functions

void CRSPRecompilerOps::Opcode_SBV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SSV(void)
{
    Cheat_r4300iOpcode(&RSPOp::SSV, "RSPOp::SSV");
}

void CRSPRecompilerOps::Opcode_SLV(void)
{
    Cheat_r4300iOpcode(&RSPOp::SLV, "RSPOp::SLV");
}

void CRSPRecompilerOps::Opcode_SDV(void)
{
    Cheat_r4300iOpcode(&RSPOp::SDV, "RSPOp::SDV");
}

void CRSPRecompilerOps::Opcode_SQV(void)
{
    Cheat_r4300iOpcode(&RSPOp::SQV, "RSPOp::SQV");
}

void CRSPRecompilerOps::Opcode_SRV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SPV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SUV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SHV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SFV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_STV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::Opcode_SWV(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

// Other functions

void CRSPRecompilerOps::UnknownOpcode(void)
{
    g_Notify->BreakPoint(__FILE__, __LINE__);
}

void CRSPRecompilerOps::EnterCodeBlock(void)
{
    m_Assembler->sub(asmjit::x86::rsp, FunctionStackSize);
    if (Profiling && m_CurrentBlock->CodeType() == RspCodeType_TASK)
    {
        m_Assembler->mov(asmjit::x86::rcx, asmjit::imm((uintptr_t)m_CompilePC));
        m_Assembler->CallFunc(AddressOf(&StartTimer), "StartTimer");
    }
}

void CRSPRecompilerOps::ExitCodeBlock(void)
{
    if (Profiling && m_CurrentBlock->CodeType() == RspCodeType_TASK)
    {
        m_Assembler->CallFunc(AddressOf(&StopTimer), "StopTimer");
    }
    m_Assembler->add(asmjit::x86::rsp, FunctionStackSize);
    m_Assembler->ret();
}

void CRSPRecompilerOps::LoadVectorRegister(asmjit::x86::Xmm xmmReg, int32_t vectorReg, int32_t e)
{
    if (e < 8)
    {
        m_Assembler->mov(asmjit::x86::r11, (uint64_t)&m_Vect[vectorReg].u64(0));
        m_Assembler->movdqa(xmmReg, asmjit::x86::ptr(asmjit::x86::r11));
        if (e > 1)
        {
            switch (e)
            {
            case 2: // 0q
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(3, 3, 1, 1));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(3, 3, 1, 1));
                break;
            case 3: // 1q
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(2, 2, 0, 0));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(2, 2, 0, 0));
                break;
            case 4: // 0h
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(3, 3, 3, 3));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(3, 3, 3, 3));
                break;
            case 5: // 1h
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(2, 2, 2, 2));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(2, 2, 2, 2));
                break;
            case 6: // 2h
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(1, 1, 1, 1));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(1, 1, 1, 1));
                break;
            case 7: // 3h
                m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(0, 0, 0, 0));
                m_Assembler->pshufhw(xmmReg, xmmReg, _MM_SHUFFLE(0, 0, 0, 0));
                break;
            }
        }
    }
    else
    {
        m_Assembler->mov(asmjit::x86::r11, (uint64_t)&m_Vect[vectorReg].s16(e));
        m_Assembler->movzx(asmjit::x86::eax, asmjit::x86::word_ptr(asmjit::x86::r11));
        m_Assembler->movd(xmmReg, asmjit::x86::eax);
        m_Assembler->pshuflw(xmmReg, xmmReg, _MM_SHUFFLE(0, 0, 0, 0));
        m_Assembler->pshufd(xmmReg, xmmReg, _MM_SHUFFLE(0, 0, 0, 0));
    }
}

#endif