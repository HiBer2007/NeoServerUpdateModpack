// nsum_args_ext - NSUM product-specific extension (STATIC, linked into hci_gui).
//
// Product-specific CLI args (--with-editor). The Git strategy (system vs
// bundled) is handled by the GENERIC hci_git extension (ext/git in
// HiBerCommonInstaller, linked statically the same way) - see install.json.
//
// Registered via HCI_REGISTER_EXTENSION (static link-time registration);
// the extension model keeps the core product-agnostic.

#include "hci/extension.h"

#include <string>

namespace {

class NsumArgsExtension : public hci::IHciExtension {
public:
    const char* id() const override { return "nsum.args"; }
    const char* version() const override { return "1.0.0"; }

    hci::HciCapabilities capabilities() const override
    {
        hci::HciCapabilities c;
        c.providesCliArgs = true;
        return c;
    }

    bool init(hci::HostApi& api) override
    {
        // --with-editor: select the editor component (pre-set before the
        // components UI step; the runner preserves pre-set values).
        api.registry().registerCliArg(
            "--with-editor",
            [](const std::string&, hci::InstallContext& ctx) {
                ctx.vars().setBool("components.editor", true);
                return true;
            },
            "preselect the NeoWorkspaceEditor component");
        return true;
    }

    void shutdown() override {}
};

} // namespace

HCI_REGISTER_EXTENSION(NsumArgsExtension);