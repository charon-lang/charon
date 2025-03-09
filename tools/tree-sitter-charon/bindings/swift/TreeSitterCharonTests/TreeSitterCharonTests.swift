import XCTest
import SwiftTreeSitter
import TreeSitterCharon

final class TreeSitterCharonTests: XCTestCase {
    func testCanLoadGrammar() throws {
        let parser = Parser()
        let language = Language(language: tree_sitter_charon())
        XCTAssertNoThrow(try parser.setLanguage(language),
                         "Error loading Charon grammar")
    }
}
