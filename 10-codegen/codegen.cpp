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
#include <iostream>

using namespace llvm;

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
genAdd3(LLVMContext &TheContext, Module *TheModule)
{
	// And a function inside the module.
	Type *u32 = Type::getInt32Ty(TheContext);
	std::vector<Type*> Ints(1, u32);
	FunctionType *FT = FunctionType::get(u32, Ints, false);
	Function *F = Function::Create(FT, Function::ExternalLinkage, "add_3", TheModule);

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

    // Emit IR functions.
    auto Add3 = genAdd3(TheContext, TheModule);
    genMain(TheContext, TheModule, Add3);

	// Print and verify IR.
	TheModule->print(errs(), nullptr);
    verifyModule(*TheModule, &errs());

    // Generate code!
    if (!codegenObject(TheModule, "out.o")) {
        return 1;
    }

    return 0;
}

