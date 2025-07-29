package main

// #cgo CFLAGS:-std=gnu23 -I/persistent/projects/charon-lang/charon/lib/include -I/persistent/projects/charon-lang/charon/lib/src
// #cgo LDFLAGS:-L /nix/store/vvp8hlss3d5q6hn0cifq04jrpnp6bini-pcre2-10.44/lib -L /nix/store/136cbgnv6rrk8ynp1d8x9977plz14cf9-llvm-20.1.6-lib/lib$ -l pcre2-8 -l LLVM /persistent/projects/charon-lang/charon/build/lib/libcharon.a
// #include <stdlib.h>
// #include <charon/diag.h>
// #include <lib/source.h>
// #include <lib/memory.h>
// #include <lib/diag.h>
// #include <lib/context.h>
// #include <lexer/tokenizer.h>
// #include <parser/parser.h>
//
// vector_diag_item_t *get_diag_items() {
// 	return &g_global_context->diag_items;
// }
import "C"
import "unsafe"

type AstNode C.ast_node_t
type SourceLocation C.source_location_t

func globalContextInitialize() {
	C.context_global_initialize()
}

func globalContextFinish() {
	C.context_global_finish()
}

func memoryAllocatorMake() *C.memory_allocator_t {
	return C.memory_allocator_make()
}

func memoryAllocatorFree(allocator *C.memory_allocator_t) {
	C.memory_allocator_free(allocator)
}

func memoryAllocatorSetActive(allocator *C.memory_allocator_t) {
	C.memory_active_allocator_set(allocator)
}

func sourceMake(name string, data string) *C.source_t {
	return C.source_make(C.CString(name), C.CString(data), C.size_t(len(data)))
}

func sourceFree(source *C.source_t) {
	C.source_free(source)
}

func tokenizerInit(source *C.source_t) C.tokenizer_t {
	return C.tokenizer_init(source)
}

func parseRoot(tokenizer *C.tokenizer_t) *C.ast_node_t {
	return C.parser_root(tokenizer)
}

func getDiagnostics() []*C.diag_item_t {
	diag_items := C.get_diag_items()
	return unsafe.Slice(diag_items.values, diag_items.size)
}

func diagnosticToString(diag *C.charon_diag_t) string {
	str := C.charon_diag_tostring(diag)
	gostr := C.GoString(str)
	C.free(unsafe.Pointer(str))
	return gostr
}
