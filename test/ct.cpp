#include "ct.h"

// пространство имен
namespace ct
{
	// это пробел
	bool	isSpace(int c)
	{
		return c == ' ' || c == '\t';
	}
	
	bool	isNewline(int c)
	{
		return (c == '\n' || c == '\r');
	}

	bool	isBlank(int c)
	{
		return isSpace(c) || isNewline(c);
	}
	
	bool	isDigit(int c)
	{
		return c>='0' && c<='9';
	}

	bool	isHexDigit(int c)
	{
		return c >= '0' && c <= '9' 
			|| c >= 'A' && c <= 'F'
			|| c >= 'a' && c <= 'f';
	}

	bool	isOctDigit(int c)
	{
		return c >= '0' && c <= '7';
	}

	bool	isBinDigit(int c)
	{
		return c == '0' || c == '1';
	}

	bool    isQuote1(int c)
	{
		return c == '\'';
	}

	bool	isQuote2(int c)
	{
		return c == '"';
	}

	bool	isQuote(int c)
	{
		return c == '"' || c == '\'';
	}

	bool	isOper(int c)
	{
		return c == '~' || c == '`' || c == '@' || c == '#' || c == '$' || c == '%'
			|| c == '^' || c == '&' || c == '*' || c == '(' || c == ')' || c == '-'
			|| c == '+' || c == '=' || c == '|' || c == '!' || c == '{' || c == '}'
			|| c == '[' || c == ']' || c == ':' || c == ';' || c == ',' || c == '<'
			|| c == '>' || c == '/' || c == '?' || c == '.' || c == '\\';
	}

	bool	isAlpha(int c)
	{
		return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z';
	}

	bool	isIdstart(int c)
	{
		return isAlpha(c) || c == '_';
	}

	bool	isIdnext(int c)
	{
		return isIdstart(c) || isDigit(c);
	}

	bool    isString(int c)
	{
		return (!isQuote(c) && !isNewline(c));
	}
	
	bool	isSpec(int c)
	{
		return (c == '\\');
	}
	
	bool	isAlnum(int c)
	{
		return isAlpha(c) || isDigit(c);
	}

	bool	isExp(int c)
	{
		return c == 'e' || c == 'E' || c=='p' || c=='P';
	}
	bool isFName(int c)
	{
		return c>=0x20 && c!='+' && c!='.' && c!='>' && c!='<' 
			&& c!='|' && c!='*' && c!='?' && c!='/' 
			&& c!='\\' && c!=':' && c!='"';
	}
	
	bool    isBase64(int c) 
	{
		return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '+' || c == '/' || c == '-' || c == '_';
	}

}; // namespace ct



