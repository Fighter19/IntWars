#include "Logger.h"

#include <errno.h>
#include <string.h>

#include <stdarg.h>

#include <time.h>

#include <vector>

/** Evaluates to the number of elements in an array (compile-time!) */
#define ARRAYCOUNT(X) (sizeof(X) / sizeof(*(X)))

std::string & AppendVPrintf(std::string & str, const char * format, va_list args)
{

	char buffer[2048];
	int len;
#ifdef va_copy
	va_list argsCopy;
	va_copy(argsCopy, args);
#else
#define argsCopy args
#endif
#ifdef _MSC_VER
	// MS CRT provides secure printf that doesn't behave like in the C99 standard
	if ((len = _vsnprintf_s(buffer, ARRAYCOUNT(buffer), _TRUNCATE, format, argsCopy)) != -1)
#else  // _MSC_VER
	if ((len = vsnprintf(buffer, ARRAYCOUNT(buffer), format, argsCopy)) < static_cast<int>(ARRAYCOUNT(buffer)))
#endif  // else _MSC_VER
	{
		// The result did fit into the static buffer
#ifdef va_copy
		va_end(argsCopy);
#endif
		str.append(buffer, static_cast<size_t>(len));
		return str;
	}
#ifdef va_copy
	va_end(argsCopy);
#endif

	// The result did not fit into the static buffer, use a dynamic buffer:
#ifdef _MSC_VER
	// for MS CRT, we need to calculate the result length
	len = _vscprintf(format, args);
	if (len == -1)
	{
		return str;
	}
#endif  // _MSC_VER

	// Allocate a buffer and printf into it:
#ifdef va_copy
	va_copy(argsCopy, args);
#endif
	std::vector<char> Buffer(static_cast<size_t>(len) + 1);
#ifdef _MSC_VER
	vsprintf_s(&(Buffer.front()), Buffer.size(), format, argsCopy);
#else  // _MSC_VER
	vsnprintf(&(Buffer.front()), Buffer.size(), format, argsCopy);
#endif  // else _MSC_VER
	str.append(&(Buffer.front()), Buffer.size() - 1);
#ifdef va_copy
	va_end(argsCopy);
#endif
	return str;
}


std::string & Printf(std::string & str, const char * format, ...)
{
	str.clear();
	va_list args;
	va_start(args, format);
	std::string & retval = AppendVPrintf(str, format, args);
	va_end(args);
	return retval;
}





std::string Printf(const char * format, ...)
{
	std::string res;
	va_list args;
	va_start(args, format);
	AppendVPrintf(res, format, args);
	va_end(args);
	return res;
}

const std::string Logger::CurrentDateTime() 
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

	return buf;
}

const std::string Logger::CurrentTime()
{
	time_t     now = time(0);
	struct tm  tstruct;
	char       buf[80];
	tstruct = *localtime(&now);
	strftime(buf, sizeof(buf), "%X", &tstruct);

	return buf;
}

Logger::Logger()
{
    m_pLogFile = stdout;
}

Logger::~Logger()
{
	if (m_pLogFile != stdout)
	{
		if (isHTML) fprintf(m_pLogFile, "</table>");
		fclose(m_pLogFile);
	}
}

void Logger::setLogFile(const char *filename, bool plainText, bool showOnScreen)
{
    m_pLogFile = loadFileStream(m_pLogFile, filename);
	 isHTML = !plainText;
	 printToScreen = showOnScreen;

	 if (isHTML)
		 fprintf(m_pLogFile, "<h1>Log file (%s)</h1><table><tr><td><b>Tag</b></td><td><b>Time</b></td><td><b>File</b></td><td><b>Function</b></td><td><b>Message</b></td></tr>", CurrentDateTime().c_str());
}

void Logger::log(const std::string &tag, const char *funcName,
                 const char *sourceFile, unsigned int lineNum, 
					  const std::string fmt, ...)
{
	std::string fileBuffer;

	fillFileBuffer(fileBuffer, tag, fmt, funcName, sourceFile, lineNum);

	// Print to file
	va_list args;
	const char* bufferCStr = fileBuffer.c_str();
	//va_start(args, bufferCStr);
	//vfprintf(m_pLogFile, bufferCStr, args);
	//va_end(args);

	if (printToScreen)
	{
		std::string outputBuffer;
		fillOutputBuffer(outputBuffer, tag, fmt, funcName, sourceFile, lineNum);
		// Print to screen
		const char* bufferCStr = outputBuffer.c_str();
		va_start(args, fmt);
		vfprintf(stdout, bufferCStr, args);
		va_end(args);
	}
}


void Logger::flush()
{
    // Flush output buffer
    fflush(m_pLogFile);
}

void Logger::fillOutputBuffer(std::string &outputBuffer, const std::string &tag, const std::string &msg,
	const char *funcName, const char *sourceFile, unsigned int lineNum)
{
	// Old format
	if(!tag.empty())
		outputBuffer = "[" + tag + "] ";
	else outputBuffer = "[NOTAG] ";

	outputBuffer += " ";
	outputBuffer += msg;
	outputBuffer += "\n";
}

void Logger::fillFileBuffer(std::string &outputBuffer, const std::string &tag, const std::string &msg,
	const char *funcName, const char *sourceFile, unsigned int lineNum)
{
	// Old format
	if (!isHTML)
	{
		if (!tag.empty())
			outputBuffer = "[" + tag + "] ";
		else outputBuffer = "[NOTAG] ";

		if (sourceFile != NULL)
		{
			outputBuffer += sourceFile;
			outputBuffer += " ";
		}

		if (funcName != NULL && lineNum != 0)
		{
			outputBuffer += funcName;
			outputBuffer += ":";
			// Convert int to char
			outputBuffer += std::to_string(lineNum);
		}

		outputBuffer += ": ";
		outputBuffer += msg;
		outputBuffer += "\n";
	}
	else
	{
		outputBuffer = "<tr>";
		if (!tag.empty())
			outputBuffer += "<td>" + tag + "</td>";
		else outputBuffer = "<td>NOTAG</td>";

		outputBuffer += "<td>" + CurrentTime() + "</td>";

		if (sourceFile != NULL)
		{
			outputBuffer += "<td>";
			outputBuffer += sourceFile;
			outputBuffer += "</td>";
		}

		if (funcName != NULL && lineNum != 0)
		{
			outputBuffer += "<td>";
			outputBuffer += funcName;
			outputBuffer += ":";
			// Convert int to char
			outputBuffer += std::to_string(lineNum);
			outputBuffer += "</td>";
		}

		outputBuffer += "<td>";
		outputBuffer += msg;
		outputBuffer += "</td></tr>";
	}
}

FILE *Logger::loadFileStream(FILE *stream, const char *filename)
{
    // overwrite previous logs
    FILE* newStream = fopen(filename, "w");

    if(newStream == NULL)
    {

        CORE_ERROR(
                   "Failed to open logger file: " + std::string(filename) +
                   ". Using console output...\n\t" +
                   strerror(errno)
        );

        // Use previous stream
        return stream;
    }

    return newStream;
}

