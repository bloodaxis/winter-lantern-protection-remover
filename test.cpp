#include <cassert>
#include <cstdint>

namespace test {
int calls;
bool original(void*) { ++calls; return true; }
bool hook(void* data_module) {
    auto* module = static_cast<std::uint8_t*>(data_module);
    auto* character = module
        ? *reinterpret_cast<std::uint8_t**>(module + 0x08)
        : nullptr;
    if (character && *reinterpret_cast<std::int32_t*>(character + 0x60) ==
                         52800086)
        return false;
    return original(data_module);
}
}

int main() {
    alignas(8) std::uint8_t target[0x68]{};
    alignas(8) std::uint8_t other[0x68]{};
    alignas(8) std::uint8_t target_module[0x10]{};
    alignas(8) std::uint8_t other_module[0x10]{};
    *reinterpret_cast<std::int32_t*>(target + 0x60) = 52800086;
    *reinterpret_cast<std::int32_t*>(other + 0x60) = 52800085;
    *reinterpret_cast<void**>(target_module + 0x08) = target;
    *reinterpret_cast<void**>(other_module + 0x08) = other;
    assert(!test::hook(target_module) && test::calls == 0);
    assert(test::hook(other_module) && test::calls == 1);
    assert(test::hook(nullptr) && test::calls == 2);
}
