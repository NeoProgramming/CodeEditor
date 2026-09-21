// syntaxstyle.cpp
#include "syntaxstyle.h"

QVector<SyntaxElementStyle> defaultSyntaxElements()
{
	return {
		{ "default",  "Default text",       "Consolas", false, false, QColor("#000000") },
		{ "keyword",  "Keyword",            "Consolas", true,  false, QColor("#0000cc") },
		{ "type",     "Type / Class",       "Consolas", false, false, QColor("#008080") },
		{ "function", "Function",           "Consolas", false, false, QColor("#006400") },
		{ "string",   "String literal",     "Consolas", false, false, QColor("#008000") },
		{ "char",     "Char literal",       "Consolas", false, false, QColor("#008000") },
		{ "number",   "Number",             "Consolas", false, false, QColor("#800000") },
		{ "comment",  "Comment",            "Consolas", true,  true,  QColor("#808080") },
		{ "preproc",  "Preprocessor",       "Consolas", false, false, QColor("#808000") },
		{ "asm",      "Assembly insert",    "Courier New", false, false, QColor("#a000a0") },
		{ "script",   "Script insert",      "Courier New", false, false, QColor("#a0522d") },
		{ "json",     "JSON insert",        "Courier New", false, false, QColor("#2e8b57") },
		{ "xml",      "XML insert",         "Courier New", false, false, QColor("#4682b4") },
		{ "quasi",    "Quasiquotation",     "Courier New", false, false, QColor("#b8860b") },
		{ "macro",    "Lexical macro",      "Courier New", false, false, QColor("#cd5c5c") },
		{ "smacro",   "Syntactic macro",    "Courier New", false, false, QColor("#dc143c") },
	};
}
