/**
 * ************************************************************************
 *
 * @file Mainwindow.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-24
 * @version 0.2
 *
 *            Table / TextBrowser
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include <ui.hpp>

namespace example::ui_demo::view
{
using namespace ui::chains;

namespace detail
{
inline entt::entity MakeSectionTitle(const std::string& text, const std::string& alias)
{
    auto lbl = ui::factory::CreateLabel(text, alias);
    lbl | TextColor({1.0F, 0.85F, 0.5F, 1.0F}) | FontSize(14.0F)
        | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 22.0F)
        | TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);
    return lbl;
}

inline std::string DemoAssetPath(std::string_view fileName)
{
    const auto assetPath = std::filesystem::path(__FILE__).parent_path().parent_path() / "assets" / fileName;
    return assetPath.string();
}

inline entt::entity MakeImageCard(std::string_view title, std::string_view assetFileName, const std::string& alias)
{
    auto card = ui::factory::CreateVBoxLayout(alias);
    card | FixedSize(96.0F, 124.0F) | BackgroundColor({0.12F, 0.12F, 0.16F, 0.92F}) | BorderRadius(6.0F)
        | BorderColor({0.28F, 0.28F, 0.36F, 1.0F}) | BorderThickness(1.0F) | Padding(6.0F) | Spacing(5.0F);

    auto image = ui::factory::CreateImageFromPath(DemoAssetPath(assetFileName), 84.0F, 84.0F, alias + "_image");
    image | FixedSize(84.0F, 84.0F) | BorderRadius(4.0F) | BackgroundColor({0.05F, 0.05F, 0.07F, 1.0F});

    auto label = ui::factory::CreateLabel(std::string(title), alias + "_label");
    label | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 18.0F)
        | TextAlignment(ui::policies::Alignment::CENTER) | TextColor({0.86F, 0.86F, 0.90F, 1.0F}) | FontSize(11.0F);

    card | AddChild(image) | AddChild(label);
    return card;
}
} // namespace detail

/**
 */
inline void CreateMainWindow() // NOLINT
{
    auto gameWindow = ui::factory::CreateWindow("UI Controls Demo", "gameWindow");

    gameWindow | WindowFlag(ui::policies::WindowFlag::DEFAULT) | Size(1200.0F, 800.0F)
        | BackgroundColor({0.1F, 0.1F, 0.12F, 1.0F}) | BorderRadius(4.0F)
        | LayoutDirection(ui::policies::LayoutDirection::VERTICAL) | Spacing(10.0F) | Padding(10.0F);

    gameWindow | WindowFlag(ui::policies::WindowFlag::DEFAULT) | Size(1200.0F, 800.0F)
        | BackgroundColor({0.10F, 0.10F, 0.12F, 1.0F}) | BorderRadius(4.0F)
        | LayoutDirection(ui::policies::LayoutDirection::VERTICAL) | Spacing(8.0F) | Padding(8.0F);

    auto panelStyle = BackgroundColor({0.06F, 0.06F, 0.09F, 0.85F}) | BorderRadius(6.0F)
                    | BorderColor({0.25F, 0.25F, 0.32F, 0.9F}) | BorderThickness(1.0F) | Padding(8.0F) | Spacing(5.0F);

    auto row1 = ui::factory::CreateHBoxLayout("row1");
    row1 | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 340.0F) | Spacing(8.0F);
    gameWindow | AddChild(row1);

    {
        auto inputPanel = ui::factory::CreateVBoxLayout("inputPanel");
        inputPanel | panelStyle | FixedSize(530.0F, 340.0F);
        row1 | AddChild(inputPanel);

        inputPanel | AddChild(detail::MakeSectionTitle("Input Controls", "inputTitle"));

        auto btnRow = ui::factory::CreateHBoxLayout("btnRow");
        btnRow | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 32.0F) | Spacing(5.0F);

        auto primaryBtn = ui::factory::CreateButton("Primary", "primaryBtn");
        primaryBtn | FixedSize(88.0F, 30.0F) | BackgroundColor({0.20F, 0.50F, 0.85F, 1.0F}) | BorderRadius(5.0F)
            | BorderColor({0.30F, 0.65F, 1.0F, 1.0F}) | BorderThickness(1.0F)
            | OnClick([]() { ui::log::Info("Primary button"); });

        auto warnBtn = ui::factory::CreateButton("Warning", "warnBtn");
        warnBtn | FixedSize(88.0F, 30.0F) | BackgroundColor({0.75F, 0.45F, 0.10F, 1.0F}) | BorderRadius(5.0F)
            | BorderColor({1.0F, 0.65F, 0.20F, 1.0F}) | BorderThickness(1.0F)
            | OnClick([]() { ui::log::Info("Warning button"); });

        auto dangerBtn = ui::factory::CreateButton("Danger", "dangerBtn");
        dangerBtn | FixedSize(88.0F, 30.0F) | BackgroundColor({0.65F, 0.18F, 0.18F, 1.0F}) | BorderRadius(5.0F)
            | BorderColor({0.85F, 0.30F, 0.30F, 1.0F}) | BorderThickness(1.0F)
            | OnClick([]() { ui::log::Info("Danger button"); });

        auto ghostBtn = ui::factory::CreateButton("Ghost", "ghostBtn");
        ghostBtn | FixedSize(88.0F, 30.0F) | BackgroundColor({0.0F, 0.0F, 0.0F, 0.0F}) | BorderRadius(5.0F)
            | BorderColor({0.60F, 0.60F, 0.65F, 1.0F}) | BorderThickness(1.5F) | TextColor({0.80F, 0.80F, 0.85F, 1.0F})
            | OnClick([]() { ui::log::Info("Ghost button"); });

        auto disabledBtn = ui::factory::CreateButton("Disabled", "disabledBtn");
        disabledBtn | FixedSize(88.0F, 30.0F) | BackgroundColor({0.25F, 0.25F, 0.28F, 0.6F}) | BorderRadius(5.0F)
            | BorderColor({0.35F, 0.35F, 0.38F, 0.5F}) | BorderThickness(1.0F) | ButtonEnabled(false);

        btnRow | AddChild(primaryBtn) | AddChild(warnBtn) | AddChild(dangerBtn) | AddChild(ghostBtn)
            | AddChild(disabledBtn);
        inputPanel | AddChild(btnRow);

        // CheckBox row
        auto cbRow = ui::factory::CreateHBoxLayout("cbRow");
        cbRow | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 26.0F) | Spacing(10.0F);

        auto cb1 = ui::factory::CreateCheckBox("Option A", true, "cb1");
        auto cb2 = ui::factory::CreateCheckBox("Option B", false, "cb2");
        auto cb3 = ui::factory::CreateCheckBox("Option C", false, "cb3");
        for (auto cbEnt : {cb1, cb2, cb3})
        {
            cbEnt | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL) | BorderRadius(3.0F);
        }
        cbRow | AddChild(cb1) | AddChild(cb2) | AddChild(cb3);
        inputPanel | AddChild(cbRow);

        auto lineEdit = ui::factory::CreateLineEdit("", "Enter text...", "demoLineEdit");
        lineEdit | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 30.0F)
            | BackgroundColor({0.15F, 0.15F, 0.18F, 0.95F}) | BorderRadius(4.0F)
            | BorderColor({0.35F, 0.35F, 0.42F, 1.0F}) | BorderThickness(1.0F) | Padding(6.0F) | FontSize(13.0F)
            | OnTextChanged([](const std::string& /*v*/) { ui::log::Info("LineEdit changed"); });
        inputPanel | AddChild(lineEdit);

        auto pwdEdit = ui::factory::CreateLineEdit("", "Password...", "pwdEdit");
        pwdEdit | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 30.0F)
            | BackgroundColor({0.15F, 0.15F, 0.18F, 0.95F}) | BorderRadius(4.0F)
            | BorderColor({0.35F, 0.35F, 0.42F, 1.0F}) | BorderThickness(1.0F) | Padding(6.0F) | FontSize(13.0F)
            | PasswordMode(ui::policies::TextFlag::PASSWORD);
        inputPanel | AddChild(pwdEdit);

        auto multiEdit = ui::factory::CreateTextEdit("Multi-line text input (TextEdit)...", true, "multiEdit");
        multiEdit | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 68.0F)
            | BackgroundColor({0.12F, 0.12F, 0.16F, 0.95F}) | BorderRadius(4.0F)
            | BorderColor({0.30F, 0.30F, 0.38F, 1.0F}) | BorderThickness(1.0F) | Padding(6.0F) | FontSize(12.0F)
            | TextWordWrap(ui::policies::TextWrap::CHAR) | TextWrapWidth(490.0F);
        inputPanel | AddChild(multiEdit);

        auto progressBar = ui::factory::CreateProgressBar("demoProgress");
        progressBar | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 16.0F)
            | ProgressValue(0.40F) | ProgressFillColor({0.20F, 0.75F, 0.45F, 1.0F})
            | ProgressBackgroundColor({0.20F, 0.20F, 0.24F, 1.0F}) | BorderRadius(8.0F)
            | ProgressLabel(ui::policies::LabelVisibility::VISIBLE);
        inputPanel | AddChild(progressBar);

        auto hSlider = ui::factory::CreateSlider("hSlider");
        hSlider | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 22.0F)
            | SliderRange(0.0F, 100.0F) | SliderValue(40.0F) | SliderStep(1.0F)
            | OnSliderValueChanged([progressBar](float val) { progressBar | ProgressValue(val / 100.0F); });
        inputPanel | AddChild(hSlider);

        auto vRow = ui::factory::CreateHBoxLayout("vSliderRow");
        vRow | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 36.0F) | Spacing(6.0F);

        auto vLabel = ui::factory::CreateLabel("Vertical Slider:", "vSliderLabel");
        vLabel | FixedSize(90.0F, 36.0F) | FontSize(12.0F) | TextColor({0.70F, 0.70F, 0.75F, 1.0F})
            | TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);

        auto vSlider = ui::factory::CreateSlider("vSlider");
        vSlider | SliderOrientation(ui::policies::Orientation::VERTICAL) | SliderRange(0.0F, 100.0F)
            | SliderValue(60.0F) | FixedSize(22.0F, 34.0F);

        vRow | AddChild(vLabel) | AddChild(vSlider);
        inputPanel | AddChild(vRow);
    }

    {
        auto canvasPanel = ui::factory::CreateVBoxLayout("canvasPanel");
        canvasPanel | panelStyle | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL);
        row1 | AddChild(canvasPanel);

        canvasPanel
            | AddChild(
                detail::MakeSectionTitle("Canvas Drawing: Line / Rect / Circle / Polyline / Bezier", "canvasTitle"));

        auto canvas = ui::factory::CreateCanvas(640.0F, 290.0F, "demoCanvas");
        canvas | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL)
            | BackgroundColor({0.08F, 0.08F, 0.11F, 1.0F}) | BorderRadius(4.0F)
            | BorderColor({0.25F, 0.25F, 0.32F, 0.9F}) | BorderThickness(1.0F);

        canvas | CanvasDrawLine({10.0F, 10.0F}, {200.0F, 80.0F}, {0.30F, 0.80F, 0.40F, 1.0F}, 2.0F)
            | CanvasDrawLine({10.0F, 80.0F}, {200.0F, 10.0F}, {0.30F, 0.80F, 0.40F, 1.0F}, 2.0F);

        canvas | CanvasDrawRect({220.0F, 10.0F}, {390.0F, 90.0F}, {0.30F, 0.60F, 1.00F, 1.0F}, 2.0F);

        canvas | CanvasDrawFilledRect({410.0F, 10.0F}, {580.0F, 90.0F}, {0.80F, 0.30F, 0.30F, 0.8F});

        ui::canvas::DrawCircle(canvas, {70.0F, 180.0F}, 55.0F, {0.90F, 0.70F, 0.20F, 1.0F}, 2.5F);
        ui::canvas::DrawFilledCircle(canvas, {230.0F, 180.0F}, 55.0F, {0.40F, 0.30F, 0.80F, 0.85F});

        ui::canvas::DrawPolyline(canvas,
                                 {{320.0F, 110.0F},
                                  {350.0F, 150.0F},
                                  {335.0F, 150.0F},
                                  {370.0F, 200.0F},
                                  {340.0F, 200.0F},
                                  {375.0F, 250.0F}},
                                 {1.0F, 0.85F, 0.10F, 1.0F},
                                 2.5F);

        ui::canvas::DrawCubicBezier(canvas,
                                    {400.0F, 110.0F},
                                    {420.0F, 260.0F},
                                    {560.0F, 110.0F},
                                    {580.0F, 250.0F},
                                    {0.40F, 0.80F, 1.0F, 1.0F},
                                    2.5F);

        {
            ui::canvas::Painter painter(canvas);
            painter
                .moveTo({490.0F, 143.0F})
                .cubicTo({488.0F, 116.0F}, {456.0F, 114.0F}, {458.0F, 140.0F})
                .cubicTo({458.0F, 168.0F}, {484.0F, 208.0F}, {490.0F, 215.0F})
                .cubicTo({496.0F, 208.0F}, {522.0F, 168.0F}, {522.0F, 140.0F})
                .cubicTo({524.0F, 114.0F}, {492.0F, 116.0F}, {490.0F, 143.0F})
                .commit({1.0F, 0.35F, 0.45F, 1.0F}, 2.0F);
        }

        canvas | CanvasDrawLine({10.0F, 270.0F}, {610.0F, 270.0F}, {0.45F, 0.45F, 0.50F, 0.7F}, 1.0F)
            | CanvasDrawLine({10.0F, 10.0F}, {10.0F, 270.0F}, {0.45F, 0.45F, 0.50F, 0.7F}, 1.0F);

        canvasPanel | AddChild(canvas);
    }

    auto row2 = ui::factory::CreateHBoxLayout("row2");
    row2 | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL) | Spacing(8.0F);
    gameWindow | AddChild(row2);

    {
        auto tablePanel = ui::factory::CreateVBoxLayout("tablePanel");
        tablePanel | panelStyle | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL);
        row2 | AddChild(tablePanel);

        tablePanel | AddChild(detail::MakeSectionTitle("Table Demo", "tableTitle"));

        auto dataTable = ui::factory::CreateTable(5, "dataTable");
        dataTable | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL)
            | BackgroundColor({0.10F, 0.10F, 0.13F, 0.9F}) | BorderRadius(4.0F)
            | BorderColor({0.28F, 0.28F, 0.35F, 0.9F}) | BorderThickness(1.0F)
            | ScrollMode(ui::policies::Scroll::BOTH)
            | TableColumns(5, {"Name", "Score", "K/D", "Online", "Action"})
            | TableColumnSizingMode(ui::policies::TableColumnSizing::FIXED)
            | TableColumnWidths({220.0F, 110.0F, 90.0F, 130.0F, 160.0F})
            | TableMinColumnWidths({180.0F, 90.0F, 80.0F, 120.0F, 150.0F})
            | TableRowHeight(30.0F) | TableMinRowHeight(28.0F)
            | TableAddRow({"Player One", "1200", "18/5", "", ""}) | TableAddRow({"Player Two", "980", "12/8", "", ""})
            | TableAddRow({"Player Three", "1560", "24/3", "", ""}) | TableAddRow({"Player Four", "740", "9/11", "", ""})
            | TableAddRow({"Player Five", "2100", "30/2", "", ""}) | TableAddRow({"Player Six", "430", "5/15", "", ""});

        auto banBtn0 = ui::factory::CreateButton("Ban", "banBtn0");
        banBtn0 | FixedSize(90.0F, 22.0F) | BackgroundColor({0.55F, 0.15F, 0.15F, 1.0F}) | BorderRadius(3.0F)
            | BorderColor({0.75F, 0.25F, 0.25F, 1.0F}) | BorderThickness(1.0F) | FontSize(11.0F)
            | OnClick([]() { ui::log::Info("Ban: Player One"); });

        auto banBtn1 = ui::factory::CreateButton("Ban", "banBtn1");
        banBtn1 | FixedSize(90.0F, 22.0F) | BackgroundColor({0.55F, 0.15F, 0.15F, 1.0F}) | BorderRadius(3.0F)
            | BorderColor({0.75F, 0.25F, 0.25F, 1.0F}) | BorderThickness(1.0F) | FontSize(11.0F)
            | OnClick([]() { ui::log::Info("Ban: Player Two"); });

        auto cb0 = ui::factory::CreateCheckBox("", true, "cb_row0");
        auto cb1 = ui::factory::CreateCheckBox("", false, "cb_row1");
        auto cb2 = ui::factory::CreateCheckBox("", true, "cb_row2");
        auto cb3 = ui::factory::CreateCheckBox("", false, "cb_row3");
        auto cb4 = ui::factory::CreateCheckBox("", true, "cb_row4");
        auto cb5 = ui::factory::CreateCheckBox("", false, "cb_row5");
        for (auto cbEnt : {cb0, cb1, cb2, cb3, cb4, cb5})
        {
            cbEnt | FixedSize(100.0F, 22.0F);
        }
        cb0 | OnCheckBoxChanged([](bool online) { ui::log::Info(online ? "Player One online" : "Player One offline"); });
        cb1 | OnCheckBoxChanged([](bool online) { ui::log::Info(online ? "Player Two online" : "Player Two offline"); });

        const std::vector<std::string> kActions{"Select action", "Ban", "Kick", "Inspect"};
        auto dd0 = ui::factory::CreateDropDown(kActions, 0, "dd_row0");
        auto dd1 = ui::factory::CreateDropDown(kActions, 0, "dd_row1");
        auto dd2 = ui::factory::CreateDropDown(kActions, 0, "dd_row2");
        auto dd3 = ui::factory::CreateDropDown(kActions, 0, "dd_row3");
        auto dd4 = ui::factory::CreateDropDown(kActions, 0, "dd_row4");
        auto dd5 = ui::factory::CreateDropDown(kActions, 0, "dd_row5");
        for (auto ddEnt : {dd0, dd1, dd2, dd3, dd4, dd5})
        {
            ddEnt | FixedSize(110.0F, 22.0F) | BackgroundColor({0.15F, 0.15F, 0.20F, 0.95F}) | BorderRadius(3.0F)
                | BorderColor({0.35F, 0.35F, 0.45F, 1.0F}) | BorderThickness(1.0F);
        }
        dd0 | OnDropDownChanged([](int idx) { ui::log::Info("Player One action: {}", idx); });
        dd1 | OnDropDownChanged([](int idx) { ui::log::Info("Player Two action: {}", idx); });

        dataTable | TableSetCellWidget(0, 3, cb0) | TableSetCellWidget(1, 3, cb1) | TableSetCellWidget(2, 3, cb2)
            | TableSetCellWidget(3, 3, cb3) | TableSetCellWidget(4, 3, cb4) | TableSetCellWidget(5, 3, cb5)
            | TableSetCellWidget(0, 4, dd0) | TableSetCellWidget(1, 4, dd1) | TableSetCellWidget(2, 4, dd2)
            | TableSetCellWidget(3, 4, dd3) | TableSetCellWidget(4, 4, dd4) | TableSetCellWidget(5, 4, dd5);
        (void)banBtn0;
        (void)banBtn1;

        tablePanel | AddChild(dataTable);
    }

    {
        auto scrollPanel = ui::factory::CreateVBoxLayout("scrollPanel");
        scrollPanel | panelStyle | Size(320.0F, 0.0F)
            | SizePolicy(ui::policies::Size::H_FIXED | ui::policies::Size::V_FILL);
        row2 | AddChild(scrollPanel);

        scrollPanel | AddChild(detail::MakeSectionTitle("ScrollArea Demo", "scrollTitle"));

        auto scrollArea = ui::factory::CreateScrollArea("demoScroll");
        scrollArea | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL)
            | BackgroundColor({0.10F, 0.10F, 0.13F, 0.7F}) | BorderRadius(4.0F)
            | BorderColor({0.28F, 0.28F, 0.35F, 0.9F}) | BorderThickness(1.0F) | Padding(4.0F)
            | ScrollMode(ui::policies::Scroll::VERTICAL)
            | ScrollBarPolicy(ui::policies::ScrollBar::DRAGGABLE | ui::policies::ScrollBar::AUTO_HIDE)
            | ScrollAnchor(ui::policies::ScrollAnchor::TOP) | ScrollSpeed(20.0F);

        auto scrollContent = ui::factory::CreateVBoxLayout("scrollContent");
        scrollContent | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::AUTO) | Spacing(3.0F);

        for (int i = 1; i <= 20; ++i)
        {
            const bool even = (i % 2 == 0);
            auto item = ui::factory::CreateLabel("Item " + std::to_string(i), "si" + std::to_string(i));
            const auto bgColor = even ? ui::Color{0.14F, 0.14F, 0.18F, 0.8F} : ui::Color{0.18F, 0.18F, 0.22F, 0.8F};
            item | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 22.0F)
                | BackgroundColor(bgColor) | BorderRadius(3.0F) | Padding(4.0F) | FontSize(12.0F)
                | TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);
            scrollContent | AddChild(item);
        }

        scrollArea | AddChild(scrollContent);
        scrollPanel | AddChild(scrollArea);
    }

    {
        auto imagePanel = ui::factory::CreateVBoxLayout("imagePanel");
        imagePanel | panelStyle | Size(330.0F, 0.0F)
            | SizePolicy(ui::policies::Size::H_FIXED | ui::policies::Size::V_FILL);
        row2 | AddChild(imagePanel);

        imagePanel | AddChild(detail::MakeSectionTitle("Image Demo", "imageTitle"));

        auto supportLabel = ui::factory::CreateLabel("Current decoder path: PNG / JPEG / BMP", "imageSupportLabel");
        supportLabel | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 20.0F)
            | TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER)
            | TextColor({0.75F, 0.78F, 0.84F, 1.0F}) | FontSize(11.0F);
        imagePanel | AddChild(supportLabel);

        auto imageRow = ui::factory::CreateHBoxLayout("imageRow");
        imageRow | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 132.0F) | Spacing(8.0F);

        imageRow | AddChild(detail::MakeImageCard("PNG", "sample.png", "pngCard"))
            | AddChild(detail::MakeImageCard("JPEG", "sample.jpg", "jpegCard"))
            | AddChild(detail::MakeImageCard("BMP", "sample.bmp", "bmpCard"));
        imagePanel | AddChild(imageRow);

        auto unsupportedLabel = ui::factory::CreateLabel("ICO / SVG: not supported by current ImageManager decoder", "imageUnsupportedLabel");
        unsupportedLabel | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 34.0F)
            | TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::TOP)
            | TextColor({0.92F, 0.66F, 0.40F, 1.0F}) | FontSize(11.0F);
        imagePanel | AddChild(unsupportedLabel);

        auto noteLabel = ui::factory::CreateLabel("Reason: current loader only dispatches BMP to SDL_LoadBMP and all other formats to stb_image; stb_image does not decode ICO/SVG.",
                                                   "imageNoteLabel");
        noteLabel | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL)
            | TextWordWrap(ui::policies::TextWrap::CHAR) | TextWrapWidth(290.0F)
            | TextAlignment(ui::policies::Alignment::TOP_LEFT) | TextColor({0.62F, 0.65F, 0.72F, 1.0F}) | FontSize(11.0F)
            | BackgroundColor({0.08F, 0.08F, 0.11F, 0.55F}) | BorderRadius(4.0F) | Padding(6.0F);
        imagePanel | AddChild(noteLabel);
    }

    auto row3 = ui::factory::CreateHBoxLayout("row3");
    row3 | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 200.0F) | Spacing(8.0F);
    gameWindow | AddChild(row3);

    {
        auto chatPanel = ui::factory::CreateVBoxLayout("chatPanel");
        chatPanel | panelStyle | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL);
        row3 | AddChild(chatPanel);

        chatPanel | AddChild(detail::MakeSectionTitle("TextBrowser + LineEdit Chat Demo", "chatTitle"));

        const std::string initMsg = "[System] Welcome to the UI controls demo.\n[System] Type a message below and press Enter or Send.";
        auto msgArea = ui::factory::CreateTextBrowser(initMsg, "", "msgArea");
        msgArea | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL) | TextContent(initMsg)
            | TextWordWrap(ui::policies::TextWrap::CHAR) | TextWrapWidth(1150.0F)
            | TextAlignment(ui::policies::Alignment::TOP_LEFT) | Padding(4.0F)
            | BackgroundColor({0.08F, 0.08F, 0.10F, 0.6F}) | BorderRadius(3.0F)
            | BorderColor({0.28F, 0.28F, 0.35F, 0.8F}) | BorderThickness(1.0F) | FontSize(13.0F);
        chatPanel | AddChild(msgArea);

        auto inputRow = ui::factory::CreateHBoxLayout("chatInputRow");
        inputRow | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FIXED) | Size(0.0F, 30.0F)
            | Spacing(5.0F);

        auto chatInput = ui::factory::CreateLineEdit("", "Type a message...", "chatInput");
        auto sendBtn = ui::factory::CreateButton("", "sendBtn");

        auto sendMessage = [chatInput, msgArea]()
        {
            std::string text = ui::text::GetTextEditContent(chatInput);
            if (!text.empty())
            {
                std::string history = ui::text::GetTextEditContent(msgArea);
                if (!history.empty()) history += "\n";
                history += "[You]: " + text;
                ui::text::SetTextEditContent(msgArea, history);
                ui::text::SetTextContent(msgArea, history);
                ui::text::SetTextEditContent(chatInput, "");
                ui::text::SetTextContent(chatInput, "");
                ui::utils::MarkRenderDirty(chatInput);
                ui::utils::MarkRenderDirty(msgArea);
            }
        };

        chatInput | SizePolicy(ui::policies::Size::H_FILL | ui::policies::Size::V_FILL)
            | BackgroundColor({0.15F, 0.15F, 0.18F, 0.9F}) | BorderRadius(3.0F)
            | BorderColor({0.30F, 0.30F, 0.35F, 1.0F}) | BorderThickness(1.0F) | FontSize(13.0F)
            | OnSubmit(sendMessage);

        sendBtn | Icon("MaterialSymbols", 0xe31b, ui::policies::IconFlag::DEFAULT, 20.0F, 0.0F)
            | SizePolicy(ui::policies::Size::H_FIXED | ui::policies::Size::V_FILL) | Size(40.0F, 0.0F)
            | BackgroundColor({0.20F, 0.50F, 0.80F, 1.0F}) | BorderRadius(4.0F)
            | BorderColor({0.30F, 0.60F, 1.0F, 1.0F}) | BorderThickness(1.0F) | OnClick(sendMessage);

        inputRow | AddChild(chatInput) | AddChild(sendBtn);
        chatPanel | AddChild(inputRow);
    }

    gameWindow | Show();
    ui::log::Info("Main window created: full controls demo");
}

} // namespace example::ui_demo::view
