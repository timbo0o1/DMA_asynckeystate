#include <array>
#include <vmmdll.h>
#include <iostream>

VMM_HANDLE hVmm = NULL;
DWORD ProcessID = 0;
DWORD csrssPID = 0;
UINT64 gafAsyncKeyState = 0;

//KeyState Variables
std::array<std::uint8_t, 256 * 2 / 8> key_state_bitmap;
std::array<std::uint8_t, 256 / 8> key_state_recent_bitmap;


DWORD GetProcID(LPSTR proc_name)
{
	DWORD procID = 0;
	VMMDLL_PidGetFromName(hVmm, (LPSTR)proc_name, &procID);
	return procID;
}

template <typename T>
T ReadMemory(uintptr_t address, DWORD pid = ProcessID, bool cache = false, const DWORD size = sizeof(T))
{
	T buffer{};
	DWORD bytes_read = 0;
	if (!cache)
		VMMDLL_MemReadEx(hVmm, pid, address, (PBYTE)&buffer, size, &bytes_read, VMMDLL_FLAG_NOCACHE);
	else
		VMMDLL_MemReadEx(hVmm, pid, address, (PBYTE)&buffer, size, &bytes_read, VMMDLL_FLAG_CACHE_RECENT_ONLY);
	return buffer;
}



//KeyState Functions
bool is_key_down(std::uint8_t const vk) {
	return key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2;
}
bool was_key_pressed(std::uint8_t const vk) {
	bool const result = key_state_recent_bitmap[vk / 8] & 1 << vk % 8;
	key_state_recent_bitmap[vk / 8] &= ~(1 << vk % 8);
	return result;
}
void update_key_state_bitmap() {
	if (!csrssPID) {
		return;
	}


	auto prev_key_state_bitmap = key_state_bitmap;

	key_state_bitmap = ReadMemory<std::array<std::uint8_t, 256 * 2 / 8>>(gafAsyncKeyState, csrssPID);


	for (auto vk = 0u; vk < 256; ++vk) {
		if ((key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2) &&
			!(prev_key_state_bitmap[(vk * 2 / 8)] & 1 << vk % 4 * 2))
			key_state_recent_bitmap[vk / 8] |= 1 << vk % 8;
	}
}


int main()
{


	hVmm = VMMDLL_Initialize(3, (LPSTR[]) { "", "-device", "fpga" });
	std::string checkHandle = (hVmm == 0) ? "[ERROR] Failed to get handle!" : "Sucessfully opened handle:";
	printf("%s %p\n", checkHandle.c_str(), hVmm);


	//Get Offsets to get AsyncKeyHandle
	csrssPID = GetProcID((LPSTR)"csrss.exe");
	auto tmp = VMMDLL_ProcessGetModuleBaseU(hVmm, csrssPID, "win32ksgd.sys");
	UINT64 va_gSessionGlobalSlots = tmp + 0x3110;
	UINT64 Session1_UserSessionState = ReadMemory<UINT64>(ReadMemory<UINT64>(ReadMemory<UINT64>(va_gSessionGlobalSlots, csrssPID), csrssPID), csrssPID);
	gafAsyncKeyState = Session1_UserSessionState + 0x3690;

	std::cout << "Waiting for Input:" << std::endl;
	while (true)
	{
		update_key_state_bitmap();

		if (is_key_down(VK_INSERT))
		{
			std::cout << "Insert pressed" << std::endl;
		}

		if (was_key_pressed(VK_END))
		{
			std::cout << "End pressed" << std::endl;
		}

	}

}
