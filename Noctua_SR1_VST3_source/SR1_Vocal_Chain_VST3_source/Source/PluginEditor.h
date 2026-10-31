#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class SR1Editor : public juce::AudioProcessorEditor
{
public:
    explicit SR1Editor (SR1Processor&);
    ~SR1Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        int paramIndex = 0;
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::SliderParameterAttachment> sAtt;
        std::unique_ptr<juce::ComboBoxParameterAttachment> cAtt;
        std::unique_ptr<juce::ButtonParameterAttachment> bAtt;
    };
    struct Panel
    {
        int group = 0;
        int powerCell = -1;               // indice en cells del boton ON del panel (si existe)
        std::vector<int> cells;           // indices de los controles normales
        juce::Rectangle<int> bounds;
    };

    SR1Processor& proc;
    juce::LookAndFeel_V4 laf;
    std::vector<std::unique_ptr<Cell>> cells;
    std::vector<Panel> panels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SR1Editor)
};
