import * as vscode from "vscode";
import { Parser, Language, Query } from "web-tree-sitter";
import fs from "fs";

const terms: {[key: string]: { token?: string, modifiers?: string[] }} = {
    "type": {},
    "namespace": {},
    "function": {},
    "variable": {},
    "number": {},
    "string": {},
    "string.escape": { token: "string", modifiers: ["escape"] },
    "invalid": {},
    "comment": {},
    "constant": { token: "variable", modifiers: ["readonly", "defaultLibrary"] },
    "keyword": {},
    "operator": {},
    "modifier": { token: "type", modifiers: ["modification"] },
    "struct": {},
    "enum": {},
    "enumMember": {},
    "languageDefinedValue": { token: "macro" },
    "parameter": {},
    "decorator": {},
    "property": {}
};

const legend = (() => {
    let tokens: string[] = [];
    let modifiers: string[] = [];
    for(const term of Object.entries(terms)) {
        const token = term[1].token ?? term[0];
        if(!tokens.includes(token)) tokens.push(token);
        for(const modifier of term[1].modifiers ?? []) {
            if(!modifiers.includes(modifier)) modifiers.push(modifier);
        }
    }
    return new vscode.SemanticTokensLegend(tokens, modifiers);
})();

class TokenProvider implements vscode.DocumentSemanticTokensProvider {
    readonly language: Language;
    readonly parser: Parser;
    readonly query: Query;

    constructor(language: Language, parser: Parser, highlights: string) {
        this.language = language;
        this.parser = parser;
        this.query = this.language.query(highlights);
    }

    async provideDocumentSemanticTokens(doc: vscode.TextDocument, token: vscode.CancellationToken): Promise<vscode.SemanticTokens> {
        const tree = this.parser.parse(doc.getText());

        let foundCaptures: { term: string; range: vscode.Range, nodeId: number }[] = [];
        for(const match of this.query.matches(tree!.rootNode, {}).reverse()) {
            for(const capture of match.captures) {
                if(!(capture.name in terms)) continue;
                if(foundCaptures.find((c) => c.nodeId == capture.node.id) != undefined) continue;

                foundCaptures.push({
                    term: capture.name,
                    nodeId: capture.node.id,
                    range: new vscode.Range(
                        new vscode.Position(capture.node.startPosition.row, capture.node.startPosition.column),
                        new vscode.Position(capture.node.endPosition.row, capture.node.endPosition.column)
                    )
                });
            }
        }

        const builder = new vscode.SemanticTokensBuilder(legend);
        for(const capture of foundCaptures) {
            const token = terms[capture.term].token ?? capture.term;
            const modifiers = terms[capture.term].modifiers;

            if(capture.range.start.line === capture.range.end.line) {
                builder.push(capture.range, token, modifiers);
                continue;
            }

            let line = capture.range.start.line;
            builder.push(new vscode.Range(capture.range.start, doc.lineAt(line).range.end), token, modifiers);
            for(line = line + 1; line < capture.range.end.line; line++) builder.push(doc.lineAt(line).range, token, modifiers);
            builder.push(new vscode.Range(doc.lineAt(line).range.start, capture.range.end), token, modifiers);
        }
        return builder.build();
    }
}

export async function activate(context: vscode.ExtensionContext) {
    const highlights = fs.readFileSync(context.asAbsolutePath("out/highlights.scm")).toString();

    await Parser.init();

    let lang = await Language.load(context.asAbsolutePath("out/tree-sitter-charon.wasm"));
    let parser = new Parser;
    parser.setLanguage(lang);

    const provider = new TokenProvider(lang, parser, highlights);
    context.subscriptions.push(vscode.languages.registerDocumentSemanticTokensProvider({ language: "charon" }, provider, legend));
}

export function deactivate() { }