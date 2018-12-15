/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "OpCodes.h"

#include "../Values.h"
#include "../intrinsics/Operators.h"
#include "CompilationError.h"

#include <cmath>
#include <map>

using namespace vc4c;

std::string ConditionCode::to_string() const
{
    switch(*this)
    {
    case COND_ALWAYS:
        return "";
    case COND_CARRY_CLEAR:
        return "ifcc";
    case COND_CARRY_SET:
        return "ifc";
    case COND_NEGATIVE_CLEAR:
        return "ifnc";
    case COND_NEGATIVE_SET:
        return "ifn";
    case COND_NEVER:
        return "never";
    case COND_ZERO_CLEAR:
        return "ifzc";
    case COND_ZERO_SET:
        return "ifz";
    }
    throw CompilationError(
        CompilationStep::CODE_GENERATION, "Unsupported condition", std::to_string(static_cast<unsigned>(value)));
}

ConditionCode ConditionCode::invert() const
{
    switch(*this)
    {
    case COND_ALWAYS:
        return COND_NEVER;
    case COND_CARRY_CLEAR:
        return COND_CARRY_SET;
    case COND_CARRY_SET:
        return COND_CARRY_CLEAR;
    case COND_NEGATIVE_CLEAR:
        return COND_NEGATIVE_SET;
    case COND_NEGATIVE_SET:
        return COND_NEGATIVE_CLEAR;
    case COND_NEVER:
        return COND_ALWAYS;
    case COND_ZERO_CLEAR:
        return COND_ZERO_SET;
    case COND_ZERO_SET:
        return COND_ZERO_CLEAR;
    }
    throw CompilationError(CompilationStep::CODE_GENERATION, "Unsupported conditions", to_string());
}

bool ConditionCode::isInversionOf(const ConditionCode other) const
{
    return other == invert();
}

BranchCond ConditionCode::toBranchCondition() const
{
    switch(value)
    {
    case COND_ALWAYS.value:
        return BranchCond::ALWAYS;
    case COND_CARRY_CLEAR.value:
        return BranchCond::ALL_C_CLEAR;
    case COND_CARRY_SET.value:
        return BranchCond::ANY_C_SET;
    case COND_NEGATIVE_CLEAR.value:
        return BranchCond::ALL_N_CLEAR;
    case COND_NEGATIVE_SET.value:
        return BranchCond::ANY_N_SET;
    case COND_ZERO_CLEAR.value:
        return BranchCond::ALL_Z_CLEAR;
    case COND_ZERO_SET.value:
        return BranchCond::ANY_Z_SET;
    }
    throw CompilationError(CompilationStep::CODE_GENERATION, "Invalid condition for branch", to_string());
}

std::string Signaling::to_string() const
{
    switch(*this)
    {
    case SIGNAL_LOAD_ALPHA:
        return "loada";
    case SIGNAL_ALU_IMMEDIATE:
        return "imm";
    case SIGNAL_BRANCH:
        return "br";
    case SIGNAL_LOAD_COLOR:
        return "loadc";
    case SIGNAL_LOAD_COLOR_END:
        return "loadc_end";
    case SIGNAL_LOAD_COVERAGE:
        return "loadcov";
    case SIGNAL_THREAD_SWITCH_LAST:
        return "lthrsw";
    case SIGNAL_LOAD_IMMEDIATE:
        return "load_imm";
    case SIGNAL_LOAD_TMU0:
        return "load_tmu0";
    case SIGNAL_LOAD_TMU1:
        return "load_tmu1";
    case SIGNAL_NONE:
        return "";
    case SIGNAL_END_PROGRAM:
        return "thrend";
    case SIGNAL_UNLOCK_SCORE:
        return "scoreu";
    case SIGNAL_SOFT_BREAK:
        return "bkpt";
    case SIGNAL_SWITCH_THREAD:
        return "thrsw";
    case SIGNAL_WAIT_FOR_SCORE:
        return "scorew";
    }
    throw CompilationError(
        CompilationStep::CODE_GENERATION, "Unsupported signal", std::to_string(static_cast<unsigned>(value)));
}

bool Signaling::hasSideEffects() const
{
    return *this != SIGNAL_NONE && *this != SIGNAL_ALU_IMMEDIATE && *this != SIGNAL_LOAD_IMMEDIATE;
}

bool Signaling::triggersReadOfR4() const
{
    return *this == SIGNAL_LOAD_ALPHA || *this == SIGNAL_LOAD_COLOR || *this == SIGNAL_LOAD_COLOR_END ||
        *this == SIGNAL_LOAD_COVERAGE || *this == SIGNAL_LOAD_TMU0 || *this == SIGNAL_LOAD_TMU1;
}

std::string Unpack::to_string() const
{
    // http://maazl.de/project/vc4asm/doc/extensions.html#pack
    switch(*this)
    {
    case UNPACK_NOP:
    case UNPACK_NOP_PM:
        return "";
    case UNPACK_16A_32:
        return "sextLow16to32";
    case UNPACK_16B_32:
        return "sextHigh16to32";
    case UNPACK_8888_32:
        return "replMSB";
    case UNPACK_8A_32:
        return "zextByte0To32";
    case UNPACK_8B_32:
        return "zextByte1To32";
    case UNPACK_8C_32:
        return "zextByte2To32";
    case UNPACK_8D_32:
        return "zextByte3To32";
    case UNPACK_R4_16A_32:
        return "r4HalfLowToFloat";
    case UNPACK_R4_16B_32:
        return "r4HalfHighToFloat";
    case UNPACK_R4_ALPHA_REPLICATE:
        return "r4ReplAlpha";
    case UNPACK_R4_COLOR0:
        return "r4Byte0ToFloat";
    case UNPACK_R4_COLOR1:
        return "r4Byte1ToFloat";
    case UNPACK_R4_COLOR2:
        return "r4Byte2ToFloat";
    case UNPACK_R4_COLOR3:
        return "r4Byte3ToFloat";
    }
    throw CompilationError(
        CompilationStep::CODE_GENERATION, "Unsupported unpack-mode", std::to_string(static_cast<unsigned>(value)));
}

Optional<Value> Unpack::operator()(const Value& val) const
{
    // TODO are the r4 unpack values additional or instead-of the "normal" ones?
    if(!hasEffect())
        return val;
    // we never can pack complex types (even pointer, there are always 32-bit)
    if(!val.type.isSimpleType())
        return NO_VALUE;
    if(val.hasContainer())
    {
        // unpack vectors per element
        Value result(ContainerValue(val.container().elements.size()), val.type);
        for(const Value& elem : val.container().elements)
        {
            auto tmp = operator()(elem);
            if(!tmp)
                return NO_VALUE;
            result.container().elements.push_back(tmp.value());
        }
        return result;
    }
    // can only unpack literals
    if(!val.getLiteralValue())
        return NO_VALUE;
    switch(*this)
    {
    case UNPACK_NOP:
        return val;
    case UNPACK_16A_32:
    {
        if(val.type.isFloatingType())
            return NO_VALUE;
        // signed conversion -> truncate to unsigned short, bit-cast to signed short and sign-extend
        uint16_t lowWord = static_cast<uint16_t>(val.getLiteralValue()->unsignedInt());
        int16_t lowWordSigned = bit_cast<uint16_t, int16_t>(lowWord);
        return Value(Literal(static_cast<int32_t>(lowWordSigned)), val.type);
    }
    case UNPACK_16B_32:
    {
        if(val.type.isFloatingType())
            return NO_VALUE;
        // signed conversion -> truncate to unsigned short, bit-cast to signed short and sign-extend
        uint16_t highWord = static_cast<uint16_t>(val.getLiteralValue()->unsignedInt() >> 16);
        int16_t highWordSigned = bit_cast<uint16_t, int16_t>(highWord);
        return Value(Literal(static_cast<int32_t>(highWordSigned)), val.type);
    }
    case UNPACK_R4_ALPHA_REPLICATE:
        FALL_THROUGH
    case UNPACK_8888_32:
    {
        // unsigned cast required to guarantee cutting off the value
        uint8_t lsb = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt());
        return Value(Literal((static_cast<uint32_t>(lsb) << 24) | (static_cast<uint32_t>(lsb) << 16) |
                         (static_cast<uint32_t>(lsb) << 8) | lsb),
            val.type);
    }
    case UNPACK_8A_32:
    {
        if(val.type.isFloatingType())
            return UNPACK_R4_COLOR0(val);
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte0 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt());
        return Value(Literal(static_cast<uint32_t>(byte0)), val.type);
    }
    case UNPACK_8B_32:
    {
        if(val.type.isFloatingType())
            return UNPACK_R4_COLOR1(val);
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte1 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 8);
        return Value(Literal(static_cast<uint32_t>(byte1)), val.type);
    }
    case UNPACK_8C_32:
    {
        if(val.type.isFloatingType())
            return UNPACK_R4_COLOR2(val);
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte2 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 16);
        return Value(Literal(static_cast<uint32_t>(byte2)), val.type);
    }
    case UNPACK_8D_32:
    {
        if(val.type.isFloatingType())
            return UNPACK_R4_COLOR3(val);
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte3 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 24);
        return Value(Literal(static_cast<uint32_t>(byte3)), val.type);
    }
    case UNPACK_R4_COLOR0:
    {
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte0 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt());
        return Value(Literal(static_cast<float>(byte0) / 255.0f), val.type);
    }
    case UNPACK_R4_COLOR1:
    {
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte1 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 8);
        return Value(Literal(static_cast<float>(byte1) / 255.0f), val.type);
    }
    case UNPACK_R4_COLOR2:
    {
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte2 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 16);
        return Value(Literal(static_cast<float>(byte2) / 255.0f), val.type);
    }
    case UNPACK_R4_COLOR3:
    {
        // unsigned cast required to guarantee cutting off the value
        uint8_t byte3 = static_cast<uint8_t>(val.getLiteralValue()->unsignedInt() >> 24);
        return Value(Literal(static_cast<float>(byte3) / 255.0f), val.type);
    }
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Unsupported unpack-mode", std::to_string(static_cast<unsigned>(value)));
}

const Unpack Unpack::unpackTo32Bit(const DataType& type)
{
    if(type.getScalarBitCount() >= DataType::WORD)
        return UNPACK_NOP;
    if(type.getScalarBitCount() == DataType::HALF_WORD)
        return UNPACK_16A_32;
    if(type.getScalarBitCount() == DataType::BYTE)
        return UNPACK_8A_32;
    throw CompilationError(CompilationStep::GENERAL, "Unhandled type-width for unpack-modes", type.to_string());
}

bool Unpack::isPMBitSet() const
{
    // check whether pm bit set
    return value & 0x1;
}

bool Unpack::hasEffect() const
{
    // exclude "normal" NOP and NOP with pm bit set
    return value != 0 && value != 1;
}

std::string Pack::to_string() const
{
    // http://maazl.de/project/vc4asm/doc/extensions.html#pack
    switch(*this)
    {
    case PACK_NOP:
    case PACK_NOP_PM:
        return "";
    case PACK_32_16A:
        return "trunc32toLow16";
    case PACK_32_16A_S:
        return "sat16ToLow16";
    case PACK_32_16B:
        return "trunc32ToHigh16";
    case PACK_32_16B_S:
        return "sat16ToHigh16";
    case PACK_32_32:
        return "sat";
    case PACK_32_8888:
        return "replLSB";
    case PACK_32_8888_S:
        return "replLSBSat";
    case PACK_32_8A:
        return "truncLSBToByte0";
    case PACK_32_8A_S:
        return "satLSBToByte0";
    case PACK_32_8B:
        return "truncLSBToByte1";
    case PACK_32_8B_S:
        return "satLSBToByte1";
    case PACK_32_8C:
        return "truncLSBToByte2";
    case PACK_32_8C_S:
        return "satLSBToByte2";
    case PACK_32_8D:
        return "truncLSBToByte3";
    case PACK_32_8D_S:
        return "satLSBToByte3";
    case PACK_MUL_GRAY_REPLICATE:
        return "mulFloatToReplLSB";
    case PACK_MUL_COLOR0:
        return "mulFloatToByte0";
    case PACK_MUL_COLOR1:
        return "mulFloatToByte1";
    case PACK_MUL_COLOR2:
        return "mulFloatToByte2";
    case PACK_MUL_COLOR3:
        return "mulFloatToByte3";
    }
    throw CompilationError(
        CompilationStep::CODE_GENERATION, "Unsupported pack-mode", std::to_string(static_cast<unsigned>(value)));
}

Optional<Value> Pack::operator()(const Value& val) const
{
    // TODO are the mul pack modes additional or instead-of the "normal" ones? Can mul ALU also use"normal" pack mode?
    if(!hasEffect())
        return val;
    // we never can pack complex types (even pointer, there are always 32-bit)
    if(!val.type.isSimpleType())
        return NO_VALUE;
    if(val.hasContainer())
    {
        // pack vectors per element
        Value result(ContainerValue(val.container().elements.size()), val.type);
        for(const Value& elem : val.container().elements)
        {
            auto tmp = operator()(elem);
            if(!tmp)
                return NO_VALUE;
            result.container().elements.push_back(tmp.value());
        }
        return result;
    }
    // can only pack literals
    if(!val.getLiteralValue())
        return NO_VALUE;
    switch(*this)
    {
    case PACK_NOP:
        return val;
    case PACK_32_16A:
        if(val.type.isFloatingType())
            return NO_VALUE;
        return Value(Literal(val.getLiteralValue()->unsignedInt() & 0xFFFF), val.type);
    case PACK_32_16A_S:
        if(val.type.isFloatingType())
            return NO_VALUE;
        return Value(Literal(saturate<int16_t>(val.getLiteralValue()->signedInt()) & 0xFFFF), val.type);
    case PACK_32_16B:
        if(val.type.isFloatingType())
            return NO_VALUE;
        return Value(Literal((val.getLiteralValue()->unsignedInt() & 0xFFFF) << 16), val.type);
    case PACK_32_16B_S:
        if(val.type.isFloatingType())
            return NO_VALUE;
        return Value(Literal(saturate<int16_t>(val.getLiteralValue()->signedInt()) << 16), val.type);
    case PACK_32_32:
        // this depends on signed integer overflow (to determine overflow and then saturate)
        throw CompilationError(CompilationStep::GENERAL, "32-bit saturation is not implemented", val.to_string());
    case PACK_32_8888:
        return Value(
            Literal(((val.getLiteralValue()->unsignedInt() & 0xFF) << 24) |
                ((val.getLiteralValue()->unsignedInt() & 0xFF) << 16) |
                ((val.getLiteralValue()->unsignedInt() & 0xFF) << 8) | (val.getLiteralValue()->unsignedInt() & 0xFF)),
            val.type);
    case PACK_32_8888_S:
        return Value(Literal((saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 24) |
                         (saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 16) |
                         (saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 8) |
                         saturate<uint8_t>(val.getLiteralValue()->unsignedInt())),
            val.type);
    case PACK_32_8A:
        return Value(Literal(val.getLiteralValue()->unsignedInt() & 0xFF), val.type);
    case PACK_32_8A_S:
        return Value(Literal(saturate<uint8_t>(val.getLiteralValue()->unsignedInt())), val.type);
    case PACK_32_8B:
        return Value(Literal((val.getLiteralValue()->unsignedInt() & 0xFF) << 8), val.type);
    case PACK_32_8B_S:
        return Value(Literal(saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 8), val.type);
    case PACK_32_8C:
        return Value(Literal((val.getLiteralValue()->unsignedInt() & 0xFF) << 16), val.type);
    case PACK_32_8C_S:
        return Value(Literal(saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 16), val.type);
    case PACK_32_8D:
        return Value(Literal((val.getLiteralValue()->unsignedInt() & 0xFF) << 24), val.type);
    case PACK_32_8D_S:
        return Value(Literal(saturate<uint8_t>(val.getLiteralValue()->unsignedInt()) << 24), val.type);
    case PACK_MUL_GRAY_REPLICATE:
    {
        auto tmp = static_cast<uint32_t>(val.getLiteralValue()->real() / 255.0f) & 0xFF;
        return Value(Literal(tmp << 24 | tmp << 16 | tmp << 8 | tmp), val.type);
    }
    case PACK_MUL_COLOR0:
    {
        auto tmp = static_cast<uint32_t>(val.getLiteralValue()->real() * 255.0f) & 0xFF;
        return Value(Literal(tmp), val.type);
    }
    case PACK_MUL_COLOR1:
    {
        auto tmp = static_cast<uint32_t>(val.getLiteralValue()->real() * 255.0f) & 0xFF;
        return Value(Literal(tmp << 8), val.type);
    }
    case PACK_MUL_COLOR2:
    {
        auto tmp = static_cast<uint32_t>(val.getLiteralValue()->real() * 255.0f) & 0xFF;
        return Value(Literal(tmp << 16), val.type);
    }
    case PACK_MUL_COLOR3:
    {
        auto tmp = static_cast<uint32_t>(val.getLiteralValue()->real() * 255.0f) & 0xFF;
        return Value(Literal(tmp << 24), val.type);
    }
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Unsupported pack-mode", std::to_string(static_cast<unsigned>(value)));
}

bool Pack::isPMBitSet() const
{
    // check for pm bit set
    return value & 0x10;
}

bool Pack::hasEffect() const
{
    // exclude "normal" NOP and NOP with pm bit set
    return value != 0 && value != 0x10;
}

std::string vc4c::toString(const SetFlag flag)
{
    switch(flag)
    {
    case SetFlag::DONT_SET:
        return "";
    case SetFlag::SET_FLAGS:
        return "setf";
    }
    throw CompilationError(
        CompilationStep::CODE_GENERATION, "Unsupported set-flags flag", std::to_string(static_cast<unsigned>(flag)));
}

bool vc4c::isFlagSetByMulALU(unsigned char opAdd, unsigned char opMul)
{
    // despite what the Broadcom specification states, only using mul ALU if add ALU executes nop.
    return opAdd == OP_NOP.opAdd && opMul != OP_NOP.opMul;
}

bool OpCode::operator==(const OpCode& right) const
{
    if(opAdd > 0 && opAdd == right.opAdd)
        return true;
    if(opMul > 0 && opMul == right.opMul)
        return true;
    if(opAdd == 0 && opMul == 0 && right.opAdd == 0 && right.opMul == 0)
        return true;
    return false;
}

bool OpCode::operator!=(const OpCode& right) const
{
    return !(*this == right);
}

bool OpCode::operator<(const OpCode& right) const
{
    return opAdd < right.opAdd || opMul < right.opMul;
}

// Taken from https://stackoverflow.com/questions/2835469/how-to-perform-rotate-shift-in-c?noredirect=1&lq=1
static unsigned int rotate_right(unsigned int value, int shift)
{
    if((shift &= 31) == 0)
        return value;
    return (value >> shift) | (value << (32 - shift));
}

Optional<Value> OpCode::operator()(const Optional<Value>& firstOperand, const Optional<Value>& secondOperand) const
{
    if(!firstOperand)
        return NO_VALUE;
    if(numOperands > 1 && !secondOperand)
        return NO_VALUE;

    if(numOperands == 1 && firstOperand->isUndefined())
        // returns an undefined value (of the correct type)
        return (acceptsFloat == returnsFloat) ? Value(firstOperand->type) : UNDEFINED_VALUE;
    if(numOperands == 2 && secondOperand->isUndefined())
        // returns an undefined value (of the correct type)
        return (acceptsFloat == returnsFloat && firstOperand->type == secondOperand->type) ? Value(firstOperand->type) :
                                                                                             UNDEFINED_VALUE;

    // extract the literal value behind the operands
    Optional<Value> firstVal =
        (firstOperand->getLiteralValue() || firstOperand->hasContainer()) ? firstOperand.value() : NO_VALUE;
    Optional<Value> secondVal = !secondOperand || (secondOperand->getLiteralValue() || secondOperand->hasContainer()) ?
        secondOperand :
        NO_VALUE;

    if(!firstVal)
        return NO_VALUE;
    if(numOperands > 1 && !secondVal)
        return NO_VALUE;

    if(firstVal->hasImmediate() && firstVal->immediate().isVectorRotation())
        return NO_VALUE;
    if(numOperands > 1 && secondVal->hasImmediate() && secondVal->immediate().isVectorRotation())
        return NO_VALUE;

    // both (used) values are literals (or literal containers)

    bool calcPerComponent =
        (firstVal->hasContainer() && firstVal->container().elements.size() > 1 && !firstVal->container().isAllSame()) ||
        (numOperands > 1 && secondVal->hasContainer() && secondVal->container().elements.size() > 1 &&
            !secondVal->container().isAllSame());
    DataType resultType = firstVal->type;
    if(numOperands > 1 &&
        (secondVal->type.getVectorWidth() > resultType.getVectorWidth() ||
            secondVal->type.containsType(firstVal->type)))
        resultType = secondVal->type;

    // at least one used value is a container, need to calculate component-wise
    if(calcPerComponent)
    {
        auto numElements = std::max(firstVal->hasContainer() ? firstVal->container().elements.size() : 1,
            secondVal ? (secondVal->hasContainer() ? secondVal->container().elements.size() : 1) : 0);
        Value res(ContainerValue(numElements), resultType);
        for(unsigned char i = 0; i < numElements; ++i)
        {
            Optional<Value> tmp = NO_VALUE;
            if(numOperands == 1)
                tmp = operator()(
                    firstVal->hasContainer() ? firstVal->container().elements.at(i) : firstVal.value(), NO_VALUE);
            else
                tmp = operator()(firstVal->hasContainer() ? firstVal->container().elements.at(i) : firstVal.value(),
                    secondVal->hasContainer() ? secondVal->container().elements.at(i) : secondVal.value());
            if(!tmp)
                // result could not be calculated for a single component of the vector, abort
                return NO_VALUE;
            res.container().elements.push_back(tmp.value());
        }
        return res;
    }

    if(firstVal->isUndefined() || (numOperands > 1 && secondVal && secondVal->isUndefined()))
        return UNDEFINED_VALUE;

    // TODO throws if first element is no literal
    const Literal firstLit = firstVal->getLiteralValue() ?
        firstVal->getLiteralValue().value() :
        firstVal->container().elements.at(0).getLiteralValue().value();
    const Literal secondLit = (!secondVal || numOperands == 1) ?
        INT_ZERO.literal() :
        secondVal->getLiteralValue() ? secondVal->getLiteralValue().value() :
                                       secondVal->container().elements.at(0).getLiteralValue().value();

    if(*this == OP_ADD)
        return Value(Literal(firstLit.signedInt() + secondLit.signedInt()), resultType);
    if(*this == OP_AND)
        return Value(Literal(firstLit.unsignedInt() & secondLit.unsignedInt()), resultType);
    if(*this == OP_ASR)
        return Value(intermediate::asr(resultType, firstLit, secondLit), resultType);
    if(*this == OP_CLZ)
        return Value(intermediate::clz(resultType, firstLit), resultType);
    if(*this == OP_FADD)
        return Value(Literal(firstLit.real() + secondLit.real()), resultType);
    if(*this == OP_FMAX)
        return Value(Literal(std::max(firstLit.real(), secondLit.real())), resultType);
    if(*this == OP_FMAXABS)
        return Value(Literal(std::max(std::fabs(firstLit.real()), std::fabs(secondLit.real()))), resultType);
    if(*this == OP_FMIN)
        return Value(Literal(std::min(firstLit.real(), secondLit.real())), resultType);
    if(*this == OP_FMINABS)
        return Value(Literal(std::min(std::fabs(firstLit.real()), std::fabs(secondLit.real()))), resultType);
    if(*this == OP_FMUL)
        return Value(Literal(firstLit.real() * secondLit.real()), resultType);
    if(*this == OP_FSUB)
        return Value(Literal(firstLit.real() - secondLit.real()), resultType);
    if(*this == OP_FTOI)
        return Value(
            Literal(static_cast<int32_t>(firstLit.real())), TYPE_INT32.toVectorType(firstVal->type.getVectorWidth()));
    if(*this == OP_ITOF)
        return Value(Literal(static_cast<float>(firstLit.signedInt())),
            TYPE_FLOAT.toVectorType(firstVal->type.getVectorWidth()));
    if(*this == OP_MAX)
        return Value(Literal(std::max(firstLit.signedInt(), secondLit.signedInt())), resultType);
    if(*this == OP_MIN)
        return Value(Literal(std::min(firstLit.signedInt(), secondLit.signedInt())), resultType);
    if(*this == OP_MUL24)
    {
        // this check is disabled on purpose
        // if(((firstLit.unsignedInt() & 0xFF000000) != 0) || ((secondLit.unsignedInt() & 0xFF000000) != 0))
        //    throw CompilationError(CompilationStep::GENERAL, "Mul24 with high byte set will discard the bits");
        return Value(Literal((firstLit.unsignedInt() & 0xFFFFFF) * (secondLit.unsignedInt() & 0xFFFFFF)), resultType);
    }
    if(*this == OP_NOT)
        return Value(Literal(~firstLit.unsignedInt()), resultType);
    if(*this == OP_OR)
        return Value(Literal(firstLit.unsignedInt() | secondLit.unsignedInt()), resultType);
    if(*this == OP_ROR)
        return Value(Literal(rotate_right(firstLit.unsignedInt(), secondLit.signedInt())), resultType);
    if(*this == OP_SHL)
        return Value(Literal(firstLit.unsignedInt() << secondLit.signedInt()), resultType);
    if(*this == OP_SHR)
        return Value(Literal(firstLit.unsignedInt() >> secondLit.signedInt()), resultType);
    if(*this == OP_SUB)
        return Value(Literal(firstLit.signedInt() - secondLit.signedInt()), resultType);
    if(*this == OP_XOR)
        return Value(Literal(firstLit.unsignedInt() ^ secondLit.unsignedInt()), resultType);
    if(*this == OP_V8ADDS || *this == OP_V8SUBS || *this == OP_V8MAX || *this == OP_V8MIN || *this == OP_V8MULD)
    {
        std::array<uint32_t, 4> bytesA, bytesB, bytesOut;
        bytesA[0] = firstLit.unsignedInt() & 0xFF;
        bytesA[1] = firstLit.unsignedInt() >> 8 & 0xFF;
        bytesA[2] = firstLit.unsignedInt() >> 16 & 0xFF;
        bytesA[3] = firstLit.unsignedInt() >> 24 & 0xFF;
        bytesB[0] = secondLit.unsignedInt() & 0xFF;
        bytesB[1] = secondLit.unsignedInt() >> 8 & 0xFF;
        bytesB[2] = secondLit.unsignedInt() >> 16 & 0xFF;
        bytesB[3] = secondLit.unsignedInt() >> 24 & 0xFF;
        std::transform(
            bytesA.begin(), bytesA.end(), bytesB.begin(), bytesOut.begin(), [this](uint32_t a, uint32_t b) -> uint32_t {
                if(*this == OP_V8ADDS)
                    return std::min(a + b, 255u);
                if(*this == OP_V8SUBS)
                    return std::max(std::min(a - b, 255u), 0u);
                if(*this == OP_V8MAX)
                    return std::max(a, b);
                if(*this == OP_V8MIN)
                    return std::min(a, b);
                if(*this == OP_V8MULD)
                    return (a * b + 127) / 255;
                throw CompilationError(CompilationStep::GENERAL, "Unhandled op-code", this->name);
            });
        uint32_t result = ((bytesOut[3] & 0xFF) << 24) | ((bytesOut[2] & 0xFF) << 16) | ((bytesOut[1] & 0xFF) << 8) |
            (bytesOut[0] & 0xFF);
        return Value(Literal(result), resultType);
    }

    return NO_VALUE;
}

const OpCode& OpCode::toOpCode(const std::string& name)
{
    const OpCode& code = findOpCode(name);
    if(code == OP_NOP && name.compare("nop") != 0)
        throw CompilationError(CompilationStep::GENERAL, "No machine code operation for this op-code", name);
    return code;
}

static const std::multimap<std::string, OpCode> opCodes = {{OP_ADD.name, OP_ADD}, {OP_AND.name, OP_AND},
    {OP_ASR.name, OP_ASR}, {OP_CLZ.name, OP_CLZ}, {OP_FADD.name, OP_FADD}, {OP_FMAX.name, OP_FMAX},
    {OP_FMAXABS.name, OP_FMAXABS}, {OP_FMIN.name, OP_FMIN}, {OP_FMINABS.name, OP_FMINABS}, {OP_FMUL.name, OP_FMUL},
    {OP_FSUB.name, OP_FSUB}, {OP_FTOI.name, OP_FTOI}, {OP_ITOF.name, OP_ITOF}, {OP_MAX.name, OP_MAX},
    {OP_MIN.name, OP_MIN}, {OP_MUL24.name, OP_MUL24}, {OP_NOP.name, OP_NOP}, {OP_NOT.name, OP_NOT}, {OP_OR.name, OP_OR},
    {OP_ROR.name, OP_ROR}, {OP_SHL.name, OP_SHL}, {OP_SHR.name, OP_SHR}, {OP_SUB.name, OP_SUB},
    {OP_V8ADDS.name, OP_V8ADDS}, {OP_V8MAX.name, OP_V8MAX}, {OP_V8MIN.name, OP_V8MIN}, {OP_V8MULD.name, OP_V8MULD},
    {OP_V8SUBS.name, OP_V8SUBS}, {OP_XOR.name, OP_XOR}};

// NOTE: The indices MUST correspond to the op-codes!
static const std::array<OpCode, 32> addCodes = {OP_NOP, OP_FADD, OP_FSUB, OP_FMIN, OP_MAX, OP_FMINABS, OP_FMAXABS,
    OP_FTOI, OP_ITOF, OP_NOP, OP_NOP, OP_NOP, OP_ADD, OP_SUB, OP_SHR, OP_ASR, OP_ROR, OP_SHL, OP_MIN, OP_MAX, OP_AND,
    OP_OR, OP_XOR, OP_NOT, OP_CLZ, OP_NOP, OP_NOP, OP_NOP, OP_NOP, OP_NOP, OP_V8ADDS, OP_V8SUBS};

// NOTE: The indices MUST correspond to the op-codes!
static const std::array<OpCode, 8> mulCodes = {
    OP_NOP, OP_FMUL, OP_MUL24, OP_V8MULD, OP_V8MIN, OP_V8MAX, OP_V8ADDS, OP_V8SUBS};

bool OpCode::isIdempotent() const
{
    if(*this == OP_AND || *this == OP_FMAX || *this == OP_FMIN || *this == OP_MAX || *this == OP_MIN ||
        *this == OP_OR || *this == OP_V8MAX || *this == OP_V8MIN)
    {
        return true;
    }
    return false;
}

bool OpCode::isAssociative() const
{
    return *this == OP_ADD || *this == OP_AND || *this == OP_FADD || *this == OP_FMAX || *this == OP_FMAXABS ||
        *this == OP_FMIN || *this == OP_FMINABS || *this == OP_FMUL || *this == OP_MAX || *this == OP_MIN ||
        *this == OP_MUL24 || *this == OP_OR || *this == OP_V8MAX || *this == OP_V8MIN || *this == OP_XOR;
}

bool OpCode::isCommutative() const
{
    return *this == OP_ADD || *this == OP_AND || *this == OP_FADD || *this == OP_FMAX || *this == OP_FMAXABS ||
        *this == OP_FMIN || *this == OP_FMINABS || *this == OP_FMUL || *this == OP_MAX || *this == OP_MIN ||
        *this == OP_MUL24 || *this == OP_OR || *this == OP_V8ADDS || *this == OP_V8MAX || *this == OP_V8MIN ||
        *this == OP_V8MULD || *this == OP_XOR;
}

const OpCode& OpCode::toOpCode(const unsigned char opCode, const bool isMulALU)
{
    if(opCode == 0)
        return OP_NOP;
    if(isMulALU)
    {
        return mulCodes.at(opCode);
    }
    else
    {
        return addCodes.at(opCode);
    }
    throw CompilationError(CompilationStep::GENERAL, "No machine code operation for this op-code",
        std::to_string(static_cast<unsigned>(opCode)));
}

const OpCode& OpCode::findOpCode(const std::string& name)
{
    auto it = opCodes.find(name);
    if(it != opCodes.end())
        return it->second;
    return OP_NOP;
}

Optional<Value> OpCode::getLeftIdentity(const OpCode& code)
{
    if(code == OP_ADD)
        return INT_ZERO;
    if(code == OP_AND)
        return VALUE_ALL_BITS_SET;
    if(code == OP_FADD)
        return FLOAT_ZERO;
    if(code == OP_FMUL)
        return FLOAT_ONE;
    if(code == OP_MUL24)
        return INT_ONE;
    if(code == OP_OR)
        return INT_ZERO;
    if(code == OP_XOR)
        return INT_ZERO;
    return NO_VALUE;
}

Optional<Value> OpCode::getRightIdentity(const OpCode& code)
{
    if(code == OP_ADD)
        return INT_ZERO;
    if(code == OP_AND)
        return VALUE_ALL_BITS_SET;
    if(code == OP_ASR)
        return INT_ZERO;
    if(code == OP_FADD)
        return FLOAT_ZERO;
    if(code == OP_FMUL)
        return FLOAT_ONE;
    if(code == OP_FSUB)
        return FLOAT_ZERO;
    if(code == OP_MUL24)
        return INT_ONE;
    if(code == OP_OR)
        return INT_ZERO;
    if(code == OP_ROR)
        return INT_ZERO;
    if(code == OP_SHL)
        return INT_ZERO;
    if(code == OP_SHR)
        return INT_ZERO;
    if(code == OP_SUB)
        return INT_ZERO;
    if(code == OP_XOR)
        return INT_ZERO;
    return NO_VALUE;
}

Optional<Value> OpCode::getLeftAbsorbingElement(const OpCode code)
{
    if(code == OP_AND)
        return INT_ZERO;
    if(code == OP_ASR)
        // XXX actually all bits set too
        return INT_ZERO;
    if(code == OP_FMAX)
        return Value(Literal(std::numeric_limits<float>::infinity()), TYPE_FLOAT);
    if(code == OP_FMAXABS)
        return Value(Literal(std::numeric_limits<float>::infinity()), TYPE_FLOAT);
    if(code == OP_FMINABS)
        return FLOAT_ZERO;
    if(code == OP_FMUL)
        return FLOAT_ZERO;
    if(code == OP_MUL24)
        return INT_ZERO;
    if(code == OP_OR)
        return VALUE_ALL_BITS_SET;
    if(code == OP_ROR)
        // XXX actually all bits set too
        return INT_ZERO;
    if(code == OP_SHL)
        return INT_ZERO;
    if(code == OP_SHR)
        return INT_ZERO;
    if(code == OP_V8MIN)
        return INT_ZERO;
    if(code == OP_V8MAX)
        return VALUE_ALL_BITS_SET;
    if(code == OP_V8MULD)
        return INT_ZERO;
    return NO_VALUE;
}

Optional<Value> OpCode::getRightAbsorbingElement(const OpCode code)
{
    if(code == OP_AND)
        return INT_ZERO;
    if(code == OP_FMAX)
        return Value(Literal(std::numeric_limits<float>::infinity()), TYPE_FLOAT);
    if(code == OP_FMAXABS)
        return Value(Literal(std::numeric_limits<float>::infinity()), TYPE_FLOAT);
    if(code == OP_FMINABS)
        return FLOAT_ZERO;
    if(code == OP_FMUL)
        return FLOAT_ZERO;
    if(code == OP_MUL24)
        return INT_ZERO;
    if(code == OP_OR)
        return VALUE_ALL_BITS_SET;
    if(code == OP_V8MIN)
        return INT_ZERO;
    if(code == OP_V8MAX)
        return VALUE_ALL_BITS_SET;
    if(code == OP_V8MULD)
        return INT_ZERO;
    return NO_VALUE;
}

std::string vc4c::toString(const BranchCond cond)
{
    switch(cond)
    {
    case BranchCond::ALL_C_CLEAR:
        return "ifallcc";
    case BranchCond::ALL_C_SET:
        return "ifallc";
    case BranchCond::ALL_N_CLEAR:
        return "ifallnc";
    case BranchCond::ALL_N_SET:
        return "ifalln";
    case BranchCond::ALL_Z_CLEAR:
        return "ifallzc";
    case BranchCond::ALL_Z_SET:
        return "ifallz";
    case BranchCond::ALWAYS:
        return "";
    case BranchCond::ANY_C_CLEAR:
        return "ifanycc";
    case BranchCond::ANY_C_SET:
        return "ifanyc";
    case BranchCond::ANY_N_CLEAR:
        return "ifanync";
    case BranchCond::ANY_N_SET:
        return "ifanyn";
    case BranchCond::ANY_Z_CLEAR:
        return "ifanyzc";
    case BranchCond::ANY_Z_SET:
        return "ifanyz";
    }
    throw CompilationError(
        CompilationStep::GENERAL, "Invalid branch-condition", std::to_string(static_cast<int>(cond)));
}
