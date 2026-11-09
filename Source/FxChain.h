#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Order: saturation, chorus, flangus, delay, reverb, volume, limiter.
class FxChain
{
public:
    struct Settings
    {
        float drive = 0, saturation = 0;   // drive 0..100, amounts 0..1
        float chorus = 0, flangus = 0;
        float delay = 0, delaySeconds = .25f;
        float reverb = 0, reverbSize = .5f; // size 0..1
        float gainDb = 0;
    };

    void prepare (double rate, int maxBlock)
    {
        sampleRate = rate;
        chorusLine.prepare (rate, .06);
        for (auto& line : flangusLines) line.prepare (rate, .02);
        echoLine.prepare (rate, 2.1);
        reverb.setSampleRate (rate);
        reverb.reset();
        reverbInput.setSize (2, maxBlock, false, false, true);
        limiter.prepare ({ rate, (juce::uint32) maxBlock, 2 });
        limiter.setThreshold (-1.f);
        limiter.setRelease (80.f);
        limiter.reset();
        for (auto* s : { &saturationMix, &chorusMix, &flangusMix, &delayMix, &reverbMix, &gain })
            s->reset (rate, .015);
        echoTime = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, const Settings& s)
    {
        saturationMix.setTargetValue (s.saturation);
        chorusMix.setTargetValue (s.chorus);
        flangusMix.setTargetValue (s.flangus * .7f);
        delayMix.setTargetValue (s.delay);
        reverbMix.setTargetValue (s.reverb);
        gain.setTargetValue (juce::Decibels::decibelsToGain (s.gainDb));

        juce::Reverb::Parameters rp;
        rp.roomSize = .3f + s.reverbSize * .66f;
        rp.damping = .45f;
        rp.wetLevel = .6f;
        rp.dryLevel = 0;
        rp.width = 1;
        reverb.setParameters (rp);

        const float k = 1 + s.drive * .28f, norm = std::tanh (k);
        const float echoTarget = s.delaySeconds * (float) sampleRate;
        if (echoTime <= 0) echoTime = echoTarget;

        const int count = buffer.getNumSamples();
        reverbInput.setSize (2, count, false, false, true);
        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getWritePointer (1);
        auto* sendL = reverbInput.getWritePointer (0);
        auto* sendR = reverbInput.getWritePointer (1);

        for (int n = 0; n < count; ++n)
        {
            float l = left[n], r = right[n];

            const float sat = saturationMix.getNextValue();
            l += sat * (std::tanh (l * k) / norm - l);
            r += sat * (std::tanh (r * k) / norm - r);

            // Chorus: two slowly moving taps, opposite on each side.
            const float cho = chorusMix.getNextValue();
            chorusLine.write (l, r);
            chorusPhase = advance (chorusPhase, 1.1f);
            const float wobble = .003f * (float) sampleRate * std::sin (chorusPhase);
            const float base = .02f * (float) sampleRate;
            const float choL = chorusLine.readLeft (base + wobble), choR = chorusLine.readRight (base - wobble);
            l += cho * choL;
            r += cho * choR;

            // Flangus: four short resonant delays drifting at different speeds.
            const float fla = flangusMix.getNextValue();
            float flaL = 0, flaR = 0;
            for (size_t v = 0; v < flangusLines.size(); ++v)
            {
                flangusPhase[v] = advance (flangusPhase[v], .22f + (float) v * .03f);
                const float t = (.003f + (float) v * .0012f + .0018f * (1 + std::sin (flangusPhase[v])) * .5f) * (float) sampleRate;
                const float a = flangusLines[v].readLeft (t), b = flangusLines[v].readRight (t);
                flangusLines[v].write (l + a * .5f, r + b * .5f);
                flaL += a;
                flaR += b;
            }
            l += fla * flaL * .5f;
            r += fla * flaR * .5f;

            // Delay, crossing sides on each repeat.
            const float dly = delayMix.getNextValue();
            echoTime += (echoTarget - echoTime) * .0005f;
            const float echoL = echoLine.readLeft (echoTime), echoR = echoLine.readRight (echoTime);
            echoLine.write (l * dly + echoR * .38f, r * dly + echoL * .38f);

            const float rev = reverbMix.getNextValue();
            sendL[n] = (l + echoL) * rev;
            sendR[n] = (r + echoR) * rev;

            const float g = gain.getNextValue();
            left[n] = (l + echoL) * g;
            right[n] = (r + echoR) * g;
        }

        reverb.processStereo (sendL, sendR, count);
        const float g = gain.getCurrentValue();
        buffer.addFrom (0, 0, sendL, count, g);
        buffer.addFrom (1, 0, sendR, count, g);

        juce::dsp::AudioBlock<float> block (buffer);
        limiter.process (juce::dsp::ProcessContextReplacing<float> (block));
    }

private:
    struct StereoLine
    {
        void prepare (double rate, double seconds)
        {
            size = (int) (rate * seconds) + 2;
            left.assign ((size_t) size, 0.f);
            right.assign ((size_t) size, 0.f);
            writeAt = 0;
        }
        void write (float l, float r)
        {
            left[(size_t) writeAt] = l;
            right[(size_t) writeAt] = r;
            writeAt = (writeAt + 1) % size;
        }
        float readLeft (float delay) const { return read (left, delay); }
        float readRight (float delay) const { return read (right, delay); }
        float read (const std::vector<float>& line, float delay) const
        {
            float at = (float) writeAt - juce::jlimit (1.f, (float) size - 2, delay);
            if (at < 0) at += (float) size;
            const int i = (int) at, j = (i + 1) % size;
            const float f = at - (float) i;
            return line[(size_t) i] + f * (line[(size_t) j] - line[(size_t) i]);
        }
        std::vector<float> left, right;
        int size = 2, writeAt = 0;
    };

    float advance (float phase, float hz) const
    {
        phase += juce::MathConstants<float>::twoPi * hz / (float) sampleRate;
        return phase > juce::MathConstants<float>::twoPi ? phase - juce::MathConstants<float>::twoPi : phase;
    }

    double sampleRate = 48000;
    StereoLine chorusLine, echoLine;
    std::array<StereoLine, 4> flangusLines;
    float chorusPhase = 0, echoTime = 0;
    std::array<float, 4> flangusPhase { 0.f, 1.57f, 3.14f, 4.71f };
    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbInput;
    juce::dsp::Limiter<float> limiter;
    juce::LinearSmoothedValue<float> saturationMix, chorusMix, flangusMix, delayMix, reverbMix, gain;
};
