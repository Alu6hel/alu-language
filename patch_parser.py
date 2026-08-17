
import sys
with open("cpp_frontend/parser.cpp", "r") as f:
    content = f.read()
patch = """
                while (temp < tokens.size() && tokens[temp].type != TokenType::TOK_EOF && tokens[temp].type != TokenType::TOK_SEMICOLON) {
                    if (tokens[temp].type == TokenType::TOK_LESS_THAN) nesting++;
                    else if (tokens[temp].type == TokenType::TOK_GREATER_THAN) {
                        nesting--;
                        if (nesting == 0) {
                            if (temp + 1 < tokens.size() && tokens[temp + 1].type == TokenType::TOK_LPAREN) {
                                isGeneric = true;
                            }
                            break;
                        }
                    }
                    temp++;
                }
"""
patched = """
                while (temp < tokens.size() && tokens[temp].type != TokenType::TOK_EOF && tokens[temp].type != TokenType::TOK_SEMICOLON) {
                    std::cout << "LOOKAHEAD: token=" << tokens[temp].value << " type=" << (int)tokens[temp].type << " nesting=" << nesting << std::endl;
                    if (tokens[temp].type == TokenType::TOK_LESS_THAN) nesting++;
                    else if (tokens[temp].type == TokenType::TOK_GREATER_THAN) {
                        nesting--;
                        if (nesting == 0) {
                            std::cout << "LOOKAHEAD END: next token=" << tokens[temp+1].value << " type=" << (int)tokens[temp+1].type << std::endl;
                            if (temp + 1 < tokens.size() && tokens[temp + 1].type == TokenType::TOK_LPAREN) {
                                isGeneric = true;
                                std::cout << "IS_GENERIC SET TO TRUE!" << std::endl;
                            }
                            break;
                        }
                    }
                    temp++;
                }
"""
content = content.replace(patch, patched)
with open("cpp_frontend/parser.cpp", "w") as f:
    f.write(content)

