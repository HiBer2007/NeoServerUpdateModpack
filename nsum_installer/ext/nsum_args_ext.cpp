// nsum_args_ext - NSUM product-specific extension (STATIC, linked into hci_gui).
//
// Product-specific CLI args (--with-editor / --use-system-git / --use-bundled-git)
// and the nsum_git_plan custom step. Registered via HCI_REGISTER_EXTENSION
// (static link-time registration) - no DLL deployment needed; the host's
// ExtensionLoader::loadStatic() picks it up. Help texts for each arg surface
// in the shells' --help output (extension metadata custom interface).
//
// The extension model keeps the core product-agnostic: nothing here lives in
// HiBerCommonInstaller itself.

#include "hci/exec.h"
#include "hci/extension.h"
#include "hci/port.h"

#include <filesystem>

namespace {

namespace fs = std::filesystem;

bool systemGitAvailable(std::string* pathOut = nullptr)
{
    hci::exec::ProcessResult r;
    if (hci::exec::runProcess({"git", "--version"}, 5000, r)) {
        if (r.exitCode == 0) {
            if (pathOut) *pathOut = "git";
            return true;
        }
    }
    // Common install paths (Program Files / Program Files (x86) / LocalAppData).
    std::vector<std::string> candidates;
    std::string pf = hci::port::getEnv("ProgramFiles");
    std::string pf86 = hci::port::getEnv("ProgramFiles(x86)");
    std::string la = hci::port::getEnv("LocalAppData");
    if (!pf.empty()) candidates.push_back(pf + "\\Git\\bin\\git.exe");
    if (!pf86.empty()) candidates.push_back(pf86 + "\\Git\\bin\\git.exe");
    if (!la.empty()) candidates.push_back(la + "\\Programs\\Git\\bin\\git.exe");
    for (const auto& c : candidates) {
        std::error_code ec;
        if (fs::exists(fs::u8path(c), ec)) {
            hci::exec::ProcessResult vr;
            if (hci::exec::runProcess({c, "--version"}, 5000, vr) && vr.exitCode == 0) {
                if (pathOut) *pathOut = c;
                return true;
            }
        }
    }
    return false;
}

class NsumArgsExtension : public hci::IHciExtension {
public:
    const char* id() const override { return "nsum.args"; }
    const char* version() const override { return "1.0.0"; }

    hci::HciCapabilities capabilities() const override
    {
        hci::HciCapabilities c;
        c.providesSteps = true;
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
        api.registry().registerCliArg(
            "--use-system-git",
            [](const std::string&, hci::InstallContext& ctx) {
                ctx.vars().set("gitMode", "system");
                return true;
            },
            "force using a system-installed Git");
        api.registry().registerCliArg(
            "--use-bundled-git",
            [](const std::string&, hci::InstallContext& ctx) {
                ctx.vars().set("gitMode", "bundled");
                return true;
            },
            "force downloading and using the bundled Git");

        // nsum_git_plan: decide git strategy.
        api.registry().registerStep(
            "nsum_git_plan",
            [](const nlohmann::json& params, hci::InstallContext& ctx,
               std::string& error) {
                (void)error;
                hci::Vars& v = ctx.vars();
                std::string mode = v.get("gitMode");
                bool editor = v.getBool("components.editor", false);
                std::string variant = editor ? "PortableGit" : "MinGit";
                if (params.contains("editorVariant") && editor)
                    variant = params.value("editorVariant", "PortableGit");

                if (mode == "system") {
                    std::string path;
                    if (systemGitAvailable(&path)) {
                        v.setBool("gitUseSystem", true);
                        v.setBool("gitDownload", false);
                        v.set("systemGitPath", path);
                        v.set("gitPath", path);
                    } else {
                        v.setBool("gitUseSystem", false);
                        v.setBool("gitDownload", true); // fallback to bundled
                        v.set("gitPath", "{installDir}/tools/git/bin/git.exe");
                    }
                } else if (mode == "bundled") {
                    v.setBool("gitUseSystem", false);
                    v.setBool("gitDownload", true);
                    v.set("gitPath", "{installDir}/tools/git/bin/git.exe");
                } else {
                    // auto: use system git when present, else download bundled.
                    std::string path;
                    if (systemGitAvailable(&path)) {
                        v.setBool("gitUseSystem", true);
                        v.setBool("gitDownload", false);
                        v.set("systemGitPath", path);
                        v.set("gitPath", path);
                    } else {
                        v.setBool("gitUseSystem", false);
                        v.setBool("gitDownload", true);
                        v.set("gitPath", "{installDir}/tools/git/bin/git.exe");
                    }
                }
                v.set("gitVariant", variant);
                v.setBool("gitPlanned", true);
                return true;
            });
        return true;
    }

    void shutdown() override {}
};

} // namespace

HCI_REGISTER_EXTENSION(NsumArgsExtension);