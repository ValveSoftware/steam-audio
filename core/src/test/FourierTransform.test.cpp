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

#include <catch.hpp>

#include <array.h>
#include <fft.h>


TEST_CASE("FourierTransform computed correctly.", "[FourierTransform]")
{
    ipl::FFT fourierTransform(20);
    REQUIRE(fourierTransform.numRealSamples == 32);
    REQUIRE(fourierTransform.numComplexSamples == 17);

    ipl::Array<float> signal(fourierTransform.numRealSamples);
    memset(signal.flatData(), 0, signal.totalSize() * sizeof(float));
    for (auto i = 0; i < 20; ++i)
    {
        signal[i] = i + 1.0f;
    }

    ipl::Array<ipl::complex_t> spectrum(fourierTransform.numComplexSamples);

    fourierTransform.applyForward(signal.data(), spectrum.data());

    REQUIRE(spectrum[0].real() == Approx(210.0));
    REQUIRE(spectrum[1].real() == Approx(-111.881));
    REQUIRE(spectrum[2].real() == Approx(46.7185));
    REQUIRE(spectrum[3].real() == Approx(-32.2693));
    REQUIRE(spectrum[4].real() == Approx(7.58579));
    REQUIRE(spectrum[5].real() == Approx(6.63635));
    REQUIRE(spectrum[6].real() == Approx(-16.0243));
    REQUIRE(spectrum[7].real() == Approx(15.9111));
    REQUIRE(spectrum[8].real() == Approx(-10.0));
    REQUIRE(spectrum[9].real() == Approx(1.11718));
    REQUIRE(spectrum[10].real() == Approx(7.15426));
    REQUIRE(spectrum[11].real() == Approx(-10.9873));
    REQUIRE(spectrum[12].real() == Approx(10.4142));
    REQUIRE(spectrum[13].real() == Approx(-4.75235));
    REQUIRE(spectrum[14].real() == Approx(-1.84847));
    REQUIRE(spectrum[15].real() == Approx(8.22497));
    REQUIRE(spectrum[16].real() == Approx(-10.0));

    REQUIRE(spectrum[0].imag() == Approx(0.0));
    REQUIRE(spectrum[1].imag() == Approx(-69.4845));
    REQUIRE(spectrum[2].imag() == Approx(1.41779));
    REQUIRE(spectrum[3].imag() == Approx(17.5007));
    REQUIRE(spectrum[4].imag() == Approx(-26.5563));
    REQUIRE(spectrum[5].imag() == Approx(19.5842));
    REQUIRE(spectrum[6].imag() == Approx(-10.4383));
    REQUIRE(spectrum[7].imag() == Approx(-2.6708));
    REQUIRE(spectrum[8].imag() == Approx(10.0));
    REQUIRE(spectrum[9].imag() == Approx(-13.6324));
    REQUIRE(spectrum[10].imag() == Approx(9.8043));
    REQUIRE(spectrum[11].imag() == Approx(-3.49605));
    REQUIRE(spectrum[12].imag() == Approx(-4.55635));
    REQUIRE(spectrum[13].imag() == Approx(9.33214));
    REQUIRE(spectrum[14].imag() == Approx(-10.3396));
    REQUIRE(spectrum[15].imag() == Approx(6.46562));
    REQUIRE(spectrum[16].imag() == Approx(0.0));
}

TEST_CASE("IFFT(FFT(x)) == x.", "[FourierTransform]")
{
    ipl::FFT fourierTransform(20);
    ipl::Array<float> signal(fourierTransform.numRealSamples);
    ipl::Array<ipl::complex_t> spectrum(fourierTransform.numComplexSamples);
    ipl::Array<float> signal2(fourierTransform.numRealSamples);

    memset(signal.flatData(), 0, signal.totalSize() * sizeof(float));
    for (auto i = 0; i < 20; ++i)
    {
        signal[i] = i + 1.0f;
    }

    fourierTransform.applyForward(signal.data(), spectrum.data());
    fourierTransform.applyInverse(spectrum.data(), signal2.data());

    for (auto i = 0; i < fourierTransform.numRealSamples; ++i)
        REQUIRE(signal2[i] == Approx(signal[i]));
}