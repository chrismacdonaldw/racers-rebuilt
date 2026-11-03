#include "mods/hook_registry.hpp"

namespace rr::mods {

void ResolveThroughHooks(assets::Vfs& vfs, const HookRegistry& hooks) {
    vfs.SetResolver([&hooks](assets::AssetRequest& request) {
        hooks.Emit(AssetResolving{.request = request});
    });
}

}
