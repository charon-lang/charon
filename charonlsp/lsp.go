package main

import (
	"github.com/tliron/commonlog"
	"github.com/tliron/glsp"
	protocol "github.com/tliron/glsp/protocol_3_16"
	"github.com/tliron/glsp/server"

	_ "github.com/tliron/commonlog/simple"
)

const LANG_NAME = "Charon"

var (
	version string = "0.0.1"
	handler protocol.Handler
)

var log = commonlog.GetLogger("charon")

func main() {
	commonlog.Configure(2, nil)

	handler = protocol.Handler{
		Initialize:            initialize,
		Initialized:           initialized,
		Shutdown:              shutdown,
		SetTrace:              setTrace,
		TextDocumentDidOpen:   didOpen,
		TextDocumentDidClose:  didClose,
		TextDocumentDidChange: didChange,
		TextDocumentHover:     hover,
	}

	server := server.NewServer(&handler, LANG_NAME, false)

	server.RunStdio()
}

func initialize(context *glsp.Context, params *protocol.InitializeParams) (any, error) {
	capabilities := handler.CreateServerCapabilities()
	capabilities.HoverProvider = true
	capabilities.CodeActionProvider = true
	capabilities.TextDocumentSync = 1

	return protocol.InitializeResult{
		Capabilities: capabilities,
		ServerInfo: &protocol.InitializeResultServerInfo{
			Name:    LANG_NAME,
			Version: &version,
		},
	}, nil
}

func initialized(context *glsp.Context, params *protocol.InitializedParams) error {
	return nil
}

func shutdown(context *glsp.Context) error {
	protocol.SetTraceValue(protocol.TraceValueOff)
	return nil
}

func setTrace(context *glsp.Context, params *protocol.SetTraceParams) error {
	protocol.SetTraceValue(params.Value)
	return nil
}

func didOpen(context *glsp.Context, params *protocol.DidOpenTextDocumentParams) error {
	processDocument(context, params.TextDocument.URI, params.TextDocument.Text)
	return nil
}

func didChange(context *glsp.Context, params *protocol.DidChangeTextDocumentParams) error {
	processDocument(context, params.TextDocument.URI, params.ContentChanges[0].(protocol.TextDocumentContentChangeEventWhole).Text)
	return nil
}

func didClose(context *glsp.Context, params *protocol.DidCloseTextDocumentParams) error {
	delete(docs, params.TextDocument.URI)
	return nil
}

func hover(context *glsp.Context, params *protocol.HoverParams) (*protocol.Hover, error) {
	_, ok := docs[params.TextDocument.URI]
	if !ok {
		return nil, nil
	}

	return nil, nil
}
