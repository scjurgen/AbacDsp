#include "Samplerate/SrConverter.h"
#include "AudioFile.h"
#include "SaveAsWave.h"
#include "FftSmall.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include "Samplerate/sinc_69_768.h"
#include "Samplerate/sinc_33_512.h"
#include "Samplerate/sinc_21_512.h"
#include "Samplerate/sinc_13_256.h"
#include "Samplerate/sinc_11_128.h"
#include "Samplerate/sinc_8_128.h"
#include "Samplerate/sinc_7_128.h"
#include "Samplerate/sinc_6_128.h"
#include "Samplerate/sinc_5_128.h"
#include "Samplerate/sinc_4_128.h"
#include "Samplerate/sinc_3_128.h"
#include "Samplerate/sinc_2_128.h"

namespace AbacDsp
{
    const std::vector<SincFilter> sincFilterSet{
        SincFilter{init_69_768}, SincFilter{init_33_512}, SincFilter{init_21_512}, SincFilter{init_13_256},
        SincFilter{init_11_128}, SincFilter{init_8_128}, SincFilter{init_7_128}, SincFilter{init_6_128},
        SincFilter{init_5_128}, SincFilter{init_4_128}, SincFilter{init_3_128}, SincFilter{init_2_128},
    };
}

#define BUFFER_LEN 512

typedef struct
{
    AudioFile<float> audioFile;
    size_t currentIndex;
    float m_buffer[BUFFER_LEN * 2];
} AudioCbData;

typedef struct
{
    AudioCbData sf;
    int freq_point;
    AbacDsp::SrConverter srConverter;
} SrConverterCbData;

static SrConverterCbData dataProvider{};

static long sampleRateConverterInputCb(void* cb_data, float** audio)
{
    AudioCbData* data = (AudioCbData*)cb_data;
    int readFrames = 0;
    *audio = &data->m_buffer[0];
    if (data->currentIndex >= data->audioFile.getNumSamplesPerChannel())
    {
        return 0;
    }
    for (size_t i = 0; i < BUFFER_LEN; i += 2)
    {
        data->m_buffer[i] = data->audioFile.samples[0][data->currentIndex];
        data->m_buffer[i + 1] = data->audioFile.samples[0][data->currentIndex];
        data->currentIndex++;
        readFrames++;
        if (data->currentIndex >= data->audioFile.getNumSamplesPerChannel())
        {
            break;
        }
    }
    return readFrames;
}

#define VARISPEED_BLOCK_LEN 64

static int varPitchGetData(float* samples, int outFrames)
{
    for (size_t outFrameIndex = 0; outFrameIndex < outFrames; outFrameIndex += VARISPEED_BLOCK_LEN)
    {
        // double pitchRatio = 2.0 - 1.75 * sin(dataProvider.freq_point * 2 * M_PI / 200);
        const auto pitchRatio = 1.2f; // > 1 slow down
        dataProvider.freq_point++;
        float* output = samples + outFrameIndex * 2;
        int rc = dataProvider.srConverter.fetchBlock(pitchRatio, output, VARISPEED_BLOCK_LEN, 2);
        if (rc < VARISPEED_BLOCK_LEN)
        {
            return 0;
        }
    }
    return outFrames;
}


bool audioIsIdentical(const std::string& lhsSampleFilename, const std::string& rhsSampleFilename)
{
    std::cout << "\ncomparing " << lhsSampleFilename << " vs " << rhsSampleFilename << std::endl;
    const float EPSILON = 1e-8f;
    AudioFile<float> lhs;
    if (!lhs.load(lhsSampleFilename))
    {
        std::cerr << "Error loading file: " << lhsSampleFilename << std::endl;
        return false;
    }

    AudioFile<float> rhs;
    if (!rhs.load(rhsSampleFilename))
    {
        std::cerr << "Error loading file: " << rhsSampleFilename << std::endl;
        return false;
    }

    if (lhs.getNumChannels() != rhs.getNumChannels())
    {
        std::cerr << "Number of channels do not match." << std::endl;
        return false;
    }

    if (lhs.getNumSamplesPerChannel() != rhs.getNumSamplesPerChannel())
    {
        std::cerr << "Number of samples per channel do not match." << std::endl;
        return false;
    }

    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
    {
        for (int sampleIndex = 0; sampleIndex < lhs.getNumSamplesPerChannel(); ++sampleIndex)
        {
            if (std::fabs(lhs.samples[channel][sampleIndex] - rhs.samples[channel][sampleIndex]) > EPSILON)
            {
                std::cerr << "Samples differ at channel " << channel << ", index " << sampleIndex << std::endl;
                return false;
            }
        }
    }

    return true;
}


void Usage(const std::string& program)
{
    std::cerr << std::format("Usage: {}"
                             " --infile <input.wav>"
                             " --outfile <output.wav>"
                             " --ratio <ratio>"
                             " --sincindex <index>",
                             " --compare <compare.wav>", program)
        << std::endl;
}


int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        Usage(argv[0]);
        return 1;
    }

    std::string infile;
    std::string comparefile = "compare.wav";
    std::string outfile = "result.wav";
    float ratio = 1.0f;
    int sincindex = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--infile" && i + 1 < argc)
        {
            infile = argv[++i];
        }
        else if (arg == "--outfile" && i + 1 < argc)
        {
            outfile = argv[++i];
        }
        else if (arg == "--compare" && i + 1 < argc)
        {
            comparefile = argv[++i];
        }
        else if (arg == "--ratio" && i + 1 < argc)
        {
            try
            {
                ratio = std::stof(argv[++i]);
                if (ratio < 1.0f / AbacDsp::SrConvertMaxRatio || ratio > AbacDsp::SrConvertMaxRatio)
                {
                    throw std::out_of_range("Ratio out of range.");
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << std::format("Error: Invalid ratio value. {}\n", e.what());
                return 1;
            }
        }
        else if (arg == "--sincindex" && i + 1 < argc)
        {
            try
            {
                sincindex = std::stoi(argv[++i]);
                if (sincindex < 0)
                {
                    throw std::out_of_range("Sincindex must be non-negative.");
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << std::format("Error: Invalid sincindex value. {}\n", e.what());
                return 1;
            }
        }
        else
        {
            std::cerr << std::format("Error: Unknown or incomplete argument: {}\n", arg);
            Usage(argv[0]);
            return 1;
        }
    }

    if (infile.empty())
    {
        std::cerr << "Error: --infile is required.\n";
        Usage(argv[0]);
        return 1;
    }


    std::cout << std::format("Processing with infile: {}, outfile: {}, ratio: {}, sincindex: {}\n", infile, outfile,
                             ratio, sincindex);


    if (!dataProvider.sf.audioFile.load(infile))
    {
        std::cerr << "Failed to load audio file!" << std::endl;
        return 1;
    }
    dataProvider.srConverter.setCallback(sampleRateConverterInputCb, &dataProvider);
    dataProvider.srConverter.setCoeffsIndex(sincindex);
    std::vector<float> total(0);
    std::vector<float> result(4096);
    auto start = std::chrono::high_resolution_clock::now();
    while (varPitchGetData(result.data(), result.size() / 2) != 0)
    {
        total.insert(total.end(), result.begin(), result.end());
    }
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);

    // Print the duration
    std::cout << "Execution time: " << duration.count() << " ms\n";
    std::cout << "saving result: " << total.size() << " frames\n";
    saveWavFile(outfile, 48000, total, 2);

    // check only if refactoring: N.B.: use SincCoeffs.h and fixed pitchfactor 1.2
    // this checks the two files on a binary level... crooked
    if (!audioIsIdentical(outfile, comparefile))
    {
        std::cerr << "\n\n##### ERROR! #####\n\nfiles are different!!!!\n";
    }
    // print spectrogram for visual check
    {
        std::vector<float> mono;
        for (size_t i = 0; i < total.size(); i += 2)
        {
            mono.push_back(total[i]);
        }
        Spectrogram spectrogram;
        spectrogram.generate("Test2", mono, Spectrogram::Colormap::Viridis, 4096, 0.2, 1024);
    }
    return 0;
}