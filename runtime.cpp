#include <windows.h>
#include <cstdint>
#include <cstring>
#include "MinHook.h"

namespace {
constexpr std::int32_t kNpcParam = 52800086;
constexpr std::int32_t kParryUnlockEffect = 20011471;
constexpr std::size_t kNpcParamOffset = 0x60;
constexpr std::size_t kChrFlagsOffset = 0x1c5;
constexpr std::size_t kDataOwnerOffset = 0x08;
constexpr std::size_t kDataFlagsOffset = 0x19b;
constexpr std::uint8_t kChrInvincible = 0x10;
constexpr std::uint8_t kDataUndamageable = 0x02;
using CheckA = std::uint8_t(*)(void*, void*, std::uint8_t);
using CheckB = std::uint8_t(*)(void*, std::uint8_t, std::uint8_t);
using CheckC = std::uint8_t(*)(void*);
using HitDisabled = bool(*)(void*);
using ApplySpEffect = void(*)(void*, std::int32_t, std::uint8_t);
using SetInvincibility = void(*)(void*, void*, void**);
CheckA original_check_a; CheckB original_check_b; CheckC original_check_c;
HitDisabled original_hit_disabled; ApplySpEffect apply_sp_effect;
SetInvincibility original_set_invincibility;

bool is_target(void* character) {
    return character && *reinterpret_cast<std::int32_t*>(
        static_cast<std::uint8_t*>(character) + kNpcParamOffset) == kNpcParam;
}

void clear_protection(void* character) {
    if (!is_target(character)) return;
    auto* chr = static_cast<std::uint8_t*>(character);
    chr[kChrFlagsOffset] &= static_cast<std::uint8_t>(~kChrInvincible);
    auto* modules = *reinterpret_cast<std::uint8_t**>(chr + 0x190);
    auto* data = modules ? *reinterpret_cast<std::uint8_t**>(modules) : nullptr;
    if (data) data[kDataFlagsOffset] &= static_cast<std::uint8_t>(~kDataUndamageable);
}

bool has_effect(void* character, std::int32_t effect_id) {
    auto* chr = static_cast<std::uint8_t*>(character);
    auto* effects = *reinterpret_cast<std::uint8_t**>(chr + 0x178);
    auto* entry = effects ? *reinterpret_cast<std::uint8_t**>(effects + 0x08) : nullptr;
    for (unsigned count = 0; entry && count < 512; ++count) {
        if (*reinterpret_cast<std::int32_t*>(entry + 0x08) == effect_id)
            return true;
        entry = *reinterpret_cast<std::uint8_t**>(entry + 0x30);
    }
    return false;
}

void unlock_target(void* character) {
    if (!is_target(character)) return;
    if (has_effect(character, kParryUnlockEffect)) {
        clear_protection(character);
        return;
    }
    thread_local bool active = false;
    if (active) return;
    active = true;
    apply_sp_effect(character, kParryUnlockEffect, 0);
    clear_protection(character);
    active = false;
}

std::uint8_t check_a_hook(void* chr, void* source, std::uint8_t mode) {
    if (is_target(chr)) { unlock_target(chr); return 0; }
    return original_check_a(chr, source, mode);
}
std::uint8_t check_b_hook(void* chr, std::uint8_t a, std::uint8_t b) {
    if (is_target(chr)) { unlock_target(chr); return 0; }
    return original_check_b(chr, a, b);
}
std::uint8_t check_c_hook(void* chr) {
    if (is_target(chr)) { unlock_target(chr); return 0; }
    return original_check_c(chr);
}
bool hit_disabled_hook(void* data) {
    auto* bytes = static_cast<std::uint8_t*>(data);
    auto* chr = bytes ? *reinterpret_cast<std::uint8_t**>(bytes + kDataOwnerOffset) : nullptr;
    if (is_target(chr)) {
        unlock_target(chr);
        bytes[kDataFlagsOffset] &= static_cast<std::uint8_t>(~kDataUndamageable);
        return false;
    }
    return original_hit_disabled(data);
}
void set_invincibility_hook(void* command, void* context, void** chr_ref) {
    original_set_invincibility(command, context, chr_ref);
    unlock_target(chr_ref ? *chr_ref : nullptr);
}

bool hook(std::uintptr_t base, std::uintptr_t rva, const unsigned char* expected,
          std::size_t size, void* detour, void** original) {
    void* target = reinterpret_cast<void*>(base + rva);
    return std::memcmp(target, expected, size) == 0 &&
           MH_CreateHook(target, detour, original) == MH_OK;
}

DWORD WINAPI install(void* module) {
    auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"eldenring.exe"));
    if (!base) return 0;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->FileHeader.TimeDateStamp != 0x6a96b418 ||
        nt->OptionalHeader.SizeOfImage != 0x5e0da00) return 0;

    constexpr unsigned char a[] = {0x48,0x8b,0x41,0x10,0x45,0x0f,0xb6,0xd8,0x4c,0x8b,0xc9,0x44,0x0f,0xb6,0x50,0x0a};
    constexpr unsigned char b[] = {0x48,0x8b,0x41,0x10,0x4c,0x8b,0xc9,0x45,0x0f,0xb6,0xd0,0x0f,0xb6,0x48,0x0a};
    constexpr unsigned char c[] = {0x40,0x53,0x48,0x83,0xec,0x20,0x48,0x8b,0x41,0x10,0x48,0x8b,0xd9,0x0f,0xb6,0x50,0x0a};
    constexpr unsigned char hit[] = {0x48,0x83,0xec,0x28,0xf6,0x81,0x9b,0x01,0x00,0x00,0x02,0x74,0x07,0xb0,0x01};
    constexpr unsigned char effect[] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xec,0xc0,0x00,0x00,0x00};
    constexpr unsigned char inv[] = {0x40,0x53,0x56,0x57,0x48,0x83,0xec,0x50,0x48,0xc7,0x44,0x24,0x38,0xfe,0xff,0xff,0xff};

    void* effect_target = reinterpret_cast<void*>(base + 0x3e8dc0);
    if (std::memcmp(effect_target, effect, sizeof(effect)) != 0) return 0;
    apply_sp_effect = reinterpret_cast<ApplySpEffect>(effect_target);

    if (MH_Initialize() != MH_OK ||
        !hook(base,0x3f3dc0,a,sizeof(a),(void*)check_a_hook,(void**)&original_check_a) ||
        !hook(base,0x3f3ed0,b,sizeof(b),(void*)check_b_hook,(void**)&original_check_b) ||
        !hook(base,0x3f3f60,c,sizeof(c),(void*)check_c_hook,(void**)&original_check_c) ||
        !hook(base,0x437970,hit,sizeof(hit),(void*)hit_disabled_hook,(void**)&original_hit_disabled) ||
        !hook(base,0x4a3df0,inv,sizeof(inv),(void*)set_invincibility_hook,(void**)&original_set_invincibility) ||
        MH_EnableHook(MH_ALL_HOOKS) != MH_OK) { MH_Uninitialize(); return 0; }

    HMODULE pinned;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(module), &pinned);
    return 0;
}
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr,0,install,module,0,nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
