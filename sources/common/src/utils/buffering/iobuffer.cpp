/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "utils/buffering/iobuffer.h"
#include "utils/logging/logging.h"

#ifdef WITH_SANITY_INPUT_BUFFER
#define SANITY_INPUT_BUFFER \
o_assert(_consumed<=_published); \
o_assert(_published<=_size);
#else
#define SANITY_INPUT_BUFFER
#endif

//#define TRACK_ALLOCATIONS

/*
	#include <execinfo.h>
	void *array[100]; \
	int arraySize = backtrace(array, 100); \
	char **messages = backtrace_symbols(array, arraySize); \
	for (int i = 0; i < arraySize && messages != NULL; ++i) \
	{ \
		fprintf(stderr, "[bt]: (%d) %s\n", i, messages[i]); \
	} \


map<DWORD,pair<uint32_t,string> > gBackTrace;
PVOID *gpTrace=NULL;
#include <Dbghelp.h>
void CaptureBacktrace(){
	if(gpTrace==NULL){
		SymSetOptions ( SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME );
		if (!::SymInitialize( ::GetCurrentProcess(), "http://msdl.microsoft.com/download/symbols", TRUE ))
			return;
		gpTrace=new PVOID[65536];
	}
	DWORD hash;
	DWORD captured=CaptureStackBackTrace(1,65535,gpTrace,&hash);
	map<DWORD,pair<uint32_t,string> >::iterator found=gBackTrace.find(hash);
	if(found!=gBackTrace.end()){
			found->second.first++;
			if((found->second.first%1000)==0){
				fprintf(stderr,"--------------------------\n");
				fprintf(stderr,"%"PRIu32"\n",found->second.first);
				fprintf(stderr,"%"PRIu32"\n%s\n",found->first,STR(found->second.second));
			}
	} else {
		string stack="";
		for(DWORD i=0;i<captured&&i<65536;i++){
			IMAGEHLP_LINE64 fileLine={0};
			ULONG64 buffer[ (sizeof( SYMBOL_INFO ) + 1024 + sizeof( ULONG64 ) - 1) / sizeof( ULONG64 ) ] = { 0 };
			SYMBOL_INFO *info = (SYMBOL_INFO *)buffer;
			info->SizeOfStruct = sizeof( SYMBOL_INFO );
			info->MaxNameLen = 1024;
			DWORD64 displacement1 = 0;
			DWORD displacement2 = 0;
			if ((SymFromAddr( ::GetCurrentProcess(), (DWORD64)gpTrace[ i ], &displacement1, info ))
				&&(SymGetLineFromAddr64( ::GetCurrentProcess(), (DWORD64)gpTrace[ i ], &displacement2,&fileLine))){
					stack+=string(info->Name,info->NameLen);
					stack+=format(" %s:%"PRIu32"\n",fileLine.FileName,fileLine.LineNumber);
			}
		}
		if(stack.find("le_ba.cpp")==string::npos){
			pair<uint32_t,string> p=pair<uint32_t,string>(1,stack);
			gBackTrace[hash]=p;
		}
	}
}
 */

#ifdef TRACK_ALLOCATIONS
uint64_t gAllocated = 0;
uint64_t gDeallocated = 0;
uint64_t gAllocationCount = 0;
uint64_t gDeallocationCount = 0;
uint64_t gIOBuffConstructorCount = 0;
uint64_t gIOBuffDestructorCount = 0;
uint64_t gLastAllocatedSize = 0;
uint64_t gLastDeallocatedSize = 0;
uint64_t gMaxSize = 0;
uint64_t gMemcpySize = 0;
uint64_t gMemcpyCount = 0;
uint64_t gSmartCopy = 0;
uint64_t gNormalCopy = 0;
#define DUMP_ALLOCATIONS_TRACKING(kind, pointer) \
do { \
	fprintf(stderr,"%p %s A/D Amounts (MB): %.2f/%.2f (%.2f); %"PRIu64"/%"PRIu64" (%"PRIu64"); %"PRIu64"/%"PRIu64" (%"PRIu64") A/D/M %"PRIu64"/%"PRIu64"/%"PRIu64" %.2f(%"PRIu64") %"PRIu64"/%"PRIu64"\n", \
		pointer, \
		kind, \
		(double)gAllocated/(1024.0*1024.0), \
		(double)gDeallocated/(1024.0*1024.0), \
		(double)(gAllocated-gDeallocated)/(1024.0*1024.0), \
		gAllocationCount, \
		gDeallocationCount, \
		gAllocationCount-gDeallocationCount, \
		gIOBuffConstructorCount, \
		gIOBuffDestructorCount, \
		gIOBuffConstructorCount-gIOBuffDestructorCount, \
		gLastAllocatedSize, \
		gLastDeallocatedSize, \
		gMaxSize, \
		(double)gMemcpySize/(1024.0*1024.0), \
		gMemcpyCount, \
		gSmartCopy, \
		gNormalCopy \
		); \
	fflush(stderr); \
}while(0)
#define TRACK_NEW(size,pointer) \
do { \
	gAllocated+=(size); \
	gAllocationCount++; \
	gLastAllocatedSize=(size); \
	gMaxSize=gMaxSize<(size)?(size):gMaxSize; \
	DUMP_ALLOCATIONS_TRACKING("   new",pointer); \
}while(0)
#define TRACK_DELETE(size,pointer) \
do { \
	gDeallocated+=(size); \
	gDeallocationCount++; \
	gLastDeallocatedSize=(size); \
	DUMP_ALLOCATIONS_TRACKING("delete",pointer); \
}while(0)
#define TRACK_IOBUFFER_CONSTRUCTOR(pointer) \
do { \
	gIOBuffConstructorCount++; \
	DUMP_ALLOCATIONS_TRACKING("constr",pointer); \
}while(0)
#define TRACK_IOBUFFER_DESTRUCTOR(pointer) \
do { \
	gIOBuffConstructorCount++; \
	DUMP_ALLOCATIONS_TRACKING(" destr",pointer); \
}while(0)
#define TRACK_IOBUFFER_MEMCPY(size,pointer) \
do { \
	gMemcpyCount++; \
	gMemcpySize+=(size); \
	if((gMemcpyCount%100)==0) \
		DUMP_ALLOCATIONS_TRACKING("memcpy",pointer); \
}while(0)
#define TRACK_SMART_COPY(pointer,smart) \
do { \
	if(smart) \
		gSmartCopy++; \
	else \
		gNormalCopy++; \
	if(((gSmartCopy+gNormalCopy)%100)==0) \
		DUMP_ALLOCATIONS_TRACKING("  copy",pointer); \
}while(0)
#else /* TRACK_ALLOCATIONS */
#define DUMP_ALLOCATIONS_TRACKING(kind, pointer)
#define TRACK_NEW(size,pointer)
#define TRACK_DELETE(size,pointer)
#define TRACK_IOBUFFER_CONSTRUCTOR(pointer)
#define TRACK_IOBUFFER_DESTRUCTOR(pointer)
#define TRACK_IOBUFFER_MEMCPY(size,pointer)
#define TRACK_SMART_COPY(pointer,smart)
#endif /* TRACK_ALLOCATIONS */

IOBuffer::IOBuffer() {
	_pBuffer = NULL;
	_size = 0;
	_published = 0;
	_consumed = 0;
	_minChunkSize = 4096;
	_dummy = sizeof (sockaddr_in);
	_sendLimit = 0xffffffff;
	SANITY_INPUT_BUFFER;
	TRACK_IOBUFFER_CONSTRUCTOR(this);
}

IOBuffer::~IOBuffer() {
	SANITY_INPUT_BUFFER;
	Cleanup();
	SANITY_INPUT_BUFFER;
	TRACK_IOBUFFER_DESTRUCTOR(this);
}

void IOBuffer::Initialize(uint32_t expected) {
	if ((_pBuffer != NULL) ||
			(_size != 0) ||
			(_published != 0) ||
			(_consumed != 0))
		ASSERT("This buffer was used before. Please initialize it before using");
	EnsureSize(expected);
}

bool IOBuffer::ReadFromTCPFd(int32_t fd, uint32_t expected, int32_t &recvAmount, int &err) {
	SANITY_INPUT_BUFFER;
	if (expected == 0) {
		err = SOCKERROR_ECONNRESET;
		SANITY_INPUT_BUFFER;
		return false;
	}

	if (_published + expected > _size) {
		if (!EnsureSize(expected)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
	}

	recvAmount = recv(fd, (char *) (_pBuffer + _published), expected, MSG_NOSIGNAL);
	if (recvAmount > 0) {
		_published += (uint32_t) recvAmount;
		SANITY_INPUT_BUFFER;
		return true;
	} else {
		err = recvAmount == 0 ? SOCKERROR_ECONNRESET : LASTSOCKETERROR;
		if ((err != SOCKERROR_EAGAIN)&&(err != SOCKERROR_EINPROGRESS)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
		return true;
	}
}

bool IOBuffer::ReadFromUDPFd(int32_t fd, int32_t &recvAmount, sockaddr_in &peerAddress) {
	SANITY_INPUT_BUFFER;
	if (_published + 65536 > _size) {
		if (!EnsureSize(65536)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
	}

	recvAmount = recvfrom(fd, (char *) (_pBuffer + _published), 65536,
			MSG_NOSIGNAL, (sockaddr *) & peerAddress, &_dummy);
	if (recvAmount > 0) {
		_published += (uint32_t) recvAmount;
		SANITY_INPUT_BUFFER;
		return true;
	} else {
		int err = LASTSOCKETERROR;
#ifdef WIN32
		if (err == SOCKERROR_ECONNRESET) {
			WARN("Windows is stupid enough to issue a CONNRESET on a UDP socket. See http://support.microsoft.com/?kbid=263823 for details");
			SANITY_INPUT_BUFFER;
			return true;
		}
#endif /* WIN32 */
		FATAL("Unable to read data from UDP socket. Error was: %d", err);
		SANITY_INPUT_BUFFER;
		return false;
	}
}

bool IOBuffer::ReadFromStdio(int32_t fd, uint32_t expected, int32_t &recvAmount) {
	SANITY_INPUT_BUFFER;
	if (_published + expected > _size) {
		if (!EnsureSize(expected)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
	}

	recvAmount = READ_FD(fd, (char *) (_pBuffer + _published), expected);
	if (recvAmount > 0) {
		_published += (uint32_t) recvAmount;
		SANITY_INPUT_BUFFER;
		return true;
	} else {
		SANITY_INPUT_BUFFER;
		return false;
	}
}

bool IOBuffer::ReadFromFs(File &file, uint32_t size) {
	SANITY_INPUT_BUFFER;
	if (size == 0) {
		SANITY_INPUT_BUFFER;
		return true;
	}
	if (_published + size > _size) {
		if (!EnsureSize(size)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
	}
	if (!file.ReadBuffer(_pBuffer + _published, size)) {
		SANITY_INPUT_BUFFER;
		return false;
	}
	_published += size;
	SANITY_INPUT_BUFFER;
	return true;
}

#ifdef HAS_MMAP

bool IOBuffer::ReadFromFs(MmapFile &file, uint32_t size) {
	SANITY_INPUT_BUFFER;
	if (size == 0) {
		SANITY_INPUT_BUFFER;
		return true;
	}
	if (_published + size > _size) {
		if (!EnsureSize(size)) {
			SANITY_INPUT_BUFFER;
			return false;
		}
	}
	if (!file.ReadBuffer(_pBuffer + _published, size)) {
		SANITY_INPUT_BUFFER;
		return false;
	}
	_published += size;
	SANITY_INPUT_BUFFER;
	return true;
}
#endif /* HAS_MMAP */

bool IOBuffer::ReadFromBuffer(const uint8_t *pBuffer, const uint32_t size) {
	SANITY_INPUT_BUFFER;
	if (!EnsureSize(size)) {
		SANITY_INPUT_BUFFER;
		return false;
	}
	memcpy(_pBuffer + _published, pBuffer, size);
	TRACK_IOBUFFER_MEMCPY(size, this);
	_published += size;
	SANITY_INPUT_BUFFER;
	return true;
}

void IOBuffer::ReadFromInputBuffer(IOBuffer *pInputBuffer, uint32_t start, uint32_t size) {
	SANITY_INPUT_BUFFER;
	ReadFromBuffer(GETIBPOINTER(*pInputBuffer) + start, size);
	SANITY_INPUT_BUFFER;
}

bool IOBuffer::ReadFromInputBuffer(const IOBuffer &buffer, uint32_t size) {
	SANITY_INPUT_BUFFER;
	if (!ReadFromBuffer(buffer._pBuffer + buffer._consumed, size)) {
		SANITY_INPUT_BUFFER;
		return false;
	}
	SANITY_INPUT_BUFFER;
	return true;
}

bool IOBuffer::ReadFromInputBufferWithIgnore(IOBuffer &src) {
	SANITY_INPUT_BUFFER;
	if (((_published - _consumed) == 0)
			&&((src._published - src._consumed) != 0)
			&&(_sendLimit == 0xffffffff)
			&&(src._sendLimit == 0xffffffff)
			) {
		//this is the ideal case in which "this" (the destiantion) doesn't
		//have anything inside it and bot the source and the destination
		//don't have any send limit. We just swap the guts
		uint8_t *_pTempBuffer = src._pBuffer;
		src._pBuffer = _pBuffer;
		_pBuffer = _pTempBuffer;

		uint32_t _tempSize = src._size;
		src._size = _size;
		_size = _tempSize;

		uint32_t _tempPublished = src._published;
		src._published = _published;
		_published = _tempPublished;

		uint32_t _tempConsumed = src._consumed;
		src._consumed = _consumed;
		_consumed = _tempConsumed;

		SANITY_INPUT_BUFFER;
		TRACK_SMART_COPY(this, true);
		return true;
	} else {
		//here we have either some bytes in the destination (this) or the source
		//has a send limit. We just copy the data and than call ignore
		//on the source which MUST take care of _sendLimit accordingly
		if ((src._published == src._consumed) || (src._sendLimit == 0)) {
			SANITY_INPUT_BUFFER;
			return true;
		}
		uint32_t ceil = min(GETAVAILABLEBYTESCOUNT(src), GETIBSENDLIMIT(src));
		if (!ReadFromBuffer(GETIBPOINTER(src), ceil)) {
			FATAL("Unable to copy data");
			SANITY_INPUT_BUFFER;
			return false;
		}
		if (!src.Ignore(ceil)) {
			FATAL("Unable to ignore data");
			SANITY_INPUT_BUFFER;
			return false;
		}
		SANITY_INPUT_BUFFER;
		TRACK_SMART_COPY(this, false);
		return true;
	}
}

bool IOBuffer::ReadFromString(string binary) {
	SANITY_INPUT_BUFFER;
	if (!ReadFromBuffer((uint8_t *) binary.data(), (uint32_t) binary.length())) {
		SANITY_INPUT_BUFFER;
		return false;
	}
	SANITY_INPUT_BUFFER;
	return true;
}

void IOBuffer::ReadFromByte(uint8_t byte) {
	SANITY_INPUT_BUFFER;
	EnsureSize(1);
	_pBuffer[_published] = byte;
	_published++;
	SANITY_INPUT_BUFFER;
}

bool IOBuffer::ReadFromBIO(BIO *pBIO) {
	SANITY_INPUT_BUFFER;
	if (pBIO == NULL) {
		SANITY_INPUT_BUFFER;
		return true;
	}
	int32_t bioAvailable = BIO_pending(pBIO);
	if (bioAvailable < 0) {
		FATAL("BIO_pending failed");
		SANITY_INPUT_BUFFER;
		return false;
	}
	if (bioAvailable == 0) {
		SANITY_INPUT_BUFFER;
		return true;
	}
	EnsureSize((uint32_t) bioAvailable);
	int32_t written = BIO_read(pBIO, _pBuffer + _published, bioAvailable);
	_published += written;
	SANITY_INPUT_BUFFER;
	return true;
}

void IOBuffer::ReadFromRepeat(uint8_t byte, uint32_t size) {
	SANITY_INPUT_BUFFER;
	EnsureSize(size);
	memset(_pBuffer + _published, byte, size);
	_published += size;
	SANITY_INPUT_BUFFER;
}

bool IOBuffer::WriteToTCPFd(int32_t fd, uint32_t size, int32_t &sentAmount, int &err) {
	SANITY_INPUT_BUFFER;
	bool result = true;
	if (_sendLimit != 0xffffffff) {
		size = size > _sendLimit ? _sendLimit : size;
	}
	if (size == 0)
		return true;
	sentAmount = send(fd, (char *) (_pBuffer + _consumed),
			//_published - _consumed,
			size > _published - _consumed ? _published - _consumed : size,
			MSG_NOSIGNAL);

	if (sentAmount < 0) {
		err = LASTSOCKETERROR;
		if ((err != SOCKERROR_EAGAIN)&&(err != SOCKERROR_EINPROGRESS)) {
			FATAL("Unable to send %"PRIu32" bytes of data data. Size advertised by network layer was %"PRIu32". Permanent error (%d): %s",
					_published - _consumed, size, err, strerror(err));
			result = false;
		}
	} else {
		_consumed += sentAmount;
		if (_sendLimit != 0xffffffff)
			_sendLimit -= sentAmount;
	}
	if (result)
		Recycle();
	SANITY_INPUT_BUFFER;

	return result;
}

bool IOBuffer::WriteToStdio(int32_t fd, uint32_t size, int32_t &sentAmount) {
	SANITY_INPUT_BUFFER;
	bool result = true;
	sentAmount = WRITE_FD(fd, (char *) (_pBuffer + _consumed),
			_published - _consumed);
	//size > _published - _consumed ? _published - _consumed : size,
	int err = errno;

	if (sentAmount < 0) {
		FATAL("Unable to send %"PRIu32" bytes of data data. Size advertised by network layer was %"PRIu32". Permanent error: (%d) %s",
				_published - _consumed, size, err, strerror(err));
		result = false;
	} else {
		_consumed += sentAmount;
	}
	if (result)
		Recycle();
	SANITY_INPUT_BUFFER;

	return result;
}

uint32_t IOBuffer::GetMinChunkSize() {
	return _minChunkSize;
}

void IOBuffer::SetMinChunkSize(uint32_t minChunkSize) {
	o_assert(minChunkSize > 0 && minChunkSize < 16 * 1024 * 1024);
	_minChunkSize = minChunkSize;
}

uint32_t IOBuffer::GetCurrentWritePosition() {

	return _published;
}

uint8_t *IOBuffer::GetPointer() {

	return _pBuffer;
}

bool IOBuffer::Ignore(uint32_t size) {
	SANITY_INPUT_BUFFER;
	_consumed += size;
	if (_sendLimit != 0xffffffff)
		_sendLimit -= size;
	Recycle();
	SANITY_INPUT_BUFFER;

	return true;
}

bool IOBuffer::IgnoreAll() {
	SANITY_INPUT_BUFFER;
	_consumed = _published;
	_sendLimit = 0xffffffff;
	Recycle();
	SANITY_INPUT_BUFFER;

	return true;
}

bool IOBuffer::MoveData() {
	SANITY_INPUT_BUFFER;
	if (_published - _consumed <= _consumed) {
		memcpy(_pBuffer, _pBuffer + _consumed, _published - _consumed);
		TRACK_IOBUFFER_MEMCPY(_published - _consumed, this);
		_published = _published - _consumed;
		_consumed = 0;
	}
	SANITY_INPUT_BUFFER;

	return true;
}

#define OUTSTANDING (_published - _consumed)
#define AVAILABLE (_size - _published)
#define TOTAL_AVAILABLE (AVAILABLE+_consumed)
#define NEEDED (OUTSTANDING + expected)

bool IOBuffer::EnsureSize(uint32_t expected) {
	SANITY_INPUT_BUFFER;
	//1. Is the trailing free space enough?
	if (AVAILABLE >= expected) {
		SANITY_INPUT_BUFFER;
		return true;
	}

	//2. Is the total free space enough?
	if (TOTAL_AVAILABLE >= expected) {

		//3. Yes, move data to consolidate free space
		MoveData();

		//4. Is the consolidated free space enough this time? (should be)
		if (AVAILABLE >= expected) {
			SANITY_INPUT_BUFFER;
			return true;
		}
	}

	//5. Nope! So, let's get busy with making a brand new buffer...
	//We are going to make the buffer at least 1.3*_size and make sure
	//is not lower than _minChunkSize
	if (NEEDED < (_size * 1.3)) {
		expected = (uint32_t) (_size * 1.3) - OUTSTANDING;
	}
	if (NEEDED < _minChunkSize) {
		expected = _minChunkSize - OUTSTANDING;
	}

	//6. Allocate the required memory
	uint8_t *pTempBuffer = new uint8_t[NEEDED];
	TRACK_NEW(NEEDED, this);

	//7. Copy existing data if necessary and switch buffers
	if (_pBuffer != NULL) {
		memcpy(pTempBuffer, _pBuffer + _consumed, OUTSTANDING);
		TRACK_IOBUFFER_MEMCPY(OUTSTANDING, this);
		delete[] _pBuffer;
		TRACK_DELETE(_size, this);
	}
	_pBuffer = pTempBuffer;

	//8. Update the size & indices
	_size = NEEDED;
	_published = OUTSTANDING;
	_consumed = 0;
	SANITY_INPUT_BUFFER;

	return true;
}

string IOBuffer::DumpBuffer(const uint8_t *pBuffer, uint32_t length) {
	IOBuffer temp;
	temp.ReadFromBuffer(pBuffer, length);
	return temp.ToString();
}

string IOBuffer::DumpBuffer(MSGHDR &message, uint32_t limit) {
	IOBuffer temp;
	for (MSGHDR_MSG_IOVLEN_TYPE i = 0; i < message.MSGHDR_MSG_IOVLEN; i++) {
		temp.ReadFromBuffer((uint8_t *) message.MSGHDR_MSG_IOV[i].IOVEC_IOV_BASE,
				message.MSGHDR_MSG_IOV[i].IOVEC_IOV_LEN);
	}
	return temp.ToString(0, limit);
}

string IOBuffer::ToString(uint32_t startIndex, uint32_t limit) {
	SANITY_INPUT_BUFFER;
	string allowedCharacters = " 1234567890-=qwertyuiop[]asdfghjkl;'\\`zxcvbnm";
	allowedCharacters += ",./!@#$%^&*()_+QWERTYUIOP{}ASDFGHJKL:\"|~ZXCVBNM<>?";
	stringstream ss;
	ss << "Size: " << _size << endl;
	ss << "Published: " << _published << endl;
	ss << "Consumed: " << _consumed << endl;
	if (_sendLimit == 0xffffffff)
		ss << "Send limit: unlimited" << endl;
	else
		ss << "Send limit: " << _sendLimit << endl;
	ss << format("Address: %p", _pBuffer) << endl;
	if (limit != 0) {
		ss << format("Limited to %u bytes", limit) << endl;
	}
	string address = "";
	string part1 = "";
	string part2 = "";
	string hr = "";
	limit = (limit == 0) ? _published : limit;
	for (uint32_t i = startIndex; i < limit; i++) {
		if (((i % 16) == 0) && (i > 0)) {
			ss << address << "  " << part1 << " " << part2 << " " << hr << endl;
			part1 = "";
			part2 = "";
			hr = "";
		}
		address = format("%08u", i - (i % 16));

		if ((i % 16) < 8) {
			part1 += format("%02hhx", _pBuffer[i]);
			part1 += " ";
		} else {
			part2 += format("%02hhx", _pBuffer[i]);
			part2 += " ";
		}

		if (allowedCharacters.find(_pBuffer[i], 0) != string::npos)
			hr += _pBuffer[i];
		else
			hr += '.';
	}

	if (part1 != "") {
		part1 += string(24 - part1.size(), ' ');
		part2 += string(24 - part2.size(), ' ');
		hr += string(16 - hr.size(), ' ');
		ss << address << "  " << part1 << " " << part2 << " " << hr << endl;
	}
	SANITY_INPUT_BUFFER;

	return ss.str();
}

IOBuffer::operator string() {

	return ToString(0, 0);
}

void IOBuffer::ReleaseDoublePointer(char ***pppPointer) {
	char **ppPointer = *pppPointer;
	if (ppPointer == NULL)
		return;
	for (uint32_t i = 0;; i++) {
		if (ppPointer[i] == NULL)
			break;
		delete[] ppPointer[i];
		ppPointer[i] = NULL;
	}
	delete[] ppPointer;
	ppPointer = NULL;
	*pppPointer = NULL;
}

void IOBuffer::Cleanup() {
	if (_pBuffer != NULL) {
		delete[] _pBuffer;
		TRACK_DELETE(_size, this);
		_pBuffer = NULL;
	}
	_size = 0;
	_published = 0;
	_consumed = 0;
}

void IOBuffer::Recycle() {
	if (_consumed != _published)
		return;
	SANITY_INPUT_BUFFER;
	_consumed = 0;
	_published = 0;
	SANITY_INPUT_BUFFER;
}

