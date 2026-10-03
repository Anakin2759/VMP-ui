#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <span>
#include <utility>

#include <ui.hpp>

namespace
{
void WriteStderr(const char* text) noexcept
{
    if (text == nullptr)
    {
        return;
    }
    const auto size = std::strlen(text);
    if (std::fwrite(text, 1U, size, stderr) != size)
    {
        std::clearerr(stderr);
    }
}

ui::entity RequireButton(ui::Result<ui::EntityHandle> result)
{
    if (!result)
    {
        ui::log::Error("Create sample button failed: {}", result.error().ToString());
        return ui::null_entity;
    }
    return result->raw;
}
}  // namespace

int main(int argc, char* argv[]) noexcept
{
    try
    {
        auto appResult = ui::factory::CreateApplication(std::span<char*>(argv, argc));
        if (!appResult)
        {
            WriteStderr("UI initialization failed: ");
            WriteStderr(appResult.error().ToString().c_str());
            WriteStderr("\n");
            return EXIT_FAILURE;
        }

        auto app = std::move(*appResult);
        auto windowResult = ui::factory::CreateWindow("VMP-ui Sample", "sampleWindow");
        if (!windowResult)
        {
            WriteStderr("Window creation failed: ");
            WriteStderr(windowResult.error().ToString().c_str());
            WriteStderr("\n");
            return EXIT_FAILURE;
        }

        using namespace ui::chains;
        const auto window = windowResult->raw;
        window | Size(520.0F, 280.0F) | BackgroundColor({0.08F, 0.10F, 0.14F, 1.0F});

        // 锚点示例：内容面板相对窗口水平、垂直居中；面板内部仍使用普通垂直布局。
        const auto content = ui::factory::CreateVBoxLayout("sampleContent");
        content | FixedSize(420.0F, 210.0F) | AnchorCenterIn() | Padding(24.0F) | Spacing(14.0F) |
            BackgroundColor({0.11F, 0.14F, 0.20F, 0.96F}) | BorderRadius(10.0F) |
            BorderColor({0.24F, 0.32F, 0.46F, 1.0F}) | BorderThickness(1.0F);

        const auto title = ui::factory::CreateLabel("Minimal form", "sampleTitle");
        title | SizePolicy(ui::policies::Size::H_EXPANDING | ui::policies::Size::V_FIXED) | Size(0.0F, 32.0F) |
            FontSize(22.0F) | TextColor({0.92F, 0.95F, 1.0F, 1.0F}) |
            TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);

        const auto hint = ui::factory::CreateLabel("Enter a value and press Submit.", "sampleHint");
        hint | SizePolicy(ui::policies::Size::H_EXPANDING | ui::policies::Size::V_FIXED) | Size(0.0F, 24.0F) |
            FontSize(13.0F) | TextColor({0.62F, 0.68F, 0.78F, 1.0F});

        const auto edit = ui::factory::CreateLineEdit("", "Type here...", "sampleEdit");
        edit | SizePolicy(ui::policies::Size::H_EXPANDING | ui::policies::Size::V_FIXED) | Size(0.0F, 40.0F) |
            Padding(10.0F) | FontSize(15.0F) | BackgroundColor({0.14F, 0.17F, 0.23F, 1.0F}) |
            BorderRadius(6.0F) | BorderColor({0.30F, 0.38F, 0.52F, 1.0F}) | BorderThickness(1.0F) |
            OnTextChanged([](const std::string& value) { ui::log::Debug("sample edit changed: {}", value); });

        const auto button = RequireButton(ui::factory::CreateButton("Submit", "sampleSubmit"));
        button | SizePolicy(ui::policies::Size::H_EXPANDING | ui::policies::Size::V_FIXED) | Size(0.0F, 42.0F) |
            BackgroundColor({0.20F, 0.48F, 0.86F, 1.0F}) | BorderRadius(6.0F) |
            OnClick([edit]() {
                ui::log::Info("Submit clicked: {}", ui::text::GetTextEditContent(edit));
            });

        content | AddChild(title) | AddChild(hint) | AddChild(edit) | AddChild(button);
        window | AddChild(content) | Show();
        app->exec();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        WriteStderr(exception.what());
        WriteStderr("\n");
        return EXIT_FAILURE;
    }
}
