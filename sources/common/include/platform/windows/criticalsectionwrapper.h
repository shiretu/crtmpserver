
#pragma once

#ifdef WIN32

class CSW {
private:
	CRITICAL_SECTION _criticalSection;
public:

	inline CSW() {
		InitializeCriticalSection(&_criticalSection);
	}

	inline ~CSW() {
		DeleteCriticalSection(&_criticalSection);
	}

	inline int EnterCriticalSection() {
		::EnterCriticalSection(&_criticalSection);
		return 0;
	}

	inline int TryEnterCriticalSection() {
		//this function should return 0 for success, <0 for failure
		return ::TryEnterCriticalSection(&_criticalSection) != 0 ? 0 : -1;
	}

	inline int LeaveCriticalSection() {
		::LeaveCriticalSection(&_criticalSection);
		return 0;
	}

	inline PCRITICAL_SECTION GetCS(){
		return &_criticalSection;
	}
};
#endif /* WIN32 */
