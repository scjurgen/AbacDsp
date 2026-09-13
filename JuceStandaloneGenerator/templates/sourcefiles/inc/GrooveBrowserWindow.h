#pragma once

#include <cmath>
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>

#include "AppSettings.h"
#include "GuiConstants.h"

// One parsed groove row - see parse()'s own doc comment for the wire format.
struct GrooveBrowserRowInfo
{
    juce::String styleName;
    juce::String folderName;
    juce::String displayName;
    float idealBpm{0.f};
    juce::String feel;
    juce::String timeSignature;
    int bars{0};
    int variationCount{0};
    juce::String instruments;

    // "<styleName>|<folderName>|<displayName>|<idealBpm>|<feel>|<timeSignature>|
    // <bars>|<variationCount>|<sound1,sound2,...>" - see TapeLooperImpl::listGrooveInfoRows().
    [[nodiscard]] static GrooveBrowserRowInfo parse(const juce::String& row)
    {
        const auto fields = juce::StringArray::fromTokens(row, "|", "");
        GrooveBrowserRowInfo info;
        info.styleName = fields[0];
        info.folderName = fields[1];
        info.displayName = fields[2];
        info.idealBpm = fields[3].getFloatValue();
        info.feel = fields[4];
        info.timeSignature = fields[5];
        info.bars = fields[6].getIntValue();
        info.variationCount = fields[7].getIntValue();
        info.instruments = fields[8].replace(",", ", ");
        return info;
    }
};

// A groove-browser dialog: pick a base folder (Basic/Advanced/Pro/educational/
// looper/performance), see every groove under it in a filterable table (name,
// BPM, style, length in bars, dominant instruments), then load the selected one.
// Non-modal, hosted by GrooveBrowserDialogWindow below (mirrors
// ScriptEditorWindow.h's own Window/DialogWindow pair), so the main plugin
// window stays interactive while this is open. Filtering is entirely local
// (over whichever folder's rows are currently loaded), not re-fetched per filter.
class GrooveBrowserWindow final : public juce::Component, public juce::TableListBoxModel
{
  public:
    // Returns every base folder name, in display order.
    std::function<juce::StringArray()> onListBaseFolders;
    // Returns every groove's "|"-delimited row for one base folder.
    std::function<juce::StringArray(const juce::String&)> onListGrooveInfoRows;
    // Called with (styleName, variationIndex) when Load is pressed.
    std::function<void(const juce::String&, int)> onLoadGroove;
    // The plugin's current BPM control value, for the Similar BPM filter.
    std::function<float()> onGetCurrentBpm;

    GrooveBrowserWindow()
    {
        m_folderCombo.onChange = [this] { folderChanged(); };
        addAndMakeVisible(m_folderCombo);

        m_similarBpmToggle.setButtonText("Similar BPM (+/-10)");
        m_similarBpmToggle.onClick = [this] { applyFilters(); };
        addAndMakeVisible(m_similarBpmToggle);

        m_feelCombo.addItemList({"All", "Even", "Shuffle", "Swing"}, 1);
        m_feelCombo.setSelectedId(1, juce::dontSendNotification);
        m_feelCombo.onChange = [this] { applyFilters(); };
        addAndMakeVisible(m_feelCombo);

        m_timeSignatureCombo.addItemList({"All", "4/4", "3/4", "6/8", "12/8", "Odd meters"}, 1);
        m_timeSignatureCombo.setSelectedId(1, juce::dontSendNotification);
        m_timeSignatureCombo.onChange = [this] { applyFilters(); };
        addAndMakeVisible(m_timeSignatureCombo);

        m_table.setModel(this);
        auto& header = m_table.getHeader();
        header.addColumn("Folder", kColumnFolder, 150);
        header.addColumn("Name", kColumnName, 190);
        header.addColumn("BPM", kColumnBpm, 55);
        header.addColumn("Style", kColumnStyle, 120);
        header.addColumn("Bars", kColumnBars, 45);
        header.addColumn("Var", kColumnVariations, 40);
        header.addColumn("Instruments", kColumnInstruments, 190);
        addAndMakeVisible(m_table);

        m_loadButton.setButtonText("Load");
        m_loadButton.setEnabled(false);
        m_loadButton.onClick = [this] { loadSelected(); };
        addAndMakeVisible(m_loadButton);

        m_cancelButton.setButtonText("Cancel");
        m_cancelButton.onClick = [this] { closeParentDialog(); };
        addAndMakeVisible(m_cancelButton);

        setSize(870, 520);
    }

    // Populates the folder combo and loads its first folder's grooves; call once
    // after wiring the callbacks above.
    void refresh()
    {
        m_folderCombo.clear(juce::dontSendNotification);
        if (onListBaseFolders)
        {
            const auto folders = onListBaseFolders();
            for (int i = 0; i < folders.size(); ++i)
            {
                m_folderCombo.addItem(folders[i], i + 1);
            }
        }
        if (m_folderCombo.getNumItems() > 0)
        {
            m_folderCombo.setSelectedItemIndex(0, juce::dontSendNotification);
            folderChanged();
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(8);
        auto buttonRow = area.removeFromBottom(32);
        m_cancelButton.setBounds(buttonRow.removeFromRight(90));
        buttonRow.removeFromRight(8);
        m_loadButton.setBounds(buttonRow.removeFromRight(90));
        area.removeFromBottom(4);

        auto filterRow = area.removeFromTop(28);
        m_folderCombo.setBounds(filterRow.removeFromLeft(160));
        filterRow.removeFromLeft(8);
        m_similarBpmToggle.setBounds(filterRow.removeFromLeft(160));
        filterRow.removeFromLeft(8);
        m_feelCombo.setBounds(filterRow.removeFromLeft(110));
        filterRow.removeFromLeft(8);
        m_timeSignatureCombo.setBounds(filterRow.removeFromLeft(130));
        area.removeFromTop(4);

        m_table.setBounds(area);
    }

    int getNumRows() override
    {
        return static_cast<int>(m_filteredRows.size());
    }

    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.setColour(juce::Colour(GuiConstants::instance().colors.knobGradientCenter).withAlpha(0.3f));
        }
        else
        {
            const bool banded = rowNumber >= 0 && rowNumber < static_cast<int>(m_rowBandOdd.size()) &&
                                m_rowBandOdd[static_cast<size_t>(rowNumber)];
            g.setColour(banded ? juce::Colours::grey.withAlpha(0.12f) : juce::Colours::transparentBlack);
        }
        g.fillRect(0, 0, width, height);
    }

    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height,
                   bool /*rowIsSelected*/) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(m_filteredRows.size()))
        {
            return;
        }
        g.setColour(juce::Colour(GuiConstants::instance().colors.labelColour));
        g.drawText(textForColumn(m_filteredRows[static_cast<size_t>(rowNumber)], columnId), 4, 0, width - 8, height,
                   juce::Justification::centredLeft);
    }

    void selectedRowsChanged(int lastRowSelected) override
    {
        m_loadButton.setEnabled(lastRowSelected >= 0 && lastRowSelected < static_cast<int>(m_filteredRows.size()));
    }

    void cellDoubleClicked(int /*rowNumber*/, int /*columnId*/, const juce::MouseEvent&) override
    {
        loadSelected();
    }

  private:
    static constexpr int kColumnFolder = 1;
    static constexpr int kColumnName = 2;
    static constexpr int kColumnBpm = 3;
    static constexpr int kColumnStyle = 4;
    static constexpr int kColumnBars = 5;
    static constexpr int kColumnVariations = 6;
    static constexpr int kColumnInstruments = 7;
    static constexpr float kSimilarBpmToleranceHz = 10.f;

    [[nodiscard]] static juce::String textForColumn(const GrooveBrowserRowInfo& row, const int columnId)
    {
        switch (columnId)
        {
            case kColumnFolder:
                return row.folderName;
            case kColumnName:
                return cleanDisplayName(row.displayName);
            case kColumnBpm:
                return row.idealBpm > 0.f ? juce::String(row.idealBpm, 0) : juce::String();
            case kColumnStyle:
                return row.timeSignature.isEmpty() ? row.feel : row.feel + ", " + row.timeSignature;
            case kColumnBars:
                return row.bars > 0 ? juce::String(row.bars) : juce::String();
            case kColumnVariations:
                return row.variationCount > 0 ? juce::String(row.variationCount) : juce::String();
            case kColumnInstruments:
                return row.instruments;
            default:
                return {};
        }
    }

    // Strips the "{feel}_{timesig}" prefix (already shown in Style) from names
    // that follow MidiDrums/README.md's naming convention, and spells the
    // trailing section letter out. Names that don't match pass through as-is.
    [[nodiscard]] static juce::String cleanDisplayName(const juce::String& raw)
    {
        auto tokens = juce::StringArray::fromTokens(raw, "_", "");
        tokens.removeEmptyStrings();
        if (!tokens.isEmpty() && (tokens[0] == "str" || tokens[0] == "swg" || tokens[0] == "shf"))
        {
            tokens.remove(0);
        }
        if (!tokens.isEmpty() && isTimeSignatureToken(tokens[0]))
        {
            tokens.remove(0);
        }
        if (!tokens.isEmpty())
        {
            tokens.set(tokens.size() - 1, sectionWordFor(tokens[tokens.size() - 1]));
        }
        return tokens.isEmpty() ? raw : tokens.joinIntoString(" ");
    }

    [[nodiscard]] static bool isTimeSignatureToken(const juce::String& token)
    {
        const auto hash = token.indexOfChar('#');
        return hash > 0 && token.substring(0, hash).containsOnly("0123456789") &&
               token.substring(hash + 1).containsOnly("0123456789");
    }

    [[nodiscard]] static juce::String sectionWordFor(const juce::String& token)
    {
        if (token == "c")
        {
            return "Chorus";
        }
        if (token == "v")
        {
            return "Verse";
        }
        if (token == "b")
        {
            return "Beats";
        }
        return token;
    }

    // Groups a style's section variants under the same substyle, for row
    // banding - folder-scoped so bands never span two different folders.
    [[nodiscard]] static juce::String groupKeyForRow(const GrooveBrowserRowInfo& row)
    {
        juce::String key = row.folderName + "|" + row.displayName;
        for (const auto* suffix : {"_c", "_v", "_b"})
        {
            if (key.endsWith(suffix))
            {
                return key.dropLastCharacters(2);
            }
        }
        return key;
    }

    void folderChanged()
    {
        m_allRows.clear();
        if (onListGrooveInfoRows)
        {
            for (const auto& row : onListGrooveInfoRows(m_folderCombo.getText()))
            {
                m_allRows.push_back(GrooveBrowserRowInfo::parse(row));
            }
        }
        applyFilters();
    }

    // Time-signature "Odd meters" is the catch-all for anything not exactly one
    // of the other four named buckets - including "mixed" and no time signature.
    [[nodiscard]] static bool matchesTimeSignature(const juce::String& timeSignature, const juce::String& filter)
    {
        if (filter == "All")
        {
            return true;
        }
        if (filter == "Odd meters")
        {
            return timeSignature != "4/4" && timeSignature != "3/4" && timeSignature != "6/8" &&
                   timeSignature != "12/8";
        }
        return timeSignature == filter;
    }

    void applyFilters()
    {
        const float currentBpm = onGetCurrentBpm ? onGetCurrentBpm() : 0.f;
        const bool similarBpmOnly = m_similarBpmToggle.getToggleState();
        const auto feelFilter = m_feelCombo.getText();
        const auto timeSigFilter = m_timeSignatureCombo.getText();

        m_filteredRows.clear();
        for (const auto& row : m_allRows)
        {
            if (similarBpmOnly && row.idealBpm > 0.f && std::abs(row.idealBpm - currentBpm) > kSimilarBpmToleranceHz)
            {
                continue;
            }
            if (feelFilter != "All" && !row.feel.equalsIgnoreCase(feelFilter))
            {
                continue;
            }
            if (!matchesTimeSignature(row.timeSignature, timeSigFilter))
            {
                continue;
            }
            m_filteredRows.push_back(row);
        }
        recomputeRowBands();
        m_table.updateContent();
        m_table.deselectAllRows();
        m_loadButton.setEnabled(false);
    }

    // One alternating-band flag per filtered row, toggling at every substyle
    // group boundary (see groupKeyForRow()) - computed once per filter change
    // rather than per paint call.
    void recomputeRowBands()
    {
        m_rowBandOdd.clear();
        m_rowBandOdd.reserve(m_filteredRows.size());
        juce::String previousKey;
        bool odd = false;
        for (const auto& row : m_filteredRows)
        {
            const auto key = groupKeyForRow(row);
            if (!m_rowBandOdd.empty() && key != previousKey)
            {
                odd = !odd;
            }
            previousKey = key;
            m_rowBandOdd.push_back(odd);
        }
    }

    void loadSelected()
    {
        const int row = m_table.getSelectedRow();
        if (row < 0 || row >= static_cast<int>(m_filteredRows.size()))
        {
            return;
        }
        if (onLoadGroove)
        {
            onLoadGroove(m_filteredRows[static_cast<size_t>(row)].styleName, 0);
        }
        closeParentDialog();
    }

    void closeParentDialog()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        {
            dw->closeButtonPressed();
        }
    }

    juce::ComboBox m_folderCombo;
    juce::ToggleButton m_similarBpmToggle;
    juce::ComboBox m_feelCombo;
    juce::ComboBox m_timeSignatureCombo;
    juce::TableListBox m_table;
    juce::TextButton m_loadButton;
    juce::TextButton m_cancelButton;

    std::vector<GrooveBrowserRowInfo> m_allRows;
    std::vector<GrooveBrowserRowInfo> m_filteredRows;
    std::vector<bool> m_rowBandOdd; // parallels m_filteredRows - see recomputeRowBands().
};

// Non-modal host window for GrooveBrowserWindow, mirroring
// ScriptEditorDialogWindow: setVisible(true) instead of enterModalState(),
// deletes itself on close, persists its own position/size across opens.
class GrooveBrowserDialogWindow final : public juce::DialogWindow, private juce::ComponentListener
{
  public:
    GrooveBrowserDialogWindow(const juce::String& title, const juce::Colour& backgroundColour)
        : juce::DialogWindow(title, backgroundColour, true)
    {
        addComponentListener(this);
    }

    void closeButtonPressed() override
    {
        juce::MessageManager::callAsync([this] { delete this; });
    }

    void armBoundsPersistence()
    {
        m_persistBounds = true;
    }

  private:
    void componentMovedOrResized(juce::Component&, bool, bool) override
    {
        if (m_persistBounds)
        {
            AppSettings::saveGrooveBrowserBounds(getBounds());
        }
    }

    bool m_persistBounds{false};
};
