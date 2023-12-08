// Copyright � 2023-2024 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: https://www.gnu.org/licenses/gpl-3.0.html

#include "Engine.h"

namespace CreationKitPlatformExtended
{
	namespace Core
	{
		constexpr static auto FILE_NONE = L"none";
		constexpr static auto UI_LOG_CMD_ADDTEXT = 0x23000;
		constexpr static auto UI_LOG_CMD_CLEARTEXT = 0x23001;
		constexpr static auto UI_LOG_CMD_AUTOSCROLL = 0x23002;

		ConsoleWindow* GlobalConsoleWindowPtr = nullptr;

		ConsoleWindow::ConsoleWindow(Engine* lpEngine) : _engine(nullptr), hWindow(NULL),
			_richEditHwnd(NULL), _autoScroll(true), _outputFileHandle(nullptr)
		{
			Create();
		}
	
		ConsoleWindow::~ConsoleWindow()
		{
			Destroy();
		}

		LRESULT CALLBACK ConsoleWindow::WndProc(HWND Hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
		{
			if (WM_NCCREATE == Message)
			{
				auto info = reinterpret_cast<const CREATESTRUCT*>(lParam);
				SetWindowLongPtr(Hwnd, GWLP_USERDATA, (LONG_PTR)(info->lpCreateParams));
			}
			else
			{
				auto moduleConsole = (ConsoleWindow*)GetWindowLongPtr(Hwnd, GWLP_USERDATA);

				switch (Message)
				{
				case WM_CREATE:
				{
					auto info = reinterpret_cast<const CREATESTRUCT*>(lParam);

					// �������� rich edit control (https://docs.microsoft.com/en-us/windows/desktop/Controls/rich-edit-controls)
					uint32_t style = WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_LEFT | ES_NOHIDESEL |
						ES_AUTOVSCROLL | ES_READONLY;

					HWND richEditHwnd = CreateWindowExW(0, MSFTEDIT_CLASS, L"", style, 0, 0, info->cx, info->cy, Hwnd,
						NULL, info->hInstance, NULL);
					if (!(richEditHwnd))
						return -1;

					moduleConsole->SetRichEditHandle(richEditHwnd);
					moduleConsole->SetAutoScroll(true);

					// ���������� ������ ����� � ������������� ����� � ������ (1 ���� = 20 ������)
					CHARFORMAT2A format = { 0 };
					format.cbSize = sizeof(format);
					format.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT;
					format.yHeight = _READ_OPTION_INT("Log", "nFontSize", 10) * 20;
					format.wWeight = (WORD)_READ_OPTION_UINT("Log", "uFontWeight", FW_NORMAL);
					strncpy_s(format.szFaceName, _READ_OPTION_STR("Log", "sFont", "Consolas").c_str(), _TRUNCATE);

					SendMessageA(richEditHwnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));

					//����������� �� EN_MSGFILTER � EN_SELCHANGE
					SendMessageA(richEditHwnd, EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_SELCHANGE);

					if (_READ_OPTION_BOOL("Log", "bShowWidow", true))
					{
						// ���������� ��������� ���� �� ���������
						int winX = _READ_OPTION_INT("Log", "nX", info->x);
						int winY = _READ_OPTION_INT("Log", "nY", info->y);
						int winW = _READ_OPTION_INT("Log", "nWidth", info->cx);
						int winH = _READ_OPTION_INT("Log", "nHeight", info->cy);

						MoveWindow(Hwnd, winX, winY, winW, winH, FALSE);

						if (winW != 0 && winH != 0)
							ShowWindow(Hwnd, SW_SHOW);
					}
				}
				return 0;

				case WM_DESTROY:
				{
					if (DestroyWindow(moduleConsole->GetRichEditHandle()))
						moduleConsole->SetRichEditHandle(NULL);
				}
				return 0;

				case WM_SIZE:
				{
					int w = LOWORD(lParam);
					int h = HIWORD(lParam);
					MoveWindow(moduleConsole->GetRichEditHandle(), 0, 0, w, h, TRUE);
				}
				break;

				case WM_ACTIVATE:
				{
					if (wParam != WA_INACTIVE)
						SetFocus(moduleConsole->GetRichEditHandle());
				}
				return 0;

				case WM_CLOSE:
				{
					ShowWindow(Hwnd, SW_HIDE);
				}
				return 0;

				case WM_NOTIFY:
				{
					static uint64_t lastClickTime;
					auto notification = reinterpret_cast<const LPNMHDR>(lParam);

					if (notification->code == EN_MSGFILTER)
					{
						auto msgFilter = reinterpret_cast<const MSGFILTER*>(notification);

						if (msgFilter->msg == WM_LBUTTONDBLCLK)
							lastClickTime = GetTickCount64();
					}
					else if (notification->code == EN_SELCHANGE)
					{
						auto selChange = reinterpret_cast<const SELCHANGE*>(notification);
						// ������� ������ ���� � ���������� ������� -> ����������� ���������������� ������������� �����
						if ((GetTickCount64() - lastClickTime > 1000) || selChange->seltyp == SEL_EMPTY)
							break;

						char lineData[2048] = { 0 };
						*reinterpret_cast<uint16_t*>(&lineData[0]) = ARRAYSIZE(lineData);

						// �������� ����� ������ � ����� �� ���������� ���������
						LRESULT lineIndex = SendMessageA(moduleConsole->GetRichEditHandle(), EM_LINEFROMCHAR,
							selChange->chrg.cpMin, 0);
						LRESULT charCount = SendMessageA(moduleConsole->GetRichEditHandle(), EM_GETLINE, lineIndex,
							reinterpret_cast<LPARAM>(&lineData));

						if (charCount > 0)
						{
							lineData[charCount - 1] = '\0';

							// ��������� ����������������� ������������� ����� � ������� "(XXXXXXXX)"
							for (char* p = lineData; p[0] != '\0'; p++)
							{
								if (p[0] == '(' && strlen(p) >= 10 && p[9] == ')')
								{
									uint32_t id = strtoul(&p[1], nullptr, 16);


									// �������
									//PostMessageA(MainWindow::GetWindow(), WM_COMMAND, UI_EDITOR_OPENFORMBYID, id);
								}
							}
						}

						lastClickTime = GetTickCount64() + 1000;
					}
				}
				break;

				case WM_TIMER:
				{
					if (wParam != UI_LOG_CMD_ADDTEXT)
						break;

					if (moduleConsole->GetPendingMessages().size() <= 0)
						break;

					return WndProc(Hwnd, UI_LOG_CMD_ADDTEXT, 0, 0);
				}
				return 0;

				case UI_LOG_CMD_ADDTEXT:
				{
					auto rich = moduleConsole->GetRichEditHandle();
					SendMessageA(rich, WM_SETREDRAW, FALSE, 0);

					// ��������� ������ �������, ���� ��� ��������������
					POINT scrollRange = { 0 };

					if (!moduleConsole->HasAutoScroll())
						SendMessageA(rich, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scrollRange));

					// �������� ����� ���� ��������� � �������� ������� ������
					auto messages(std::move(moduleConsole->GetPendingMessages()));

					for (const char* message : messages)
					{
						// ����������� ������ � �����, ����� ��������
						CHARRANGE range
						{
							.cpMin = LONG_MAX,
							.cpMax = LONG_MAX,
						};

						SendMessageA(rich, EM_EXSETSEL, 0, reinterpret_cast<LPARAM>(&range));
						SendMessageA(rich, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(message));

						voltek::scalable_free(message);
					}

					if (!moduleConsole->HasAutoScroll())
						SendMessageA(rich, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scrollRange));

					SendMessageA(rich, WM_SETREDRAW, TRUE, 0);
					RedrawWindow(rich, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_NOCHILDREN);
				}
				return 0;

				case UI_LOG_CMD_CLEARTEXT:
				{
					moduleConsole->Clear();
				}
				return 0;

				case UI_LOG_CMD_AUTOSCROLL:
				{
					moduleConsole->SetAutoScroll(static_cast<bool>(wParam));
				}
				return 0;
				}
			}

			return DefWindowProc(Hwnd, Message, wParam, lParam);
		}

		bool ConsoleWindow::SaveRichTextToFile(const char* _filename) const
		{
			// ������� ������
			// https://subscribe.ru/archive/comp.soft.prog.qandacpp/200507/10000511.html

			// ��������\������ ����, ��������� ������� ��� ����������
			// ���� ���� ����������
			FILE* fileStream = _fsopen(_filename, "wt", _SH_DENYRW);
			if (!fileStream)
			{
				_ERROR("I can't open the file for writing: \"%s\"", _filename);
				return false;
			}

			CreationKitPlatformExtended::Utils::ScopeFileStream file(fileStream);

			// ������� ��������� ������ ��� ������ � ����
			auto MyOutFunction = [](
				DWORD_PTR dwCookie, // �� ����� ���������������� �������� �������
									// �� ������� � EDITSTREAM::dwCookie
				LPBYTE pbBuff,		// ����� � ������� ������� �������� RichEdit
				LONG cb,			// ������ ������ � ������
				LONG* pcb			// ��������� �� ���������� � ������� �������
									// �������� ������� ������� MyOutFunction
									// ������� ���������� ������ �� ������ pbBuff) -> DWORD 
			) -> DWORD {
					// � �������� dwCookie �������� ��������� ������� ��
					// ���������� � EDITSTREAM
					FILE* stream = reinterpret_cast<FILE*>(dwCookie);
					// ���������� ���������� ����� � ����, ������ ����������� ���
					// ������ � ���������� cb
					fwrite(pbBuff, 1, cb, stream);
					// ������� RichEdit� ������� �� ���������� ������
					*pcb = cb;
					// ���������� ���� (� ��� �� ��)
					return 0;
			};
			
			EDITSTREAM es = { 0 };
			// ��������� ������� ��������� ������
			es.pfnCallback = static_cast<EDITSTREAMCALLBACK>(MyOutFunction);
			// ���������� ������
			es.dwError = 0;
			// � �������� Cookie �������� ��������� �� ��� ������ file
			es.dwCookie = (DWORD_PTR)&fileStream;
			// �������� ��������� ���� RichEdit, WPARAM==�����,
			// LPARAM - ��������� �� EDITSTREAM
			SendMessage(_richEditHwnd, EM_STREAMOUT,
				SF_TEXT /*�������� ������� �����*/,
				(LPARAM)&es);

			// true - ���� �� ���� ������, ����� ���-�� ���-�� �����
			// ����� �� � �������.
			return !es.dwError;
		}

		void ConsoleWindow::CloseOutputFile()
		{
			if (_outputFileHandle)
			{
				fclose(_outputFileHandle);
				_outputFileHandle = nullptr;
			}
		}

		void ConsoleWindow::UpdateWindow() const
		{
			if (hWindow)
				::UpdateWindow(hWindow);
		}

		void ConsoleWindow::BringToFront() const
		{
			if (hWindow)
			{
				ShowWindow(hWindow, SW_SHOW);
				SetForegroundWindow(hWindow);
			}
		}

		void ConsoleWindow::Hide() const
		{
			if (hWindow)
				ShowWindow(hWindow, SW_HIDE);
		}

		bool ConsoleWindow::Create()
		{
			if (hWindow)
				return false;

			// �������� ������ RichEdit
			if (!LoadLibraryA("MSFTEDIT.dll"))
				return false;

			auto fName = _READ_OPTION_USTR("Log", "sOutputFile", FILE_NONE);
			if (fName != FILE_NONE)
			{
				_outputFileHandle = _wfsopen(fName.c_str(), L"wt", _SH_DENYRW);
				if (!_outputFileHandle)
					_ERROR(L"Unable to open the log file '%s' for writing. To disable, "
						"set the 'OutputFile' INI option to 'none'.", fName.c_str());
			}

			// � ��������� ������ ������ ���� � ��� �� ��� ���� ��������� ������� ���������
			// ���������, ��� �� ����� ������������� ���� (��� ��� ���), ���� ���� ����� ������
			// � �� ������������� ������ � ����� Creation Kit, � �������.

			std::thread asyncLogThread([](HWND* window, ConsoleWindow* module)
				{
					// ���� ������
					auto instance = static_cast<HINSTANCE>(GetModuleHandle(NULL));

					WNDCLASSEXA wc
					{
						.cbSize = sizeof(WNDCLASSEX),
						.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
						.lpfnWndProc = WndProc,
						.hInstance = instance,
						.hIcon = LoadIconA(instance, MAKEINTRESOURCE(0x13E)),				// 0x13E ������ ������ Creation Kit
						.hCursor = LoadCursor(NULL, IDC_ARROW),
						.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)),
						.lpszClassName = "RTEDITLOG",
						.hIconSm = wc.hIcon,
					};

					if (!RegisterClassExA(&wc))
						return false;

					*window = CreateWindowExA(0, "RTEDITLOG", "Console Window", WS_OVERLAPPEDWINDOW,
						64, 64, 1024, 480, NULL, NULL, instance, module);

					if (!(*window))
						return false;

					// ����������� ������ 100 �� �� ������� ����� �����
					SetTimer(*window, UI_LOG_CMD_ADDTEXT, 100, NULL);
					::UpdateWindow(*window);

					MSG msg;
					while (GetMessageA(&msg, NULL, 0, 0) > 0)
					{
						TranslateMessage(&msg);
						DispatchMessageA(&msg);
					}

					return true;
				}, &hWindow, this);

			asyncLogThread.detach();

			return true;
		}

		void ConsoleWindow::Destroy()
		{
			if (hWindow)
				DestroyWindow(hWindow);
		}

		void ConsoleWindow::InputLog(const char* Format, ...)
		{
			va_list va;
			va_start(va, Format);
			InputLogVa(Format, va);
			va_end(va);
		}

		void ConsoleWindow::InputLogVa(const char* Format, va_list Va)
		{
			char buffer[2048];
			int len = _vsnprintf_s(buffer, _TRUNCATE, Format, Va);

			if (len <= 0)
				return;

			//if (MessageBlacklist.count(XUtil::MurmurHash64A(buffer, len)))
			//	return;

			auto line = Utils::Trim(buffer);
			std::replace_if(line.begin(), line.end(), [](auto const& x) { return x == '\n' || x == '\r'; }, ' ');

			if (!line.length())
				return;

			line += "\n";

			if (_outputFileHandle)
			{
				fputs(line.c_str(), _outputFileHandle);
				fflush(_outputFileHandle);
			}

			if (_pendingMessages.size() < 50000)
				_pendingMessages.push_back(Utils::StrDub(line.c_str()));
		}
	}

	void _CONSOLE(const char* fmt, ...)
	{
		va_list va;
		va_start(va, fmt);
		_CONSOLE(fmt, va);
		va_end(va);
	}

	void _CONSOLE(const char* fmt, va_list va)
	{
		if (Core::GlobalConsoleWindowPtr)
			Core::GlobalConsoleWindowPtr->InputLogVa(fmt, va);
	}
}