/*
 * This is a simple example which generates two functions. One calls the other.
 * Then an object file is created.
 */

#include "llvm/ADT/APInt.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_os_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/DIBuilder.h"
#include <iostream>

using namespace llvm;

struct DebugStuff {
    DICompileUnit *TheCU;
    DIBuilder *DBuilder;
};

static DISubroutineType *CreateFunctionType(unsigned NumArgs, DIFile *Unit, DebugStuff &Dbg) {
  SmallVector<Metadata *, 8> EltTys;
  static DIType *U32Ty;
  if (!U32Ty) {
      U32Ty = Dbg.DBuilder->createBasicType("u32", 32, dwarf::DW_ATE_unsigned);
  }

  // Add the result type.
  EltTys.push_back(U32Ty);

  for (unsigned i = 0, e = NumArgs; i != e; ++i) {
    EltTys.push_back(U32Ty);
  }

  return Dbg.DBuilder->createSubroutineType(Dbg.DBuilder->getOrCreateTypeArray(EltTys));
}

bool
codegenObject(Module *TheModule, std::string ObjName)
{
    // Init a load of stuff.
    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    // Identify the platform.
    auto TargetTriple = sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    // Find the definition of this platform.
    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
        errs() << Error;
        return false;
    }

    // Set up the target.
    auto CPU = "generic";
    auto Features = "";

    TargetOptions opt;
    auto RM = Optional<Reloc::Model>();
    auto TheTargetMachine =
        Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    TheModule->setDataLayout(TheTargetMachine->createDataLayout());

    std::error_code EC;
    auto FileName = ObjName;
    raw_fd_ostream dest(FileName, EC, sys::fs::F_None);

    if (EC) {
        errs() << FileName << ": " << EC.message();
        return false;
    }

    legacy::PassManager pass;
    auto FileType = TargetMachine::CGFT_ObjectFile;

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
        errs() << "can't emit a file of this type";
        return 1;
    }

    pass.run(*TheModule);
    dest.flush();

    return true;
}

Function *
genAdd3(LLVMContext &TheContext, Module *TheModule, DebugStuff &Dbg)
{
	// And a function inside the module.
	Type *u32 = Type::getInt32Ty(TheContext);
	std::vector<Type*> Ints(1, u32);
	FunctionType *FT = FunctionType::get(u32, Ints, false);
	Function *F = Function::Create(FT, Function::ExternalLinkage, "add_3", TheModule);

    // Add Debug information for this function.
    DIFile *Unit = Dbg.DBuilder->createFile(Dbg.TheCU->getFilename(),
                                            Dbg.TheCU->getDirectory());
    DIScope *FContext = Unit;
    unsigned LineNo = 0;
    unsigned ScopeLine = 0;
    DISubprogram *SP = Dbg.DBuilder->createFunction(FContext, "add_3", StringRef(), Unit, LineNo,
                                                    CreateFunctionType(F->arg_size(), Unit, Dbg),
                                                    false, true, ScopeLine, DINode::FlagPrototyped, false);
    F->setSubprogram(SP);

    // Give arguments names.
    auto Arg1 = F->args().begin();
    Arg1->setName("x");

    // We use an IRBuilder to build our LLVM IR program in-memory.
    IRBuilder<> Builder(TheContext);

	// The function has a block
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
	Builder.SetInsertPoint(BB);

    // The block just adds two constants.
	auto AddConst = ConstantInt::get(TheContext, APInt(32, 3));
	auto Res = Builder.CreateAdd(Arg1, AddConst, "res");

    // Let's insert a DILabel right before the return.
    // DILabel * 	createLabel (DIScope *Scope, StringRef Name, DIFile *File, unsigned LineNo, bool AlwaysPreserve=false)
    auto label = Dbg.DBuilder->createLabel(FContext, "my_label", Unit, 0, true);
    //
    // Instruction * 	insertLabel (DILabel *LabelInfo, const DILocation *DL, BasicBlock *InsertAtEnd)

    // Emit return value.
	Builder.CreateRet(Res);

    return F;
}

bool
genMain(LLVMContext &TheContext, Module *TheModule, Function *Add3)
{
	std::vector<Type*> NoArgs = {};
	Type *u32 = Type::getInt32Ty(TheContext);
	FunctionType *FT = FunctionType::get(u32, NoArgs, false);
	Function *F = Function::Create(FT, Function::ExternalLinkage, "main", TheModule);

    IRBuilder<> Builder(TheContext);
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
	Builder.SetInsertPoint(BB);

    // Emit the call to add_3.
	auto Arg = ConstantInt::get(TheContext, APInt(32, 1));
	std::vector<Value*> Args(1);
    Args[0] = Arg;
	auto Res = Builder.CreateCall(Add3, Args);

    // Return the result of the process.
	Builder.CreateRet(Res);

    return true;
}

int
main(void)
{
    // Start with a LLVM context.
    LLVMContext TheContext;

	// Make a module.
	Module *TheModule = new Module("mymod", TheContext);

    // Get debugging stuff ready.
    struct DebugStuff Dbg;
    Dbg.DBuilder = new DIBuilder(*TheModule);
    Dbg.TheCU = Dbg.DBuilder->createCompileUnit(dwarf::DW_LANG_C,
                                                Dbg.DBuilder->createFile("main.xxx", "."),
                                                "API Example", 0, "", 0);

    // Emit IR functions.
    auto Add3 = genAdd3(TheContext, TheModule, Dbg);
    genMain(TheContext, TheModule, Add3);

    // Finalise debug info.
    Dbg.DBuilder->finalize();

	// Print and verify IR.
	TheModule->print(errs(), nullptr);
    verifyModule(*TheModule, &errs());

    // Generate code!
    if (!codegenObject(TheModule, "out.o")) {
        return 1;
    }

    return 0;
}

