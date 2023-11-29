#pragma once
#include "Disassembler.h"
#include "il.h"

// forward declaration
class DartApp;
class DartFunction;

struct AsmText {
	enum DataType : uint8_t {
		None,
		ThreadOffset,
		PoolOffset,
		Boolean,
		Call,
	};

	uint64_t addr;
	char text[71]; // first 16 bytes are for mnemonic and spaces, after that is operands
	uint8_t dataType;
	union {
		uint64_t threadOffset;
		uint64_t poolOffset;
		bool boolVal;
		uint64_t callAddress;
	};
};

class AsmTexts {
public:
	AsmTexts(std::vector<AsmText> asm_texts) : first_addr(asm_texts.front().addr), last_addr(asm_texts.back().addr), asm_texts(std::move(asm_texts)) {}

	std::vector<AsmText>& Data() { return asm_texts; }

	size_t AtIndex(uint64_t addr) {
		ASSERT(addr >= first_addr && addr <= last_addr);
		// TODO: below is specific to arm64
		// estimate index (normally 4 bytes per instruction for arm64)
		auto idx = (addr - first_addr) / 4;
		while (asm_texts[idx].addr < addr)
			++idx;
		return idx;
	}

	AsmText& AtAddr(uint64_t addr) {
		return asm_texts[AtIndex(addr)];
	}

private:
	uint64_t first_addr;
	uint64_t last_addr;
	std::vector<AsmText> asm_texts;
};

struct FnParamInfo {
	A64::Register valReg;
	int32_t localOffset{ 0 }; // offset from FP (local variable)
	DartType* type{ nullptr };
	std::string name;
	std::shared_ptr<VarValue> val;
	explicit FnParamInfo(A64::Register valReg) : valReg(valReg) {}
	explicit FnParamInfo(A64::Register valReg, int32_t localOffset) : valReg(valReg), localOffset(localOffset) {}
	explicit FnParamInfo(A64::Register valReg, std::shared_ptr<VarValue> val) : valReg(valReg), val(val) {}
	explicit FnParamInfo(A64::Register valReg, int32_t localOffset, DartType* type, std::string name, std::shared_ptr<VarValue> val)
		: valReg(valReg), localOffset(localOffset), type(type), name(std::move(name)), val(val) {}
	explicit FnParamInfo(A64::Register valReg, std::string name) : valReg(valReg), name(std::move(name)) {}
	explicit FnParamInfo(std::string name) : name(std::move(name)) {}

	std::string ToString() const;
};

struct FnParams {
	uint8_t numFixedParam{ 0 };
	bool isNamedParam{ false };
	std::vector<FnParamInfo> params;

	FnParamInfo* findValReg(A64::Register reg);
	bool movValReg(A64::Register dstReg, A64::Register srcReg);
	std::string ToString() const;
};

class AnalyzedFnData {
public:
	AnalyzedFnData(DartApp& app, DartFunction& dartFn, AsmTexts asmTexts);
	void AddInstruction(std::unique_ptr<ILInstr> insn) {
		il_insns.push_back(std::move(insn));
	}

	ILInstr* LastIL() {
		return il_insns.back().get();
	}

	void RemoveLastIL() {
		il_insns.pop_back();
	}

	DartApp& app;
	DartFunction& dartFn;
	AsmTexts asmTexts;
	cs_insn* last_ret;
	uint32_t stackSize; // for local variables (this includes space for call arguments)
	bool useFramePointer;
	FnParams params;
	std::vector<std::unique_ptr<ILInstr>> il_insns;

	friend class CodeAnalyzer;
};

class CodeAnalyzer
{
public:
	CodeAnalyzer(DartApp& app) : app(app) {};

	void AnalyzeAll();

private:
	static AsmTexts convertAsm(AsmInstructions& asm_insns);
	
	// implementation is specific to architecture
	void asm2il(DartFunction* dartFn, AsmInstructions& asm_insns);

	DartApp& app;
};
