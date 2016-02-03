#pragma once
#ifndef blue_BlueSysInfo_h
#define blue_BlueSysInfo_h


BLUE_CLASS( BlueSysInfoCpu ): public IRoot
{
public:
    EXPOSE_TO_BLUE();

	BlueSysInfoCpu();
    
	uint32_t m_family;
	uint32_t m_revision;
	uint32_t m_logicalCpuCount;
	uint32_t m_bitCount;
	std::string m_brand;
	std::string m_identifier;
};

TYPEDEF_BLUECLASS( BlueSysInfoCpu );


BLUE_CLASS( BlueSysInfoTaskTimes ): public IRoot
{
public:
    EXPOSE_TO_BLUE();
    
    double m_userTime;
    double m_systemTime;
};

TYPEDEF_BLUECLASS( BlueSysInfoTaskTimes );


BLUE_CLASS( BlueSysInfoOs ): public IRoot
{
public:
    EXPOSE_TO_BLUE();

	BlueSysInfoOs();

	enum Platform
	{
		WINDOWS,
		OSX,
	};

	enum Suite
	{
		DESKTOP,
		WORKSTATION,
		SERVER,
	};
    
	Platform m_platform;
	int32_t m_majorVersion;
	int32_t m_minorVersion;
	int32_t m_buildNumber;
	std::string m_patch;
	Suite m_suite;
};

TYPEDEF_BLUECLASS( BlueSysInfoOs );


BLUE_CLASS( BlueSysInfoMemory ): public IRoot
{
public:
    EXPOSE_TO_BLUE();

	BlueSysInfoMemory();

	uint64_t m_workingSet;
	uint64_t m_pageFile;

	uint64_t m_totalPhysical;
	uint64_t m_availablePhysical;
};

TYPEDEF_BLUECLASS( BlueSysInfoMemory );



BLUE_CLASS( BlueSysInfo ): public IRoot
{
public:
    EXPOSE_TO_BLUE();
    
    std::wstring GetUserDocumentsDirectory() const;
    std::wstring GetSharedApplicationDataDirectory() const;
    std::wstring GetUserApplicationDataDirectory() const;
	std::wstring GetSharedFontsDirectory() const;
    uint32_t GetProcessBitCount() const;
    uint32_t GetSystemBitCount() const;
    
    BlueSysInfoTaskTimesPtr GetProcessTimes() const;
    BlueSysInfoTaskTimesPtr GetThreadTimes() const;
	BlueSysInfoMemoryPtr GetMemory() const;
    uint64_t GetProcessStartTime() const;

	bool IsTransgaming() const;
	bool IsWine() const;
	std::wstring GetWineHostOs() const;

	std::string GetMachineUuid() const;
private:
    CBlueSysInfoCpu m_cpu;
	CBlueSysInfoOs m_os;
};

TYPEDEF_BLUECLASS( BlueSysInfo );

#endif
