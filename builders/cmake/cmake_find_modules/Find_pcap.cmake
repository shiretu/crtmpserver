MESSAGE(STATUS "Looking for pcap")
FIND_PATH(PCAP_INCLUDE_PATH
	NAMES
		pcap/pcap.h
	PATHS
		${TOOLCHAIN_HEADER_PATH}
		/opt/local/include
		/sw/include
		/usr/include
		/usr/local/include
		/usr/local/ssl/include
		NO_DEFAULT_PATH)

FIND_LIBRARY(PCAP_LIBRARY_PATH
	NAMES
		pcap
	PATHS
		${TOOLCHAIN_LIBRARY_PATH}
		/opt/local/lib64
		/opt/local/lib
		/sw/lib64
		/sw/lib
		/lib64
		/usr/lib64
		/usr/local/lib64
		/lib/x86_64-linux-gnu
		/usr/lib/x86_64-linux-gnu
		/opt/local/lib64
		/lib
		/usr/lib
		/usr/local/lib
		/lib/i386-linux-gnu
		/usr/lib/i386-linux-gnu
		/usr/local/ssl/lib
		/opt/local/lib64/pcap
		/opt/local/lib/pcap
		/sw/lib64/pcap
		/sw/lib/pcap
		/lib64/pcap
		/usr/lib64/pcap
		/usr/local/lib64/pcap
		/lib/x86_64-linux-gnu/pcap
		/usr/lib/x86_64-linux-gnu/pcap
		/opt/local/lib64/pcap
		/lib/pcap
		/usr/lib/pcap
		/usr/local/lib/pcap
		/lib/i386-linux-gnu/pcap
		/usr/lib/i386-linux-gnu/pcap
		/usr/local/ssl/lib/pcap
		NO_DEFAULT_PATH)

IF(PCAP_INCLUDE_PATH)
	SET(PCAP_FOUND 1 CACHE STRING "Set to 1 if pcap is found, 0 otherwise")
	MESSAGE(STATUS "Looking for pcap headers - found")
ELSE(PCAP_INCLUDE_PATH)
	SET(PCAP_FOUND 0 CACHE STRING "Set to 1 if pcap is found, 0 otherwise")
	MESSAGE(FATAL_ERROR "Looking for pcap headers - not found")
ENDIF(PCAP_INCLUDE_PATH)

IF(PCAP_LIBRARY_PATH)
	SET(PCAP_FOUND 1 CACHE STRING "Set to 1 if pcap is found, 0 otherwise")
	MESSAGE(STATUS "Looking for pcap library - found")
ELSE(PCAP_LIBRARY_PATH)
	SET(PCAP_FOUND 0 CACHE STRING "Set to 1 if pcap is found, 0 otherwise")
	MESSAGE(FATAL_ERROR "Looking for pcap library - not found")
ENDIF(PCAP_LIBRARY_PATH)

MARK_AS_ADVANCED(PCAP_FOUND)
