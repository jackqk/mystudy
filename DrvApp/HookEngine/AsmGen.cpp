#include <stdafx.h>
#include "asmjit/asmjit/src/asmjit/x86.h"
#include "asmjit/asmtk/src/asmtk/asmtk.h"
#include "AsmGen.h"

AsmGen::AsmGen(ULONGLONG Ip, AsmGenType Type)
{
	m_Ip = Ip;
	m_X64Code = (Type == ASMGEN_64);
	m_StreamData = nullptr;

	m_Stream.reserve(64);
}

AsmGen::~AsmGen()
{
	if (m_StreamData)
		free(m_StreamData);

	// m_Stream is cleaned up by its own destructor
}

bool AsmGen::AddCode(char *Instruction)
{
	_strlwr(Instruction);
	asmjit::ArchInfo::Type type = (m_X64Code) ?
		asmjit::ArchInfo::kTypeX64 : asmjit::ArchInfo::kTypeX86;

	// Initialize CodeHolder with architecture
	asmjit::CodeInfo ci;
	ci.init(type, 0, m_Ip);

	// Initialize CodeHolder
	asmjit::CodeHolder code;
	asmjit::Error err = code.init(ci);

	if (err)
	{
		LOG_DEBUG("code init failed ins = %s err = %d\r\n", Instruction, err);
		__debugbreak();
		return false;
	}

	// Parse instructions
	asmjit::X86Assembler a(&code);
	err = asmtk::AsmParser(&a).parse(Instruction, strlen(Instruction));

	if (err)
	{
		LOG_DEBUG("ins = %s err = %d %s\r\n", Instruction, err, asmjit::DebugUtils::errorAsString(err));
		__debugbreak();
		return false;
	}

	// Send each assembled byte to the stream
	code.sync();
	asmjit::CodeBuffer& buffer = code.getSectionEntry(0)->getBuffer();

	for (unsigned int i = 0; i < buffer.getLength(); i++)
		m_Stream.push_back(buffer.getData()[i]);

	// Shift the internal instruction pointer
	m_Ip += buffer.getLength();
	return true;
}

bool AsmGen::AddCode(const char *Format, ...)
{
	char buffer[2048] = {};
	va_list va;

	va_start(va, Format);
	vsprintf_s(buffer, 2048, Format, va);
	va_end(va);

	return AddCode(buffer);
}

bool AsmGen::AddCodeArray(const char **Instructions, size_t Count)
{
	for (size_t i = 0; i < Count; i++)
	{
		if (!AddCode(Instructions[i]))
			return false;
	}

	return true;
}

BYTE *AsmGen::GetStream(bool Free)
{
	BYTE *data = (BYTE *)malloc(m_Stream.size() * sizeof(BYTE));

	for (size_t i = 0; i < m_Stream.size(); i++)
		data[i] = m_Stream.at(i);

	m_StreamData = (Free) ? data : nullptr;

	return data;
}

size_t AsmGen::GetStreamLength()
{
	return m_Stream.size();
}

bool AsmGen::WriteStreamTo(BYTE *Buffer, size_t BufferSize)
{
	// Disregard size checks if someone sends -1
	if (BufferSize != (size_t)-1)
	{
		if (m_Stream.size() > BufferSize)
			return false;
	}
	if (BufferSize == size_t(-1))
	{
		BufferSize = m_Stream.size();
	}

	memset(Buffer, 0, BufferSize);
	memcpy(Buffer, m_Stream.data(), m_Stream.size());
	return true;
}