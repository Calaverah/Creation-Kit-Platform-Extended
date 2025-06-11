// Copyright � 2025 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: https://www.gnu.org/licenses/lgpl-3.0.html

#include <vector>
#include <unordered_map>
#include <windows.h>
#include <CKPE.Starfield.VersionLists.h>

namespace CKPE
{
	namespace Starfield
	{
		VersionLists::EDITOR_EXECUTABLE_TYPE _seditor_ver{ VersionLists::EDITOR_UNKNOWN };

		// ������ ����������� ����������� ������, ���������� � �������
		static std::unordered_map<uint32_t, VersionLists::EDITOR_EXECUTABLE_TYPE> _sallowedEditorVersion =
		{
			//{ 0x6CDE4424ul, VersionLists::EDITOR_STARFIELD_1_13_61_0		},	// Redirect Steam
			//{ 0x8777A522ul, VersionLists::EDITOR_STARFIELD_1_14_70_0		},	// Redirect Steam
			//{ 0x01BF6FB3ul, VersionLists::EDITOR_STARFIELD_1_14_74_0		},	// Redirect Steam
			{ 0x8C475320ul, VersionLists::EDITOR_STARFIELD_1_14_78_0		},	// Redirect Steam
			{ 0x8C475320ul, VersionLists::EDITOR_STARFIELD_1_15_216_0		},	// Redirect Steam
		};

		// ������ ���������� ������ ����������
		static std::vector<VersionLists::EDITOR_EXECUTABLE_TYPE> _soutdatedEditorVersion =
		{
			VersionLists::EDITOR_STARFIELD_1_13_61_0,
			VersionLists::EDITOR_STARFIELD_1_14_70_0,
			VersionLists::EDITOR_STARFIELD_1_14_74_0,
		};

		// ������ �������� �������� � ����������� ������, ���������� � ������� (�� �� �����)
		static std::unordered_map<uint32_t,
			std::pair<std::string_view, VersionLists::EDITOR_EXECUTABLE_TYPE>> _sallowedEditorVersion2 =
		{
			//{ 0x86DD768ul, { "1.13.61.0",	VersionLists::EDITOR_STARFIELD_1_13_61_0		} },
			//{ 0x873D2B8ul, { "1.14.70.0",	VersionLists::EDITOR_STARFIELD_1_14_70_0		} },
			//{ 0x875F450ul, { "1.14.74.0",	VersionLists::EDITOR_STARFIELD_1_14_74_0		} },
			{ 0x875F550ul, { "1.14.78.0",	VersionLists::EDITOR_STARFIELD_1_14_78_0		} },
			{ 0x84D9B40ul, { "1.15.216.0",	VersionLists::EDITOR_STARFIELD_1_15_216_0		} },
		};

		// ������ �������� ����������
		static std::vector<std::wstring_view> _sEditorVersionStr =
		{
			L"Unknown version",
			L"Starfield [v1.13.61.0]",
			L"Starfield [v1.14.70.0]",
			L"Starfield [v1.14.74.0]",
			L"Starfield [v1.14.78.0]",
			L"Starfield [v1.15.216.0]",
		};

		// ������ ��� ������ ���� ������
		static std::unordered_map<VersionLists::EDITOR_EXECUTABLE_TYPE, std::wstring_view> _sallowedDatabaseVersion =
		{
			//{ VersionLists::EDITOR_STARFIELD_1_13_61_0,	L"CreationKitPlatformExtended_SF_1_13_61_0.database"	},
			//{ VersionLists::EDITOR_STARFIELD_1_14_70_0,	L"CreationKitPlatformExtended_SF_1_14_70_0.database"	},
			//{ VersionLists::EDITOR_STARFIELD_1_14_74_0,	L"CreationKitPlatformExtended_SF_1_14_74_0.database"	},
			{ VersionLists::EDITOR_STARFIELD_1_14_78_0,		L"CreationKitPlatformExtended_SF_1_14_78_0.database"	},
			{ VersionLists::EDITOR_STARFIELD_1_15_216_0,	L"CreationKitPlatformExtended_SF_1_15_216_0.database"	},
		};

		static constexpr auto QT_RESOURCE = L"CreationKitPlatformExtended_SF_QResources.pak";

		void VersionLists::Verify()
		{
			for (auto editorVersionIterator2 = _sallowedEditorVersion2.begin();
				editorVersionIterator2 != _sallowedEditorVersion2.end();
				editorVersionIterator2++)
			{
				// ������ � ������ ������ �� ������� ��� ��������
				__try
				{
					// ��������� �� ���������� �������� ������ ������
					if (!_stricmp((const char*)((std::uintptr_t)GetModuleHandleA(nullptr) + editorVersionIterator2->first),
						editorVersionIterator2->second.first.data()))
					{
						_seditor_ver = editorVersionIterator2->second.second;
						break;
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{}
			}
		}

		bool VersionLists::HasAllowedEditorVersion() noexcept(true)
		{
			if (HasOutdatedEditorVersion())
				return false;

			return _seditor_ver != VersionLists::EDITOR_UNKNOWN;
		}

		bool VersionLists::HasOutdatedEditorVersion() noexcept(true)
		{
			return std::find(_soutdatedEditorVersion.begin(), _soutdatedEditorVersion.end(), _seditor_ver)
				!= _soutdatedEditorVersion.end();
		}

		std::wstring VersionLists::GetGameName() noexcept(true)
		{
			return L"SF";
		}

		std::wstring VersionLists::GetDatabaseFileName() noexcept(true)
		{
			auto it = _sallowedDatabaseVersion.find(_seditor_ver);
			return (it != _sallowedDatabaseVersion.end()) ? it->second.data() : L"";
		}

		std::wstring VersionLists::GetEditorVersionByString() noexcept(true)
		{
			return (_sEditorVersionStr.size() > _seditor_ver) ? _sEditorVersionStr[_seditor_ver].data() : _sEditorVersionStr[0].data();
		}

		std::wstring VersionLists::GetExternalResourcePackageFileName() noexcept(true)
		{
			return QT_RESOURCE;
		}
	}
}