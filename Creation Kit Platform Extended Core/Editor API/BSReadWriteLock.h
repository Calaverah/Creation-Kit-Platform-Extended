// Special thanks to Nukem: https://github.com/Nukem9/SkyrimSETest/blob/master/skyrim64_test/src/patches/TES/BSReadWriteLock.h

#pragma once

namespace CreationKitPlatformExtended
{
	namespace EditorAPI
	{
		class BSReadWriteLock
		{
		private:
			//
			// NOTE: In order to fit into 8 bytes, m_Bits is declared as int16. This means
			// a recursive read lock acquired more than 32,767 times is undefined behavior.
			//
			std::atomic<uint32_t> m_ThreadId = 0;	// We don't really care what other threads see
			std::atomic<int16_t> m_Bits = 0;		// Must be globally visible
			volatile int8_t m_WriteCount = 0;

			enum : int32_t
			{
				READER = 2,
				WRITER = 1
			};

		public:
			DECLARE_CONSTRUCTOR_HOOK(BSReadWriteLock);

			BSReadWriteLock() = default;
			~BSReadWriteLock();

			void LockForRead();
			void UnlockRead();
			bool TryLockForRead();

			void LockForWrite();
			void UnlockWrite();
			bool TryLockForWrite();

			void LockForReadAndWrite() const;
			bool IsWritingThread() const;
		};
		static_assert(sizeof(BSReadWriteLock) <= 0x8, "Lock must fit inside the original game structure");

		class BSAutoReadAndWriteLock
		{
		private:
			BSReadWriteLock* m_Lock;

		public:
			BSAutoReadAndWriteLock() = delete;

			// Constructor hook
			BSAutoReadAndWriteLock* Initialize(BSReadWriteLock* Child);

			// Destructor hook
			void Deinitialize() const;
		};
		static_assert(sizeof(BSAutoReadAndWriteLock) <= 0x8, "Lock must fit inside the original game structure");
	}
}