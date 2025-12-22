#pragma once

#if defined(__amd64__) || defined(_M_X64)
#include "RSPOpcode.h"
#include <stdint.h>
#include <string>

enum class RSPInstructionFlag
{
    Unknown,
    Branch,
    Break,
    GPROperation,
    Jump,
    JumpRegister,
    Load,
    Store,
    MF,
    MT,
    CF,
    CT,
    Vector,
    VectorSetAccum,
    VectorUseAccum,
    VectorLoad,
    VectorStore,
    InvalidOpcode,
};

class RSPInstruction
{
public:
    enum : uint32_t
    {
        UNUSED_OPERAND = ~0u,
    };
    RSPInstruction(uint32_t Address, uint32_t Instruction);
    RSPInstruction & operator=(const RSPInstruction &);
    RSPInstruction(const RSPInstruction & e);

    uint32_t Address() const;
    uint32_t ConditionalBranchTarget() const;
    uint32_t StaticCallTarget() const;
    uint32_t JumpTarget() const;
    bool DelaySlotAffectBranch() const;
    bool IsBranch() const;
    bool IsJump() const;
    bool IsJumpReturn() const;
    bool IsRegisterJump() const;
    bool IsStaticCall() const;
    bool IsConditionalBranch() const;
    bool IsNop() const;
    const char * Name() const;
    const char * Param() const;
    std::string NameAndParam() const;
    uint32_t Value() const;
    bool isVectorOp() const;
    bool isVectorStoreOp() const;
    bool isJump() const;
    bool isBranch() const;
    bool isMfCop2() const;
    bool isMtCop2() const;
    bool ReadAccumLow() const;
    bool ReadAccumMid() const;
    bool ReadAccumHigh() const;
    bool SetAccumLow() const;
    bool SetAccumMid() const;
    bool SetAccumHigh() const;
    RSPInstructionFlag Flag() const;
    uint32_t DestReg() const;
    uint32_t SourceReg0() const;
    uint32_t SourceReg1() const;

private:
    RSPInstruction(void);

    void AnalyzeInstruction(void) const;
    void DecodeName(void) const;
    void DecodeSpecialName(void) const;
    void DecodeRegImmName(void) const;
    void DecodeCop0Name(void) const;
    void DecodeCop2Name(void) const;
    void DecodeLSC2Name(const char LoadStoreIdent) const;

    static const char * ElementSpecifier(uint32_t Element);

    uint32_t m_Address;
    RSPOpcode m_Instruction;
    mutable RSPInstructionFlag m_Flag;
    mutable char m_Name[40];
    mutable char m_Param[200];
    mutable bool m_Analyzed;
    mutable bool m_isBranch;
    mutable bool m_isJump;
    mutable bool m_isGprOp;
    mutable bool m_isVectorOp;
    mutable bool m_isVectorStoreOp;
    mutable bool m_isMfCop2;
    mutable bool m_isMtCop2;
    mutable bool m_ReadAccumLow;
    mutable bool m_ReadAccumMid;
    mutable bool m_ReadAccumHigh;
    mutable bool m_SetAccumLow;
    mutable bool m_SetAccumMid;
    mutable bool m_SetAccumHigh;

    mutable uint32_t m_DestReg;
    mutable uint32_t m_SourceReg0;
    mutable uint32_t m_SourceReg1;
};
#endif