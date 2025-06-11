// Copyright � 2025 aka perchik71. All rights reserved.
// Contacts: <email:timencevaleksej@gmail.com>
// License: https://www.gnu.org/licenses/lgpl-3.0.html

#pragma once

namespace CKPE
{
	namespace Fallout4
	{
		class Runner
		{
			Runner(const Runner&) = delete;
			Runner& operator=(const Runner&) = delete;

			[[nodiscard]] bool InstallPatches() noexcept(true);
		public:
			constexpr Runner() noexcept(true) = default;

			[[nodiscard]] static Runner* GetSingleton() noexcept(true);
			[[nodiscard]] bool Install() noexcept(true);
		};
	}
}