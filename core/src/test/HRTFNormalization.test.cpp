#include <catch.hpp>

#include <cmath>

#include <hrtf_database.h>

namespace
{
// Mean energy of the measured HRTFs (both ears, all measurement directions, all bins).
double directEnergy(const ipl::HRTFDatabase& hrtf)
{
    auto sum = 0.0;
    for (auto i = 0; i < hrtf.numHRIRs(); ++i)
    {
        const ipl::complex_t* spectra[2] = { nullptr, nullptr };
        hrtf.getHRTFByIndex(i, spectra);
        for (auto e = 0; e < 2; ++e)
            for (auto k = 0; k < hrtf.numSpectrumSamples(); ++k)
                sum += std::norm(spectra[e][k]);
    }
    return sum / hrtf.numHRIRs();
}

// Energy of the order-1 Ambisonics HRTFs (both ears, all bins).
double ambisonicsEnergy(const ipl::HRTFDatabase& hrtf)
{
    auto sum = 0.0;
    for (auto c = 0; c < 4; ++c)
    {
        const ipl::complex_t* spectra[2] = { nullptr, nullptr };
        hrtf.ambisonicsHRTF(c, spectra);
        for (auto e = 0; e < 2; ++e)
            for (auto k = 0; k < hrtf.numSpectrumSamples(); ++k)
                sum += std::norm(spectra[e][k]);
    }
    return sum;
}

double toDB(double energyRatio)
{
    return 10.0 * std::log10(energyRatio);
}
}

TEST_CASE("HRTF volume applies to the Ambisonics HRTFs as well as to the measured HRTFs.", "[HRTFDatabase]")
{
    ipl::HRTFSettings reference{};
    ipl::HRTFDatabase hrtf0(reference, 48000, 1024);

    ipl::HRTFSettings boosted{};
    boosted.volume = 6.0f;
    ipl::HRTFDatabase hrtf6(boosted, 48000, 1024);

    auto directDB = toDB(directEnergy(hrtf6) / directEnergy(hrtf0));
    auto ambisonicsDB = toDB(ambisonicsEnergy(hrtf6) / ambisonicsEnergy(hrtf0));
    INFO("volume +6 dB: measured HRTFs " << directDB << " dB, Ambisonics HRTFs " << ambisonicsDB << " dB");

    REQUIRE(fabs(directDB - 6.0) < 0.1);
    REQUIRE(fabs(ambisonicsDB - 6.0) < 0.1);
}

TEST_CASE("RMS normalization applies to the Ambisonics HRTFs as well as to the measured HRTFs.", "[HRTFDatabase]")
{
    ipl::HRTFSettings reference{};
    ipl::HRTFDatabase hrtfNone(reference, 48000, 1024);

    ipl::HRTFSettings normalized{};
    normalized.normType = ipl::HRTFNormType::RMS;
    ipl::HRTFDatabase hrtfRMS(normalized, 48000, 1024);

    auto directDB = toDB(directEnergy(hrtfRMS) / directEnergy(hrtfNone));
    auto ambisonicsDB = toDB(ambisonicsEnergy(hrtfRMS) / ambisonicsEnergy(hrtfNone));
    INFO("RMS normalization: measured HRTFs " << directDB << " dB, Ambisonics HRTFs " << ambisonicsDB << " dB");

    REQUIRE(fabs(ambisonicsDB - directDB) < 0.5);
}
