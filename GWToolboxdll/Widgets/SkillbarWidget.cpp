#include "stdafx.h"

#include <GWCA/Constants/Skills.h>

#include <GWCA/GameEntities/Skill.h>

#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>

#include "SkillbarWidget.h"

#include "Timer.h"

/*
 * Based off of @JuliusPunhal April skill timer - https://github.com/JuliusPunhal/April-old/blob/master/Source/April/SkillbarOverlay.cpp
 */

namespace
{
    auto ms_to_string_sec(std::array<char, 16> &arr, clock_t const time, const char *fmt = "%d") -> void
    {
        snprintf(arr.data(), sizeof(arr), fmt, time);
    }

    auto ms_to_string_secf(std::array<char, 16> &arr, clock_t const cd, const char *fmt = "%.1f") -> void
    {
        snprintf(arr.data(), sizeof(arr), fmt, cd);
    }

    auto skill_cooldown_to_string(std::array<char, 16> &arr, clock_t const cd) -> void
    {
        if (cd > 180'000l || cd <= 0) {
            sprintf(arr.data(), "");
            return;
        }
        if (cd >= 10'000l)
            return ms_to_string_sec(arr, cd);
        if (cd > 0l)
            return ms_to_string_secf(arr, cd);
    }

    auto get_effect_durations(GW::Constants::SkillID const skillId) -> std::vector<clock_t>
    {
        auto durations = std::vector<clock_t>{};
        for (auto const &effect : GW::Effects::GetPlayerEffectArray()) {
            if (static_cast<GW::Constants::SkillID>(effect.skill_id) != skillId)
                continue;
            if (effect.effect_type == 7) // hex
                continue;
            auto duration = static_cast<clock_t>(effect.GetTimeRemaining());
            durations.emplace_back(std::max(duration, 0l));
        }
        return durations;
    }

    auto get_longest_effect_duration(GW::Constants::SkillID const skillId) -> clock_t
    {
        auto const durations = get_effect_durations(skillId);
        return *std::max_element(durations.begin(), durations.end());
    }
} // namespace

void SkillbarWidget::Draw(IDirect3DDevice9 *)
{
    if (!visible)
        return;
    [this]() {
        auto const *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (skillbar == nullptr)
            return;

        for (auto it = 0u; it < 8; it++) {
            skill_cooldown_to_string(m_skills[it].cooldown, static_cast<clock_t>(skillbar->skills[it].GetRecharge()));
            auto const skill_id = static_cast<GW::Constants::SkillID>(skillbar->skills[it].skill_id);
            clock_t const effect_duration = get_longest_effect_duration(skill_id);
            m_skills[it].color = UptimeToColor(effect_duration);
            if (display_effect_times) {
                auto durations = get_effect_durations(skill_id);
                std::sort(durations.begin(), durations.end(), [](auto a, auto b) { return b < a; });
                m_skills[it].effects.clear();
                std::transform(
                    durations.begin(), durations.end(), std::back_inserter(m_skills[it].effects), [this](auto duration) -> std::pair<std::array<char, 16>, Color> {
                        auto buf = std::array<char, 16>{};
                        skill_cooldown_to_string(buf, duration);
                        return std::pair{buf, UptimeToColor(duration)};
                    });
            }
        }
    }();

    const auto wnd_flags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar;

    ImGuiStyle &style = ImGui::GetStyle();
    const auto old_padding = style.WindowPadding;
    style.WindowPadding = {0, 0};

    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin(Name(), nullptr, GetWinFlags());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleColor(ImGuiCol_Text, color_text);
    ImGui::PushStyleColor(ImGuiCol_Border, color_border);
    {
        for (auto const &skill : m_skills) {
            ImGui::PushID(&skill);
            ImGui::PushStyleColor(ImGuiCol_Button, skill.color);
            const ImVec2 button_pos = ImGui::GetCursorPos();
            ImGui::Button(skill.cooldown.data(), {static_cast<float>(m_skill_width), static_cast<float>(m_skill_height)});
            if (!vertical)
                ImGui::SameLine();
            auto const next_button_pos = ImGui::GetCursorPos();
            if (display_effect_times) {
                auto const wnd_size = vertical ? ImVec2{static_cast<float>(m_skill_width) * 2, static_cast<float>(m_skill_height)} : ImVec2{static_cast<float>(m_skill_width), static_cast<float>(m_skill_height) * 2};
                if (vertical)
                    ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + static_cast<float>(m_effect_offset) - (m_effect_offset < 0 ? wnd_size.x / 2.f : 0), ImGui::GetWindowPos().y + button_pos.y});
                else
                    ImGui::SetNextWindowPos({ImGui::GetWindowPos().x + button_pos.x, ImGui::GetWindowPos().y + static_cast<float>(m_effect_offset) - (m_effect_offset < 0 ? wnd_size.y / 2.f : 0)});
                ImGui::SetNextWindowSize(wnd_size);
                ImGui::SetNextWindowBgAlpha(0.0f);
                ImGui::Begin((std::string("Skill###") + std::to_string(reinterpret_cast<uintptr_t>(&skill))).c_str(), nullptr, wnd_flags);
                for (auto i = 0u; i < skill.effects.size(); i++) {
                    int colbuf[4];
                    if (vertical) {
                        ImGui::SameLine();
                        if (m_effect_offset < 0) {
                            ImGui::SetCursorPosX(wnd_size.x - (i + 1) * static_cast<float>(m_skill_width) / 3);
                        } else {
                            ImGui::SetCursorPosX((i + 1) * static_cast<float>(m_skill_width) / 3);
                        }
                    } else {
                        if (m_effect_offset < 0) {
                            ImGui::SetCursorPosY(wnd_size.y - (i + 1) * static_cast<float>(m_skill_height) / 3);
                        } else {
                            ImGui::SetCursorPosY((i + 1) * static_cast<float>(m_skill_height) / 3);
                        }
                    }
                    Colors::ConvertU32ToInt4(skill.effects.at(i).second, colbuf);
                    colbuf[0] = 255;
                    ImGui::PushStyleColor(ImGuiCol_Text, Colors::ConvertInt4ToU32(colbuf));
                    ImGui::PushStyleColor(ImGuiCol_Button, 0);
                    ImGui::PushStyleColor(ImGuiCol_Border, 0);
                    if (vertical) {
                        ImGui::Button(skill.effects.at(i).first.data(), {static_cast<float>(m_skill_width) / 3, static_cast<float>(m_skill_height)});
                    } else {
                        ImGui::Button(skill.effects.at(i).first.data(), {static_cast<float>(m_skill_width), static_cast<float>(m_skill_height) / 3});
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::End();
            }
            ImGui::SetCursorPos(next_button_pos);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
    ImGui::End();
    style.WindowPadding = old_padding;
}

void SkillbarWidget::LoadSettings(CSimpleIni *ini)
{
    ToolboxWidget::LoadSettings(ini);
    color_text = Colors::Load(ini, Name(), VAR_NAME(color_text), Colors::White());
    color_border = Colors::Load(ini, Name(), VAR_NAME(color_border), Colors::White());
    color_long = Colors::Load(ini, Name(), VAR_NAME(color_long), Colors::ARGB(50, 0, 255, 0));
    color_medium = Colors::Load(ini, Name(), VAR_NAME(color_medium), Colors::ARGB(50, 255, 255, 0));
    color_short = Colors::Load(ini, Name(), VAR_NAME(color_short), Colors::ARGB(50, 255, 0, 0));
    m_skill_height = static_cast<int>(ini->GetLongValue(Name(), "height", m_skill_height));
    m_skill_width = static_cast<int>(ini->GetLongValue(Name(), "width", m_skill_width));
    vertical = ini->GetBoolValue(Name(), "vertical", false);
    display_effect_times = ini->GetBoolValue(Name(), "effect_text", false);
    m_effect_offset = static_cast<int>(ini->GetLongValue(Name(), "effect_offset", -100));
    medium_treshold = ini->GetLongValue(Name(), "medium_treshold", 5000);
    short_treshold = ini->GetLongValue(Name(), "short_treshold", 2500);
}

void SkillbarWidget::SaveSettings(CSimpleIni *ini)
{
    ToolboxWidget::SaveSettings(ini);
    Colors::Save(ini, Name(), VAR_NAME(color_text), color_text);
    Colors::Save(ini, Name(), VAR_NAME(color_border), color_border);
    Colors::Save(ini, Name(), VAR_NAME(color_long), color_long);
    Colors::Save(ini, Name(), VAR_NAME(color_medium), color_medium);
    Colors::Save(ini, Name(), VAR_NAME(color_short), color_short);
    ini->SetLongValue(Name(), "height", static_cast<long>(m_skill_height));
    ini->SetLongValue(Name(), "width", static_cast<long>(m_skill_width));
    ini->SetBoolValue(Name(), "vertical", vertical);
    ini->SetLongValue(Name(), "medium_treshold", static_cast<long>(medium_treshold));
    ini->SetLongValue(Name(), "short_treshold", static_cast<long>(short_treshold));
    ini->SetBoolValue(Name(), "effect_text", display_effect_times);
    ini->SetLongValue(Name(), "effect_offset", m_effect_offset);
}

void SkillbarWidget::DrawSettingInternal()
{
    ToolboxWidget::DrawSettingInternal();
    ImGui::SameLine();
    auto vertical_copy = vertical;
    if (ImGui::Checkbox("Vertical", &vertical_copy)) {
        vertical = vertical_copy;
    }

    if (ImGui::TreeNode("Colors")) {
        Colors::DrawSettingHueWheel("Text color", &color_text);
        Colors::DrawSettingHueWheel("Border color", &color_border);
        ImGui::TreePop();
    }
    if (ImGui::TreeNode("Effect uptime")) {
        int mediumdur = static_cast<int>(medium_treshold);
        if (ImGui::DragInt("Medium Treshold", &mediumdur)) {
            medium_treshold = clock_t{mediumdur};
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the medium color is used.");
        int shortdur = static_cast<int>(short_treshold);
        if (ImGui::DragInt("Short Treshold", &shortdur)) {
            short_treshold = clock_t{shortdur};
        }
        ImGui::ShowHelp("Number of milliseconds of effect uptime left, until the short color is used.");
        Colors::DrawSettingHueWheel("Long uptime", &color_long);
        Colors::DrawSettingHueWheel("Medium uptime", &color_medium);
        Colors::DrawSettingHueWheel("Short uptime", &color_short);
        auto display = display_effect_times;
        if (ImGui::Checkbox("Effect text", &display)) {
            display_effect_times = display;
        }
        auto distance = m_effect_offset;
        if (ImGui::DragInt("Text offset", &distance, 1, -200, 200)) {
            m_effect_offset = distance;
        }
        ImGui::ShowHelp("Whether effect uptime timers should be displayed above the skill.");
        ImGui::TreePop();
    }
    auto height = m_skill_height;
    if (ImGui::DragInt("Skill height", &height)) {
        m_skill_height = height;
    }
    auto width = m_skill_width;
    if (ImGui::DragInt("Skill width", &width)) {
        m_skill_width = width;
    }
}

Color SkillbarWidget::UptimeToColor(const clock_t uptime) const
{
    if (uptime > medium_treshold) {
        return color_long;
    }

    if (uptime > short_treshold) {
        auto const diff = static_cast<float>(medium_treshold - short_treshold);
        auto const fraction = 1 - ((medium_treshold - uptime) / diff);
        int colold[4], colnew[4], colout[4];
        Colors::ConvertU32ToInt4(color_long, colold);
        Colors::ConvertU32ToInt4(color_medium, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>(static_cast<float>(1 - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    if (uptime > 0) {
        auto const fraction = uptime / static_cast<float>(short_treshold);
        int colold[4], colnew[4], colout[4];;
        Colors::ConvertU32ToInt4(color_medium, colold);
        Colors::ConvertU32ToInt4(color_short, colnew);
        for (auto i = 0; i < 4; i++) {
            colout[i] = static_cast<int>(static_cast<float>(1 - fraction) * static_cast<float>(colnew[i]) + fraction * static_cast<float>(colold[i]));
        }
        return Colors::ConvertInt4ToU32(colout);
    }

    return 0x00000000;
}
