// Copyright � 2023-2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: https://www.gnu.org/licenses/gpl-3.0.html

#include "Version/resource_version2.h"
#include "CommandLineParser.h"
#include "RelocationDatabase.h"
#include "Engine.h"

#include "Patches/QuitHandlerPatch.h"

namespace CreationKitPlatformExtended
{
	namespace Core
	{
		Engine* GlobalEnginePtr = nullptr;

		CHAR TempNTSIT[16];
		ULONG_PTR TempNTSITAddress;
		std::atomic_uint32_t g_DumpTargetThreadId;
		LONG(NTAPI* NtSetInformationThread)(HANDLE ThreadHandle, LONG ThreadInformationClass,
			PVOID ThreadInformation, ULONG ThreadInformationLength);
		using VCoreNotifyEvent = void (Engine::*)();
		VCoreNotifyEvent VCoreDisableBreakpoint;
		VCoreNotifyEvent VCoreContinueInitialize;

		//////////////////////////////////////////////

		BOOL WINAPI hk_QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount)
		{
			// ���������� ����� ��������
			(GlobalEnginePtr->*VCoreDisableBreakpoint)();

			// ��������� ��� ���������
			__try
			{
				__debugbreak();
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{}

			(GlobalEnginePtr->*VCoreContinueInitialize)();
			return QueryPerformanceCounter(lpPerformanceCount);
		}

		LONG NTAPI hk_NtSetInformationThread(HANDLE ThreadHandle, LONG ThreadInformationClass,
			PVOID ThreadInformation, ULONG ThreadInformationLength)
		{
			// ��� Steam
			if (ThreadInformationClass == 0x11)
				return 0;

			return NtSetInformationThread(ThreadHandle, ThreadInformationClass,
				ThreadInformation, ThreadInformationLength);
		}

		//////////////////////////////////////////////

		Engine::Engine(HMODULE hModule, EDITOR_EXECUTABLE_TYPE eEditorVersion, uintptr_t nModuleBase) :
			_module(hModule), _moduleBase(nModuleBase), _editorVersion(eEditorVersion), PatchesManager(new ModuleManager())
		{
			GlobalEnginePtr = this;

			int info[4];
			__cpuid(info, 7);
			_hasAVX2 = (info[1] & (1 << 5)) != 0;

			__cpuid(info, 1);
			_hasSSE41 = (info[2] & (1 << 19)) != 0;

			_MESSAGE("The processor supports the SSE 4.1 instruction set: %s", (_hasSSE41 ? "true" : "false"));
			_MESSAGE("The processor supports the AVX 2 instruction set: %s", (_hasAVX2 ? "true" : "false"));

			// �������� ��������� ������� ������, ��� ����������� �������������
			VCoreDisableBreakpoint = &Engine::DisableBreakpoint;
			VCoreContinueInitialize = &Engine::ContinueInitialize;

			GlobalRelocationDatabasePtr = new RelocationDatabase(this);
			GlobalRelocatorPtr = new Relocator(this);

			// ���������� ������
			PatchesManager->Append({
				new CreationKitPlatformExtended::Patches::QuitHandlerPatch(),
			});

			// ��������� ����� ��������, ����� ��������� ����������� DRM ����������
			EnableBreakpoint();
		}

		void Engine::EnableBreakpoint()
		{
			_MESSAGE("Module base: %016X", _moduleBase);

			PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)(_moduleBase +
				((PIMAGE_DOS_HEADER)_moduleBase)->e_lfanew);

			// �������� ������ ������������ ��������, � ������� ���������� ����� security cookie
			auto dataDirectory = ntHeaders->OptionalHeader.DataDirectory;
			auto sectionRVA = dataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress;
			auto sectionSize = dataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size;
			auto loadConfig = (PIMAGE_LOAD_CONFIG_DIRECTORY)(_moduleBase + sectionRVA);

			Assert(sectionRVA > 0 && sectionSize > 0);
			AssertMsg(loadConfig->SecurityCookie, "SecurityCookie is a null pointer!");

			// ���������� ������ � ������� �������/�������� ����

			_moduleSize = ntHeaders->OptionalHeader.SizeOfImage;
			Assert(Utils::GetPESectionRange(_moduleBase, ".text", &Sections[SECTION_TEXT].base, &Sections[SECTION_TEXT].end));
			Assert(Utils::GetPESectionRange(_moduleBase, ".rdata", &Sections[SECTION_DATA_READONLY].base, &Sections[SECTION_DATA_READONLY].end));
			Assert(Utils::GetPESectionRange(_moduleBase, ".data", &Sections[SECTION_DATA].base, &Sections[SECTION_DATA].end));

			uintptr_t tempBssStart, tempBssEnd;
			if (Utils::GetPESectionRange(_moduleBase, ".textbss", &tempBssStart, &tempBssEnd))
			{
				Sections[0].base = std::min(Sections[SECTION_TEXT].base, tempBssStart);
				Sections[0].end = std::max(Sections[SECTION_TEXT].end, tempBssEnd);
			}

			_MESSAGE("Section range \".text\": (base: %016X, end: %016X)", Sections[SECTION_TEXT].base, Sections[SECTION_TEXT].end);
			_MESSAGE("Section range \".rdata\": (base: %016X, end: %016X)", Sections[SECTION_DATA_READONLY].base, Sections[SECTION_DATA_READONLY].end);
			_MESSAGE("Section range \".data\": (base: %016X, end: %016X)", Sections[SECTION_DATA].base, Sections[SECTION_DATA].end);

			// ���������� ���������� ��������, ������� ��������� ������ ����� QueryPerformanceCounter
			*(uint64_t*)loadConfig->SecurityCookie = 0x2B992DDFA232;
			PatchIAT(hk_QueryPerformanceCounter, "kernel32.dll", "QueryPerformanceCounter");

			// ��������� ����� ������������ steam ��� NtSetInformationThread(ThreadHideFromDebugger)
			TempNTSITAddress = (uintptr_t)GetProcAddress(GetModuleHandle("ntdll.dll"), "NtSetInformationThread");
			if (TempNTSITAddress)
			{
				memcpy(&TempNTSIT, (LPVOID)TempNTSITAddress, sizeof(TempNTSIT));
				*(uintptr_t*)&NtSetInformationThread = Detours::X64::DetourFunctionClass(TempNTSITAddress, &hk_NtSetInformationThread);
			}
		}

		void Engine::DisableBreakpoint()
		{
			// ������������ �������� ��������� �� QPC
			PatchIAT(QueryPerformanceCounter, "kernel32.dll", "QueryPerformanceCounter");

			if (TempNTSITAddress)
				// ������������ �������� ��� NtSetInformationThread
				Utils::PatchMemory(TempNTSITAddress, (PBYTE)&TempNTSIT, sizeof(TempNTSIT));
		}

		void Engine::CommandLineRun()
		{
			CommandLineParser CommandLine;

			if (CommandLine.Count() > 0)
			{
				_MESSAGE("\tAccessing the console...");

				// ���������� �������
				auto Command = CommandLine.At(0);
				_MESSAGE("\tCommand: \"%s\"", Command.c_str());

				if (!_stricmp(Command.c_str(), "-PECreateDatabase"))
				{
					// ������ ����
					GlobalRelocationDatabasePtr->CreateDatabase();
					// ��������� Creation Kit
					CreationKitPlatformExtended::Utils::Quit();
				}
				else if (!_stricmp(Command.c_str(), "-PEUpdateDatabase"))
				{
					if (CommandLine.Count() != 3)
					{
						_ERROR("Invalid number of command arguments: %u", CommandLine.Count());
						_MESSAGE("Example: CreationKit -PEUpdateDatabase \"test\" \"test.relb\"");
					}
					else if (!CreationKitPlatformExtended::Utils::FileExists(CommandLine.At(2).c_str()))
					{
						_ERROR("The file does not exist: \"%s\"", CommandLine.At(2).c_str());
					}
					else
					{
						// ��������� ���� ������
						if (GlobalRelocationDatabasePtr->OpenDatabase())
						{
							auto Patch = GlobalRelocationDatabasePtr->GetByName(CommandLine.At(1).c_str());
							// ����� ���?
							if (Patch.Empty())
							{
								Patch = GlobalRelocationDatabasePtr->Append(CommandLine.At(1).c_str(), new RelocationDatabaseItem());
								if (Patch.Empty())
									// ��������� Creation Kit
									CreationKitPlatformExtended::Utils::Quit();
							}
							// ���������
							if (Patch->LoadFromFileDeveloped(CommandLine.At(2).c_str()))
								// ��������� ���� ������
								GlobalRelocationDatabasePtr->SaveDatabase();
						}
						else
							_FATALERROR("The database is not loaded");
					}
					// ��������� Creation Kit
					CreationKitPlatformExtended::Utils::Quit();
				}
				else if (!_stricmp(Command.c_str(), "-PEExtractFromDatabase"))
				{
					if (CommandLine.Count() != 3)
					{
						_ERROR("Invalid number of command arguments: %u", CommandLine.Count());
						_MESSAGE("Example: CreationKit -PEUpdateDatabase \"test\" \"test.relb\"");
					}
					else
					{
						// ��������� ���� ������
						if (GlobalRelocationDatabasePtr->OpenDatabase())
						{
							auto Patch = GlobalRelocationDatabasePtr->GetByName(CommandLine.At(1).c_str());
							// ����� ����?
							if (!Patch.Empty())
								Patch->SaveToFileDeveloped(CommandLine.At(2).c_str());
						}
						else
							_FATALERROR("The database is not loaded");
					}
					// ��������� Creation Kit
					CreationKitPlatformExtended::Utils::Quit();
				}
			}
		}

		void Engine::ContinueInitialize()
		{
			CommandLineRun();
			
			if (!GlobalRelocationDatabasePtr->OpenDatabase())
			{
				_FATALERROR("The database is not loaded, patches are not installed");
				return;
			}
			
			if (!PatchesManager)
			{
				_FATALERROR("The patch manager has not been initialized");
				return;
			}

			// ������� � �������� ���� ������ �� ����������
			PatchesManager->QueryAll();
			// ��������� ��������������� ������
			PatchesManager->EnableAll();
		}

		IResult Engine::Initialize(HMODULE hModule, LPCSTR lpcstrAppName)
		{
			if (GlobalEnginePtr)
				return RC_INITIALIZATION_ENGINE_AGAIN;

			IResult Result = RC_OK;

			// ��������� ����� ��� Creation Kit
			if (CheckFileNameProcess(lpcstrAppName))
			{
				// ������������� ���������� vmm
				voltek::scalable_memory_manager_initialize();
					
				GlobalDebugLogPtr = new DebugLog(L"CreationKitPlatformExtended.log");
				AssertMsg(GlobalDebugLogPtr, "Failed create the log file \"CreationKitPlatformExtended.log\"");
				
				GlobalINIConfigPtr = new INIConfig(L"CreationKitPlatformExtended.ini");
				AssertMsg(GlobalINIConfigPtr, "Failed open the config file \"CreationKitPlatformExtended.ini\"");

				ULONG _osMajorVersion = 0;
				ULONG _osMinorVersion = 0;
				ULONG _osBuildNubmer = 0;

				LONG(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW) = nullptr;
				OSVERSIONINFOEXW osInfo = { 0 };
				*(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
				if (RtlGetVersion)
				{
					osInfo.dwOSVersionInfoSize = sizeof(osInfo);
					RtlGetVersion(&osInfo);

					_osMajorVersion = osInfo.dwMajorVersion;
					_osMinorVersion = osInfo.dwMinorVersion;
					_osBuildNubmer = osInfo.dwBuildNumber;
				}

				_MESSAGE("Creation Kit Platform Extended Runtime: Initialize (Version: %s, OS: %u.%u Build %u)",
					VER_FILE_VERSION_STR, _osMajorVersion, _osMinorVersion, _osBuildNubmer);

				auto LogCurrentTime = []() {
					char timeBuffer[80];
					struct tm* timeInfo;
					time_t rawtime;
					time(&rawtime);
					timeInfo = localtime(&rawtime);
					strftime(timeBuffer, sizeof(timeBuffer), "%A %d %b %Y %r %Z", timeInfo);

					_MESSAGE("Current time: %s", timeBuffer);
				};

				LogCurrentTime();

				// ��������� CRC32 � �����
				uint32_t hash_crc32 = CRC32File((std::string(lpcstrAppName) + ".exe").c_str());
				_MESSAGE("CRC32 executable file: 0x%08X", hash_crc32);

				// ��������� ���������� ������ ������ �������� ��������
				uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);

				// �������� ���������� ����
				EDITOR_EXECUTABLE_TYPE editorVersion = EDITOR_UNKNOWN;
				auto editorVersionIterator = allowedEditorVersion.find(hash_crc32);
				if (editorVersionIterator == allowedEditorVersion.end())
				{
					// ���� �� ������ ����� �����������, ������� ���������� �� �������� ������
					_WARNING("CRC32 does not match any of the known ones, running a version check by signature");

					for (auto editorVersionIterator2 = allowedEditorVersion2.begin();
						editorVersionIterator2 != allowedEditorVersion2.end();
						editorVersionIterator2++)
					{
						// ��������� �� ���������� �������� ������ ������
						if (!_stricmp((const char*)(moduleBase + editorVersionIterator2->first),
							editorVersionIterator2->second.first.data()))
						{
							editorVersion = editorVersionIterator2->second.second;
							break;
						}
					}
				}
				else
					editorVersion = editorVersionIterator->second;

				if (editorVersion != EDITOR_UNKNOWN)
				{
					_MESSAGE("Current CK version: %s", allowedEditorVersionStr[(int)editorVersion].data());

					new Engine(hModule, editorVersion, moduleBase);
				}
				else
				{
					_ERROR("The CK version has not been determined");

					Result = RC_UNSUPPORTED_VERSION_CREATIONKIT;
				}
			}
			else
				Result = RC_UNKNOWN_APPLICATION;

			return Result;
		}
	}
}