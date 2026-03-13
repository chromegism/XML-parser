#pragma once

#include <string>
#include <optional>
#include <vector>
#include <iostream>

namespace XML {
	struct Cursor {
		size_t column = 1, line = 1, ch = 0;

		std::string to_string() {
			return "(" + std::to_string(line) + ":" + std::to_string(column) + " - " + std::to_string(ch) + ")";
		}
	};

	enum class TokenType {
		UDef,
		OpenBrack,
		CloseBrack,
		FSlash,
		Identifier,
		Equal,
		String,
		Text
	};

	struct Token {
		TokenType type = TokenType::UDef;
		std::optional<std::string> lexeme;
		Cursor cursor;

		static Token makeToken(TokenType t, std::string le, Cursor c) {
			std::optional<std::string> lex;
			if (le.size() > 0) {
				lex = le;
			}

			return { t, lex, c };
		}
	};

	class Lexer {
		const std::string* input;
		Cursor cursor;

	public:
		Lexer(const std::string& str) : input(&str) {}

		char at() {
			return input->operator[](cursor.ch);
		}

		char isEOF() {
			return cursor.ch >= input->size();
		}

		char isNextEOF() {
			return cursor.ch + 1 >= input->size();
		}

		char next() {
			if (isEOF()) throw std::runtime_error("Unexpected EOF in XML file");
			cursor.ch++;

			cursor.column++;
			checkNewline(at());

			return at();
		}

		void checkNewline(char c) {
			if (c == '\n') {
				cursor.line++;
				cursor.column = 1;
			}
		}

		// All lexing functions expect to begin on the token and end after the token
		void skipWhitespace() {
			char c = at();
			checkNewline(c);
			while ((std::isblank(c) || c == '\n') && !isNextEOF()) {
				c = next();
			}
		}

		std::vector<Token> lex() {
			std::vector<Token> tokens;

			bool textMode = false;

			while (!isEOF()) {
				if (textMode) {
					Token t = LexText();
					if (t.lexeme.has_value())
						tokens.push_back(t);
				}

				switch (at()) {
					case '<': tokens.emplace_back(Token::makeToken(TokenType::OpenBrack, "<", cursor)); next(); textMode = false;  break;
					case '>': tokens.emplace_back(Token::makeToken(TokenType::CloseBrack, ">", cursor)); next(); textMode = true; break;
					case '=': tokens.emplace_back(Token::makeToken(TokenType::Equal, "=", cursor)); next(); break;
					case '/': tokens.emplace_back(Token::makeToken(TokenType::FSlash, "/", cursor)); next(); break;
					case '"': tokens.emplace_back(lexString()); break;
					default:
						if (std::isalpha(at())) {
							tokens.emplace_back(lexIdentifier());
							break;
						}
						else {
							std::cerr << "Unrecognised character - " << at() << '\n';
							next();
						}
				}

				skipWhitespace();
			}
			
			return tokens;
		}

		Token LexText() {
			std::string str;

			std::string whitespaceBuffer;

			while (at() != '<') {
				if (std::isblank(at()) || at() == '\n')
					whitespaceBuffer += at();
				else {
					str += whitespaceBuffer;
					whitespaceBuffer.clear();
					str += at();
				}
				next();
			}

			return Token::makeToken(TokenType::Text, str, cursor);
		}

		Token lexIdentifier() {
			std::string str;

			char c = at();
			while (std::isalpha(c) && !isNextEOF()) {
				str.push_back(c);
				c = next();
			}

			return Token::makeToken(TokenType::Identifier, str, cursor);
		}

		Token lexString() {
			std::string str;

			char c = at();
			if (c != '"') throw std::runtime_error("String does not begin on a quote");
			c = next();

			while (c != '"' && !isNextEOF()) {
				str.push_back(c);
				c = next();
			}

			(void)next();

			return Token::makeToken(TokenType::String, str, cursor);
		}

		Token lexText() {
			return {};
		}
	};


	struct Entry {
		std::string name;
		std::map<std::string, std::string> attributes;
		std::string text;
		
		std::vector<Entry> children;
		bool isSingular;

		std::string to_string(size_t offset = 0) const {
			std::string flattened;
			size_t counter = 0;
			for (const auto& [key, value] : attributes) {
				counter++;

				flattened += key + "=\"" + value + "\"";
				if (counter != attributes.size())
					flattened += ' ';
			}

			std::string this_one;
			std::string children_str;
			if (isSingular) {
				this_one = std::string(offset, '\t') + "<" + name + "/>";
				return this_one + '\n';
			}
			else if (attributes.size() == 0)
				this_one = std::string(offset, '\t') + "<" + name + ">";
			else
				this_one = std::string(offset, '\t') + "<" + name + " " + flattened + ">";

			for (const auto& child : children) {
				children_str += child.to_string(offset + 1);
			}
			if (!text.empty())
				children_str += std::string(offset + 1, '\t') + text + '\n';

			std::string ending;
			if (children.size() != 0) {
				this_one += '\n';
				ending = std::string(offset, '\t') + "</" + name + ">\n";
			}
			else {
				ending = "</" + name + ">\n";
			}

			return this_one + children_str + ending;
		}
	};


	class Parser {
		std::vector<Token> tokens;
		size_t index = 0;

	public:
		Parser(std::vector<Token>&& toks) : tokens(std::move(toks)) {}

		Token& at() {
			return tokens[index];
		}

		bool isEOF() {
			return index >= tokens.size();
		}

		bool isNextEOF() {
			return index + 1 >= tokens.size();
		}

		Token& next() {
			Token& t = at();
			index++;
			if (isEOF()) throw std::runtime_error("Unexpected EOF at " + t.cursor.to_string());
			return at();
		}

		bool check(TokenType type) {
			return at().type == type;
		}

		bool expect(TokenType type) {
			return next().type == type;
		}

		bool expect(TokenType type, std::string& lexeme) {
			Token& t = next();
			if (t.type == type) {
				lexeme = t.lexeme.value();
				return true;
			}
			return false;
		}

		Token& peek(size_t offset) {
			if (index + offset >= tokens.size())
				throw std::runtime_error("Peek overshoots token array");
			return tokens[index + offset];
		}

		bool peekCheck(size_t offset, TokenType type) {
			return peek(offset).type == type;
		}

		std::map<std::string, std::string> parseAttributes() {
			std::map<std::string, std::string> attribs;

			do {
				std::string name = at().lexeme.value();
				if (attribs.find(name) != attribs.end())
					throw std::runtime_error("Attributes must be unique - " + name);

				expect(TokenType::Equal);

				std::string value;
				expect(TokenType::String, value);

				attribs[name] = value;

			} while (expect(TokenType::Identifier));
			
			return attribs;
		}

		std::vector<Entry> parseChildren(Entry& parent) {
			std::vector<Entry> children;

			if (!check(TokenType::OpenBrack))
				throw std::runtime_error("Entry must have a closing statement");

			do {
				if (peekCheck(1, TokenType::FSlash)) {
					(void)next(); // Offset peekCheck

					std::string lexeme;

					if (!expect(TokenType::Identifier, lexeme))
						throw std::runtime_error("Closing statement must have an identifier");
					if (lexeme != parent.name)
						throw std::runtime_error("Closing statement must have the same identifier as the opening statement");
					if (!expect(TokenType::CloseBrack))
						throw std::runtime_error("Closing statement must end with a >");

					// Just to reset it to the beginning of the next statement
					if (isNextEOF()) return children;
					(void)next();

					return children;
				}
				
				if (check(TokenType::Text)) {
					std::string text = at().lexeme.value();
					(void)next();
					parent.text += text;
				}
				else {
					children.push_back(parse());
				}

			} while (check(TokenType::OpenBrack) || check(TokenType::Text));

			return children;
		}

		Entry parse() {
			Entry entry;

			if (tokens.size() == 0) return {};

			if (!check(TokenType::OpenBrack))
				throw std::runtime_error("Entry must start with a <");

			if (!expect(TokenType::Identifier, entry.name))
				throw std::runtime_error("Entry must have an identifier");
			
			if (expect(TokenType::Identifier)) {
				entry.attributes = parseAttributes();
			}

			entry.isSingular = false;
			if (check(TokenType::FSlash)) {
				entry.isSingular = true;
				(void)next();
			}
			if (!check(TokenType::CloseBrack))
				throw std::runtime_error("Entry must end with a >");

			// Just to offset for parsing children
			(void)next();

			if (!entry.isSingular)
				entry.children = parseChildren(entry);

			return entry;
		}
	};

	std::string loadFile(const std::string& path) {
		std::ifstream in(path);
		if (!in) {
			throw std::runtime_error("Invalid XML path");
		}
		std::stringstream ss;
		ss << in.rdbuf();

		return ss.str();
	}

	Entry parseString(std::string& str) {
		auto toks = Lexer(str).lex();
		return Parser(std::move(toks)).parse();
	}
}
