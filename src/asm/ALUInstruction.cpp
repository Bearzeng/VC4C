/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "ALUInstruction.h"

#include "../Values.h"

#include <iostream>
#include <stdexcept>

using namespace vc4c;
using namespace vc4c::qpu_asm;

ALUInstruction::ALUInstruction(const Signaling sig, const Unpack unpack, const Pack pack, const ConditionCode condAdd,
    const ConditionCode condMul, const SetFlag sf, const WriteSwap ws, const Address addOut, const Address mulOut,
    const OpCode& mul, const OpCode& add, const Address addInA, const Address addInB, const InputMultiplex muxAddA,
    const InputMultiplex muxAddB, const InputMultiplex muxMulA, const InputMultiplex muxMulB)
{
    this->setSig(sig);
    this->setUnpack(unpack);
    this->setPack(pack);
    this->setAddCondition(condAdd);
    this->setMulCondition(condMul);
    this->setSetFlag(sf);
    this->setWriteSwap(ws);
    this->setAddOut(addOut);
    this->setMulOut(mulOut);

    this->setMultiplication(mul.opMul);
    this->setAddition(add.opAdd);
    this->setInputA(addInA);
    this->setInputB(addInB);
    this->setAddMultiplexA(muxAddA);
    this->setAddMultiplexB(muxAddB);
    this->setMulMultiplexA(muxMulA);
    this->setMulMultiplexB(muxMulB);

    if((mul != OP_NOP && !mul.runsOnMulALU()) || (add != OP_NOP && !add.runsOnAddALU()))
        throw CompilationError(CompilationStep::CODE_GENERATION, "Opcode specified for wrong ALU");
}

ALUInstruction::ALUInstruction(const Unpack unpack, const Pack pack, const ConditionCode condAdd,
    const ConditionCode condMul, const SetFlag sf, const WriteSwap ws, const Address addOut, const Address mulOut,
    const OpCode& mul, const OpCode& add, const Address addInA, const SmallImmediate addInB,
    const InputMultiplex muxAddA, const InputMultiplex muxAddB, const InputMultiplex muxMulA,
    const InputMultiplex muxMulB)
{
    this->setSig(SIGNAL_ALU_IMMEDIATE);
    this->setUnpack(unpack);
    this->setPack(pack);
    this->setAddCondition(condAdd);
    this->setMulCondition(condMul);
    this->setSetFlag(sf);
    this->setWriteSwap(ws);
    this->setAddOut(addOut);
    this->setMulOut(mulOut);

    this->setMultiplication(mul.opMul);
    this->setAddition(add.opAdd);
    this->setInputA(addInA);
    this->setInputB(static_cast<Address>(addInB));
    this->setAddMultiplexA(muxAddA);
    this->setAddMultiplexB(muxAddB);
    this->setMulMultiplexA(muxMulA);
    this->setMulMultiplexB(muxMulB);

    if((mul != OP_NOP && !mul.runsOnMulALU()) || (add != OP_NOP && !add.runsOnAddALU()))
        throw CompilationError(CompilationStep::CODE_GENERATION, "Opcode specified for wrong ALU", toASMString());
}

std::string ALUInstruction::toASMString() const
{
    const OpCode& opAdd = OpCode::toOpCode(getAddition(), false);
    const OpCode& opMul = OpCode::toOpCode(getMultiplication(), true);
    std::string addPart;
    std::string mulPart;
    bool hasImmediate = getSig() == SIGNAL_ALU_IMMEDIATE;
    std::string addArgs, mulArgs;
    bool addCanUnpack = false;
    bool mulCanUnpack = false;
    if(opAdd.numOperands > 0)
    {
        addArgs.append(", ").append(toInputRegister(getAddMultiplexA(), getInputA(), getInputB(), hasImmediate));
        addCanUnpack |= getAddMultiplexA() == InputMultiplex::REGA;
        addCanUnpack |= getAddMultiplexA() == InputMultiplex::ACC4;
    }
    if(opAdd.numOperands > 1)
    {
        addArgs.append(", ").append(toInputRegister(getAddMultiplexB(), getInputA(), getInputB(), hasImmediate));
        addCanUnpack |= getAddMultiplexB() == InputMultiplex::REGA;
        addCanUnpack |= getAddMultiplexB() == InputMultiplex::ACC4;
    }
    if(opMul.numOperands > 0)
    {
        mulArgs.append(", ").append(toInputRegister(getMulMultiplexA(), getInputA(), getInputB(), hasImmediate));
        mulCanUnpack |= getMulMultiplexA() == InputMultiplex::REGA;
        mulCanUnpack |= getMulMultiplexA() == InputMultiplex::ACC4;
    }
    if(opMul.numOperands > 1)
    {
        mulArgs.append(", ").append(toInputRegister(getMulMultiplexB(), getInputA(), getInputB(), hasImmediate));
        mulCanUnpack |= getMulMultiplexB() == InputMultiplex::REGA;
        mulCanUnpack |= getMulMultiplexB() == InputMultiplex::ACC4;
    }
    addPart = std::string(opAdd.name) +
        (toExtrasString(getSig(), getAddCondition(), getSetFlag(), getUnpack(), getPack(),
             getWriteSwap() == WriteSwap::DONT_SWAP, addCanUnpack) +
            " ") +
        (opAdd != OP_NOP ? toOutputRegister(getWriteSwap() == WriteSwap::DONT_SWAP, getAddOut()) : "") + addArgs;
    mulPart = std::string(opMul.name) +
        (toExtrasString(getSig(), getMulCondition(), getSetFlag(), getUnpack(), getPack(),
             getWriteSwap() == WriteSwap::SWAP, mulCanUnpack) +
            " ") +
        (opMul != OP_NOP ? toOutputRegister(getWriteSwap() == WriteSwap::SWAP, getMulOut()) : "") + mulArgs;
    if(getMulMultiplexA() != InputMultiplex::REGA && getMulMultiplexA() != InputMultiplex::REGB &&
        getMulMultiplexB() != InputMultiplex::REGA && getMulMultiplexB() != InputMultiplex::REGB &&
        getSig() == SIGNAL_ALU_IMMEDIATE &&
        (opAdd == OP_NOP || (getAddMultiplexA() != InputMultiplex::REGB && getAddMultiplexB() != InputMultiplex::REGB)))
    {
        // both inputs for mul are accumulators, an immediate value is used
        // and the ADD ALU executes NOP or both inputs from the ADD ALU are not on
        // register-file B
        // -> vector rotation
        mulPart.append(" ").append(static_cast<SmallImmediate>(getInputB()).to_string());
    }
    std::string s;
    if(opAdd != OP_NOP)
    {
        if(opMul != OP_NOP)
            s = (addPart + "; ") + mulPart;
        else
            s = addPart;
    }
    else if(opMul != OP_NOP)
        s = mulPart;
    else
        s = addPart;

    return s;
}

static Register getInputRegister(InputMultiplex mux, Address regA, Address regB)
{
    switch(mux)
    {
    case InputMultiplex::ACC0:
        return REG_ACC0;
    case InputMultiplex::ACC1:
        return REG_ACC1;
    case InputMultiplex::ACC2:
        return REG_ACC2;
    case InputMultiplex::ACC3:
        return REG_ACC3;
    case InputMultiplex::ACC4:
        return REG_TMU_OUT;
    case InputMultiplex::ACC5:
        return REG_ACC5;
    case InputMultiplex::REGA:
        return Register{RegisterFile::PHYSICAL_A, regA};
    case InputMultiplex::REGB:
        return Register{RegisterFile::PHYSICAL_B, regB};
    }
    throw CompilationError(CompilationStep::CODE_GENERATION, "Unknown input register");
}

Register ALUInstruction::getAddFirstOperand() const
{
    if(getSig() == SIGNAL_ALU_IMMEDIATE && getAddMultiplexA() == InputMultiplex::REGB)
        return REG_NOP;
    if(getAddition() == OP_NOP.opAdd)
        return REG_NOP;
    return getInputRegister(getAddMultiplexA(), getInputA(), getInputB());
}

Register ALUInstruction::getAddSecondOperand() const
{
    if(getSig() == SIGNAL_ALU_IMMEDIATE && getAddMultiplexB() == InputMultiplex::REGB)
        return REG_NOP;
    if(getAddition() == OP_NOP.opAdd)
        return REG_NOP;
    return getInputRegister(getAddMultiplexB(), getInputA(), getInputB());
}

Register ALUInstruction::getMulFirstOperand() const
{
    if(getSig() == SIGNAL_ALU_IMMEDIATE && getMulMultiplexA() == InputMultiplex::REGB)
        return REG_NOP;
    if(getMultiplication() == OP_NOP.opMul)
        return REG_NOP;
    return getInputRegister(getMulMultiplexA(), getInputA(), getInputB());
}

Register ALUInstruction::getMulSecondOperand() const
{
    if(getSig() == SIGNAL_ALU_IMMEDIATE && getMulMultiplexB() == InputMultiplex::REGB)
        return REG_NOP;
    if(getMultiplication() == OP_NOP.opMul)
        return REG_NOP;
    return getInputRegister(getMulMultiplexB(), getInputA(), getInputB());
}
