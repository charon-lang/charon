#include <stdlib.h>
#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

int main() {
    LLVMContextRef context = LLVMContextCreate();
    LLVMModuleRef module = LLVMModuleCreateWithNameInContext("EpicModule", context);
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(context);

    LLVMValueRef fn = LLVMAddFunction(module, "main", LLVMFunctionType(LLVMInt32TypeInContext(context), NULL, 0, false));

    // !--------------------------------!

    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(context, fn, "");
    LLVMPositionBuilderAtEnd(builder, entry);

    LLVMValueRef anon_struct = LLVMConstStructInContext(context, (LLVMValueRef[]) { LLVMConstInt(LLVMInt64TypeInContext(context), 420, false) }, 1, false);
    LLVMValueRef ptr = LLVMBuildAlloca(builder, LLVMTypeOf(anon_struct), "");
    LLVMBuildStore(builder, anon_struct, ptr);

    // !--------------------------------!

    LLVMPrintModuleToFile(module, "main.ll", NULL);

    LLVMDisposeBuilder(builder);
    LLVMDisposeModule(module);
    LLVMContextDispose(context);

    return 0;
}