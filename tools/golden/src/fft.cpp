// Golden data for core/src/core/fft.{h,cpp} (PFFFT backend).

#include <fft.h>

#include "generators.h"
#include "npy.h"

namespace {

using namespace golden;
using complex_t = std::complex<float>;

void generateFFT(const std::filesystem::path& out)
{
    // 20 is not a power of two: FFT rounds up and the caller zero-pads (as in FourierTransform.test.cpp).
    for (int size : {20, 32, 256, 1024, 4096})
    {
        const auto name = std::to_string(size);

        ipl::FFT real(size, ipl::FFTDomain::Real);
        Lcg lcg(static_cast<uint32_t>(size));
        std::vector<float> signal(real.numRealSamples, 0.0f);
        for (int i = 0; i < size; ++i)
            signal[i] = lcg.nextSigned();

        std::vector<complex_t> spectrum(real.numComplexSamples);
        real.applyForward(signal.data(), spectrum.data());
        writeNpy(out / ("real_forward_" + name + ".npy"), spectrum);

        std::vector<float> roundTrip(real.numRealSamples);
        real.applyInverse(spectrum.data(), roundTrip.data());
        writeNpy(out / ("real_inverse_" + name + ".npy"), roundTrip);

        ipl::FFT complex(size, ipl::FFTDomain::Complex);
        std::vector<complex_t> complexSignal(complex.numComplexSamples, complex_t(0.0f, 0.0f));
        for (int i = 0; i < size; ++i)
        {
            const float re = lcg.nextSigned();
            complexSignal[i] = complex_t(re, lcg.nextSigned());
        }

        std::vector<complex_t> complexSpectrum(complex.numComplexSamples);
        complex.applyForward(complexSignal.data(), complexSpectrum.data());
        writeNpy(out / ("complex_forward_" + name + ".npy"), complexSpectrum);

        std::vector<complex_t> complexRoundTrip(complex.numComplexSamples);
        complex.applyInverse(complexSpectrum.data(), complexRoundTrip.data());
        writeNpy(out / ("complex_inverse_" + name + ".npy"), complexRoundTrip);
    }
}

Register fft("fft", generateFFT);

}
