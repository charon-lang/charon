package main

// #include <stdlib.h>
// #include <lib/source.h>
// #include <lib/memory.h>
// #include <lexer/tokenizer.h>
// #include <parser/parser.h>
import "C"

type MemoryAllocator C.memory_allocator_t
type Source C.source_t
type Tokenizer C.tokenizer_t
type AstNode C.ast_node_t

func memoryAllocatorMake() *MemoryAllocator {
	return (*MemoryAllocator)(C.memory_allocator_make())
}

func memoryAllocatorFree(allocator *MemoryAllocator) {
	C.memory_allocator_free((*C.memory_allocator_t)(allocator))
}

func memoryAllocatorSetActive(allocator *MemoryAllocator) {
	C.memory_active_allocator_set((*C.memory_allocator_t)(allocator))
}

func sourceMake(name string, data string) *Source {
	return (*Source)(C.source_make(C.CString(name), C.CString(data), C.size_t(len(data))))
}

func sourceFree(source *Source) {
	C.source_free((*C.source_t)(source))
}

func tokenizerMake(source *Source) *Tokenizer {
	return (*Tokenizer)(C.tokenizer_make((*C.source_t)(source)))
}

func tokenizerFree(tokenizer *Tokenizer) {
	C.tokenizer_free((*C.tokenizer_t)(tokenizer))
}

func parseRoot(tokenizer *Tokenizer) *AstNode {
	return (*AstNode)(C.parser_root((*C.tokenizer_t)(tokenizer)))
}
