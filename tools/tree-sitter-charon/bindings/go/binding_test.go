package tree_sitter_charon_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_charon "git.thenest.dev/wux/charon/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_charon.Language())
	if language == nil {
		t.Errorf("Error loading Charon grammar")
	}
}
