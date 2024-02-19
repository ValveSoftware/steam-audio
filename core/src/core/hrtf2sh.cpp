//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "audio_buffer.h"
#include "hrtf_database.h"
using namespace ipl;

byte_t gDefaultHrtfData[] = {0};

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("USAGE: hrtf2sh <infile> <outfile>\n");
        return 0;
    }

    const char* in_file_name = argv[1];
    const char* out_file_name = argv[2];

    std::vector<uint8_t> in_data;

    // Read the entire input file into memory.
    FILE* in_file = fopen(in_file_name, "rb");
    if (in_file)
    {
        fseek(in_file, 0, SEEK_END);
        size_t size = ftell(in_file);
        fseek(in_file, 0, SEEK_SET);

        in_data.resize(size);
        fread(in_data.data(), sizeof(uint8_t), size, in_file);

        fclose(in_file);
    }
    else
    {
        printf("ERROR: Unable to open input file: %s.\n", in_file_name);
        return 1;
    }

    FILE* out_file = fopen(out_file_name, "wb");
    if (out_file)
    {
        const uint8_t* read_ptr = in_data.data();

        // Copy the FOURCC identifier from the input.
        fwrite(read_ptr, sizeof(int32_t), 1, out_file);
        read_ptr += sizeof(int32_t);

        // Write version = 3.
        int32_t new_version = 3;
        fwrite(&new_version, sizeof(int32_t), 1, out_file);
        read_ptr += sizeof(int32_t);

        // Copy the rest of the data
        fwrite(read_ptr, sizeof(uint8_t), in_data.size() - (2 * sizeof(int32_t)), out_file);

        // This program assumes that the input HRTF file contains HRIRs at
        // three sampling rates: 44100 Hz, 48000 Hz, and 24000 Hz,
        // *in that order*.
        int sampling_rates[] = {44100, 48000, 24000};

        // Project HRIRs into SH for each sampling rate.
        for (int i = 0; i < 3; ++i)
        {
            int sampling_rate = sampling_rates[i];
            printf("Projecting HRIRs for %d Hz...\n", sampling_rate);

            AudioSettings audio_settings{};
            audio_settings.samplingRate = sampling_rate;
            audio_settings.frameSize = 1024;

            HRTFSettings hrtf_settings{};
            hrtf_settings.type = HRTFMapType::Default;
            hrtf_settings.hrtfData = in_data.data();

            HRTFDatabase hrtf(hrtf_settings, audio_settings.samplingRate, audio_settings.frameSize);

            hrtf.saveAmbisonicsHRIRs(out_file);
        }

        fclose(out_file);
    }
    else
    {
        printf("ERROR: Unable to open output file: %s.\n", out_file_name);
        return 1;
    }

    return 0;
}
