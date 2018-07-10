/*
 * This is a simple example which generates a function which simply adds the constant
 * value 665 to its sole argument.
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
#include <iostream>

using namespace llvm;

int
main(void)
{
    // Start with a LLVM context.
    LLVMContext TheContext;

	// Make a module.
	Module *TheModule = new Module("mymod", TheContext);

	// And a function inside the module.
	Type *u32 = Type::getInt32Ty(TheContext);
	std::vector<Type*> Ints(1, u32);
	FunctionType *FT = FunctionType::get(u32, Ints, false);
	Function *F = Function::Create(FT, Function::ExternalLinkage, "add_665", TheModule);

    // Give arguments names.
    auto Arg1 = F->args().begin();
    Arg1->setName("x");

    // We use an IRBuilder to build our LLVM IR program in-memory.
    IRBuilder<> Builder(TheContext);

	// The function has a block
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", F);
	Builder.SetInsertPoint(BB);

    // The block just adds two constants.
	auto AddConst = ConstantInt::get(TheContext, APInt(32, 665));
	auto Res = Builder.CreateAdd(Arg1, AddConst, "res");

    // Emit return value.
	Builder.CreateRet(Res);

	// Print IR.
    // Notice that constant propagation occurs.
	TheModule->print(errs(), nullptr);

    // We can check the validity of our code like so.
    auto CErr = make_unique<raw_os_ostream>(std::cout);
    verifyModule(*TheModule, &(*CErr));
}
