#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ListenAndPlayScriptEngine.h"
#include "NotePattern.h"

struct LessonInfo
{
    std::string stem;
    std::string title;
    NotePattern::Focus focus{NotePattern::Focus::Pitch};
};

// The curriculum: the library scripts named like a06-major-scale, each run once to read its name and focus.
class LessonCatalog
{
  public:
    // Group letter, two digits, a dash: a06-major-scale. Anything else in the library is a helper.
    [[nodiscard]] static bool isLessonStem(const std::string_view stem) noexcept
    {
        const auto digit = [](const char c) { return c >= '0' && c <= '9'; };
        return stem.size() > 4 && stem[0] >= 'a' && stem[0] <= 'e' && digit(stem[1]) && digit(stem[2]) &&
               stem[3] == '-';
    }

    // Groups a to d train pitch, rhythm, dynamics and all together; group e holds fixed exercises for recording.
    [[nodiscard]] static NotePattern::Focus focusOfGroup(const char group) noexcept
    {
        constexpr std::array<NotePattern::Focus, 5> kFocus{NotePattern::Focus::Pitch, NotePattern::Focus::Rhythm,
                                                           NotePattern::Focus::Dynamics, NotePattern::Focus::Combined,
                                                           NotePattern::Focus::Pitch};
        return kFocus[static_cast<size_t>(std::clamp(group - 'a', 0, 4))];
    }

    void rebuild(const std::span<const std::string> names, const ListenAndPlayScriptEngine::ImportResolver& resolver)
    {
        m_lessons.clear();
        ListenAndPlayScriptEngine engine;
        engine.setImportResolver(resolver);
        for (const auto& name : names)
        {
            if (!isLessonStem(name))
            {
                continue;
            }
            LessonInfo info{name, name, focusOfGroup(name[0])};
            const auto lookup = resolver(name);
            if (lookup.source && engine.loadPattern(*lookup.source))
            {
                info.title = engine.result().name.empty() ? name : engine.result().name;
                info.focus = engine.result().pattern.focus;
            }
            m_lessons.push_back(std::move(info));
        }
        std::ranges::sort(m_lessons, {}, &LessonInfo::stem);
    }

    [[nodiscard]] const std::vector<LessonInfo>& lessons() const noexcept
    {
        return m_lessons;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_lessons.empty();
    }

    [[nodiscard]] std::optional<size_t> indexOf(const std::string_view stem) const
    {
        const auto it = std::ranges::find(m_lessons, stem, &LessonInfo::stem);
        return it == m_lessons.end() ? std::nullopt
                                     : std::optional<size_t>{static_cast<size_t>(it - m_lessons.begin())};
    }

  private:
    std::vector<LessonInfo> m_lessons;
};
