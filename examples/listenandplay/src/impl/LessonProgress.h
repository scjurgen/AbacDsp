#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "LessonCatalog.h"

struct LessonRecord
{
    float best{0.f};
    float last{0.f};
    uint32_t attempts{0};
    bool passed{false};
};

// Best score and attempts per lesson, which lesson was open last, and what follows from them:
// a lesson opens when the one before it in its group has been passed.
class LessonProgress
{
  public:
    void record(const std::string& stem, const float score, const bool passed)
    {
        auto& entry = m_records[stem];
        entry.best = std::max(entry.best, score);
        entry.last = score;
        entry.passed = entry.passed || passed;
        ++entry.attempts;
    }

    [[nodiscard]] const LessonRecord* find(const std::string& stem) const
    {
        const auto it = m_records.find(stem);
        return it == m_records.end() ? nullptr : &it->second;
    }

    void setLastLesson(std::string stem)
    {
        m_lastLesson = std::move(stem);
    }

    [[nodiscard]] const std::string& lastLesson() const noexcept
    {
        return m_lastLesson;
    }

    [[nodiscard]] bool isPassed(const std::string& stem) const
    {
        const auto* entry = find(stem);
        return entry != nullptr && entry->passed;
    }

    [[nodiscard]] bool isUnlocked(const std::vector<LessonInfo>& lessons, const size_t index) const
    {
        for (size_t i = index; i-- > 0;)
        {
            if (lessons[i].stem[0] == lessons[index].stem[0])
            {
                return isPassed(lessons[i].stem);
            }
        }
        return true;
    }

    // The first lesson that is open and not passed yet, in curriculum order.
    [[nodiscard]] std::optional<size_t> recommended(const std::vector<LessonInfo>& lessons) const
    {
        for (size_t i = 0; i < lessons.size(); ++i)
        {
            if (isUnlocked(lessons, i) && !isPassed(lessons[i].stem))
            {
                return i;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::string toJson() const
    {
        nlohmann::json root;
        root["lastLesson"] = m_lastLesson;
        auto& lessons = root["lessons"] = nlohmann::json::object();
        for (const auto& [stem, entry] : m_records)
        {
            lessons[stem] = {
                {"best", entry.best}, {"last", entry.last}, {"attempts", entry.attempts}, {"passed", entry.passed}};
        }
        return root.dump();
    }

    // A text that is not valid progress leaves everything as it was.
    bool fromJson(const std::string_view text)
    {
        const auto root = nlohmann::json::parse(text, nullptr, false);
        if (!root.is_object())
        {
            return false;
        }
        m_records.clear();
        m_lastLesson = root.value("lastLesson", std::string{});
        if (const auto it = root.find("lessons"); it != root.end() && it->is_object())
        {
            for (const auto& [stem, entry] : it->items())
            {
                m_records[stem] = {entry.value("best", 0.f), entry.value("last", 0.f), entry.value("attempts", 0u),
                                   entry.value("passed", false)};
            }
        }
        return true;
    }

  private:
    std::map<std::string, LessonRecord> m_records;
    std::string m_lastLesson;
};
