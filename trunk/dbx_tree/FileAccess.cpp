#include "FileAccess.h"

CFileAccess::CFileAccess(const char* FileName)
{
	m_FileName = new char[strlen(FileName) + 1];
	strcpy_s(m_FileName, strlen(FileName) + 1, FileName);
	m_Cipher = NULL;
}

CFileAccess::~CFileAccess()
{
	delete [] m_FileName;
}

void CFileAccess::SetCipher(CCipher * Cipher)
{
	m_Cipher = Cipher;
}


unsigned int CFileAccess::Read(void* Buf, unsigned int Source, unsigned int Size)
{
	if (m_Cipher)
	{
		unsigned char * cryptbuf;
		unsigned int startadd = Source % m_Cipher->BlockSizeBytes();
		unsigned int endadd   = (m_Cipher->BlockSizeBytes() - ((Size + Source) % m_Cipher->BlockSizeBytes())) % m_Cipher->BlockSizeBytes();

		cryptbuf = new unsigned char [startadd + Size + endadd];
		mRead(cryptbuf, Source - startadd, startadd + Size + endadd);
		m_Cipher->Decrypt(cryptbuf, startadd + Size + endadd);
		
		memcpy(Buf, cryptbuf + startadd, Size);

		delete [] cryptbuf;

	} else {
		mRead(Buf, Source, Size);
	}

	return Size;
}
unsigned int CFileAccess::Write(void* Buf, unsigned int Dest, unsigned int Size)
{
	if (m_Cipher)
	{
		unsigned char * cryptbuf;
		unsigned int startadd = Dest % m_Cipher->BlockSizeBytes();
		unsigned int endadd   = (m_Cipher->BlockSizeBytes() - ((Size + Dest) % m_Cipher->BlockSizeBytes())) % m_Cipher->BlockSizeBytes();

		cryptbuf = new unsigned char [startadd + Size + endadd];
		
		if (startadd > 0)
		{
			mRead(cryptbuf, Dest - startadd, m_Cipher->BlockSizeBytes());
			m_Cipher->Decrypt(cryptbuf, m_Cipher->BlockSizeBytes());
		}

		if (endadd > 0)
		{
			mRead(cryptbuf + startadd + Size + endadd - m_Cipher->BlockSizeBytes(), Dest + Size + endadd - m_Cipher->BlockSizeBytes(), m_Cipher->BlockSizeBytes());
			m_Cipher->Decrypt(cryptbuf + startadd + Size + endadd - m_Cipher->BlockSizeBytes(), m_Cipher->BlockSizeBytes());
		}

		memcpy(cryptbuf + startadd, Buf, Size);
		m_Cipher->Encrypt(cryptbuf, startadd + Size + endadd);		

		mWrite(cryptbuf, Dest - startadd, startadd + Size + endadd);

		delete [] cryptbuf;

	} else {
		mWrite(Buf, Dest, Size);
	}

	return Size;
}
/*
unsigned int CFileAccess::Move(unsigned int Source, unsigned int Dest, unsigned int Size)
{

}
*/