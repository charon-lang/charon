#!/usr/bin/env bash

TREE_SITTER_PATH=../tree-sitter-charon

(cd $TREE_SITTER_PATH && tree-sitter generate)
(cd $TREE_SITTER_PATH && tree-sitter build --wasm)

mkdir -p ./out
cp $TREE_SITTER_PATH/tree-sitter-charon.wasm ./out
cp $TREE_SITTER_PATH/queries/highlights.scm ./out