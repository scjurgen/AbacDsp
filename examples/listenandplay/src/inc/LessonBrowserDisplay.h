#pragma once

#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

#include "../impl/DisplaySnapshots.h"
#include "ListenPlayUi.h"

/**
 * The curriculum as a table: state, lesson, focus, best score and tries. A lesson that follows one not yet
 * passed is marked locked but can still be opened; "next" marks the one the curriculum suggests. Double click
 * or Load opens the selected lesson.
 */
class LessonBrowserDisplay final : public ListenPlayUi::DisplayFrame, public juce::TableListBoxModel
{
  public:
    std::function<void(const juce::String&)> onLoad;

    LessonBrowserDisplay()
    {
        auto& header = m_table.getHeader();
        header.addColumn("State", kColState, 70, 50, 120, juce::TableHeaderComponent::notResizable);
        header.addColumn("Lesson", kColLesson, 340, 120, 600);
        header.addColumn("Focus", kColFocus, 90, 60, 160);
        header.addColumn("Best", kColBest, 60, 40, 100, juce::TableHeaderComponent::notResizable);
        header.addColumn("Tries", kColTries, 50, 40, 100, juce::TableHeaderComponent::notResizable);
        m_table.setRowHeight(kRowHeight);
        m_table.setMultipleSelectionEnabled(false);
        addAndMakeVisible(m_table);
        m_load.setButtonText("Load lesson");
        m_load.onClick = [this] { loadRow(m_table.getSelectedRow()); };
        addAndMakeVisible(m_load);
        m_next.setButtonText("Load next suggested");
        m_next.onClick = [this] { loadRow(suggestedRow()); };
        addAndMakeVisible(m_next);
    }

    // The rows are fetched only when the version differs from the one shown.
    void setRows(const uint64_t version, const std::function<std::vector<ListenPlayDisplay::LessonRow>()>& provider)
    {
        if (version == m_version)
        {
            return;
        }
        m_version = version;
        m_rows = provider();
        m_table.updateContent();
        selectCurrent();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        static_cast<void>(paintFrame(g));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttons = area.removeFromBottom(kButtonHeight);
        m_load.setBounds(buttons.removeFromLeft(buttons.getWidth() / 2).reduced(2, 0));
        m_next.setBounds(buttons.reduced(2, 0));
        area.removeFromBottom(6);
        m_table.setBounds(area);
    }

    int getNumRows() override
    {
        return static_cast<int>(m_rows.size());
    }

    void paintRowBackground(juce::Graphics& g, const int row, const int, const int, const bool selected) override
    {
        if (selected)
        {
            g.fillAll(ListenPlayUi::accentColour(3).withAlpha(0.35f));
        }
        else if (isRow(row) && m_rows[static_cast<size_t>(row)].current)
        {
            g.fillAll(ListenPlayUi::accentColour(7).withAlpha(0.18f));
        }
    }

    void paintCell(juce::Graphics& g, const int row, const int column, const int width, const int height,
                   const bool) override
    {
        if (!isRow(row))
        {
            return;
        }
        const auto& lesson = m_rows[static_cast<size_t>(row)];
        const float alpha = lesson.unlocked || lesson.passed ? 0.95f : 0.45f;
        const juce::Rectangle<float> area{4.f, 0.f, static_cast<float>(width) - 8.f, static_cast<float>(height)};
        ListenPlayUi::drawText(g, textOf(lesson, column), area, 14.f,
                               column == kColBest || column == kColTries ? juce::Justification::centredRight
                                                                         : juce::Justification::centredLeft,
                               column == kColState ? stateColour(lesson) : ListenPlayUi::textColour(alpha));
    }

    void cellDoubleClicked(const int row, const int, const juce::MouseEvent&) override
    {
        loadRow(row);
    }

  private:
    static constexpr int kColState{1};
    static constexpr int kColLesson{2};
    static constexpr int kColFocus{3};
    static constexpr int kColBest{4};
    static constexpr int kColTries{5};
    static constexpr int kRowHeight{24};
    static constexpr int kButtonHeight{28};

    [[nodiscard]] bool isRow(const int row) const noexcept
    {
        return row >= 0 && static_cast<size_t>(row) < m_rows.size();
    }

    [[nodiscard]] static juce::String stateOf(const ListenPlayDisplay::LessonRow& lesson)
    {
        if (lesson.passed)
        {
            return "passed";
        }
        if (lesson.recommended)
        {
            return "next";
        }
        return lesson.unlocked ? "open" : "locked";
    }

    [[nodiscard]] static juce::Colour stateColour(const ListenPlayDisplay::LessonRow& lesson)
    {
        if (lesson.passed)
        {
            return ListenPlayUi::zoneColour(ListenPlayUi::Zone::Ok);
        }
        if (lesson.recommended)
        {
            return ListenPlayUi::zoneColour(ListenPlayUi::Zone::Warn);
        }
        return ListenPlayUi::textColour(lesson.unlocked ? 0.8f : 0.45f);
    }

    [[nodiscard]] static juce::String textOf(const ListenPlayDisplay::LessonRow& lesson, const int column)
    {
        switch (column)
        {
            case kColState:
                return stateOf(lesson);
            case kColLesson:
                return juce::String(lesson.title);
            case kColFocus:
                return juce::String(lesson.focus);
            case kColBest:
                return lesson.attempts > 0 ? juce::String(std::lround(static_cast<double>(lesson.best))) + " %" : "-";
            case kColTries:
                return juce::String(lesson.attempts);
            default:
                return {};
        }
    }

    [[nodiscard]] int suggestedRow() const noexcept
    {
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            if (m_rows[i].recommended)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void selectCurrent()
    {
        for (size_t i = 0; i < m_rows.size(); ++i)
        {
            if (m_rows[i].current)
            {
                m_table.selectRow(static_cast<int>(i));
                return;
            }
        }
    }

    void loadRow(const int row)
    {
        if (isRow(row) && onLoad)
        {
            onLoad(juce::String(m_rows[static_cast<size_t>(row)].stem));
        }
    }

    juce::TableListBox m_table{"lessons", this};
    juce::TextButton m_load;
    juce::TextButton m_next;
    std::vector<ListenPlayDisplay::LessonRow> m_rows;
    uint64_t m_version{0};
};
