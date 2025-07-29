package main

import (
	"runtime"
	"unsafe"

	"github.com/tliron/glsp"
	protocol "github.com/tliron/glsp/protocol_3_16"
)

var docs map[protocol.DocumentUri]*AstNode = make(map[protocol.DocumentUri]*AstNode, 0)

func processDocument(context *glsp.Context, uri protocol.DocumentUri, data string) {
	runtime.LockOSThread()
	globalContextInitialize()

	source := sourceMake(uri, data)
	defer sourceFree(source)

	tokenizer := tokenizerInit(source)

	allocator := memoryAllocatorMake()
	defer memoryAllocatorFree(allocator)
	memoryAllocatorSetActive(allocator)

	docs[uri] = (*AstNode)(parseRoot(&tokenizer))

	calculateRange := func(item *SourceLocation) protocol.Range {
		data := unsafe.Slice(item.source.data_buffer, item.source.data_buffer_size)
		r := protocol.Range{}
		line := uint64(0)
		column := uint64(0)
		for i, ch := range data {
			if uint64(item.offset) == uint64(i) {
				r.Start = protocol.Position{Line: protocol.UInteger(line), Character: protocol.UInteger(column)}
			}

			if uint64(item.offset+item.length) == uint64(i) {
				r.End = protocol.Position{Line: protocol.UInteger(line), Character: protocol.UInteger(column)}
				break
			}

			if ch == '\n' {
				line++
				column = 0
				continue
			}

			column++
		}
		return r
	}

	diagnostics := make([]protocol.Diagnostic, 0)
	for _, diag := range getDiagnostics() {
		severity := protocol.DiagnosticSeverityError
		diagnostics = append(diagnostics, protocol.Diagnostic{
			Range:    calculateRange((*SourceLocation)(&diag.location)),
			Severity: &severity,
			Code:     nil,
			Message:  diagnosticToString(&diag.diagnostic),
		})
	}
	context.Notify(protocol.ServerTextDocumentPublishDiagnostics, protocol.PublishDiagnosticsParams{
		URI:         uri,
		Diagnostics: diagnostics,
	})

	globalContextFinish()
	runtime.UnlockOSThread()
}
