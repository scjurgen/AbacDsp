

#include <gtest/gtest.h>
#include <cmath>
#include <random>

#include "Reverbs/FdnTankBlockDelayWalshSIMD.h"
#include "Reverbs/FdnTankBlockDelaySIMD.h"
#include "Reverbs/FdnTankBlockDelay.h"
#include "Reverbs/FdnTankRef.h"

namespace AbacDsp::Test
{
// use clinical sizes

static const std::array<unsigned, 8> primes{113, 127, 131, 137, 139, 149, 151, 157};

const std::vector expected{
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    1.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    1.f,           0.f,          0.f,           0.f,           1.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           1.f,           0.f,           1.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           1.f,          0.f,           1.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          1.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           -0.205607f,   0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           -0.39786f,    0.f,           0.f,           0.f,           -0.394206f,    0.f,
    0.f,           0.f,          0.f,           0.f,           -0.388855f,    0.f,           -0.387105f,
    0.f,           0.192252f,    0.f,           0.f,           0.f,           -0.380851f,    0.f,
    0.f,           0.f,          -0.190006f,    0.f,           -0.00145233f,  0.f,           -0.37375f,
    0.f,           0.371846f,    0.f,           -0.742187f,    0.f,           0.f,           0.f,
    -0.183248f,    0.f,          0.000503689f,  0.f,           -0.1821f,      0.f,           -0.361595f,
    0.f,           0.359944f,    0.f,           0.358736f,     0.f,           0.356245f,     0.f,
    1.06417f,      0.f,          0.352843f,     0.f,           0.f,           0.f,           -0.349731f,
    0.f,           0.347982f,    0.f,           -0.172997f,    0.f,           0.344342f,     0.f,
    -0.171345f,    0.f,          0.f,           0.f,           -0.339481f,    0.f,           -0.337829f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.166484f,     0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.0422743f,    0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.f,           0.f,
    0.f,           0.f,          0.121331f,     0.f,           0.f,           0.f,           0.119829f,
    0.f,           0.f,          0.f,           0.f,           0.f,           0.117628f,     0.f,
    0.116909f,     0.f,          -0.036961f,    0.f,           0.f,           0.f,           0.229128f,
    0.f,           0.f,          0.f,           0.0778437f,    0.f,           0.0422743f,    0.f,
    0.223478f,     0.f,          -0.0321595f,   0.f,           0.331384f,     0.f,           -0.036961f,
    0.f,           0.108934f,    0.f,           0.114419f,     0.f,           0.289879f,     0.f,
    0.212556f,     0.f,          -0.137148f,    0.f,           0.146751f,     0.f,           0.0412873f,
    0.f,           -0.257541f,   0.f,           -0.029178f,    0.f,           -0.103482f,    0.f,
    0.0691914f,    0.f,          -0.158005f,    0.f,           0.200052f,     0.f,           0.341394f,
    0.f,           -0.14131f,    0.f,           -0.09451f,     0.f,           0.42932f,      0.f,
    0.0664964f,    0.f,          -0.103366f,    0.f,           -0.191041f,    0.f,           -0.13284f,
    0.f,           0.285563f,    0.f,           0.0293412f,    0.f,           -0.147394f,    0.f,
    -0.000567302f, 0.f,          0.092759f,     0.f,           0.0978676f,    0.f,           0.0623714f,
    0.f,           -0.0634025f,  0.f,           -0.0298855f,   0.f,           -0.0271244f,   0.f,
    -0.0299279f,   0.f,          -0.057076f,    -0.00869191f,  0.117509f,     0.f,           0.0875302f,
    0.f,           -0.0592843f,  0.f,           0.0864115f,    0.f,           0.f,           0.f,
    -0.0277168f,   0.f,          -0.0277168f,   -0.0330739f,   0.f,           0.f,           0.f,
    -0.0326105f,   0.0277168f,   0.f,           0.f,           0.f,           0.f,           -0.0319319f,
    0.f,           -0.03171f,    0.f,           -0.000527896f, 0.f,           0.f,           0.f,
    -0.0930307f,   0.f,          0.f,           0.f,           -0.0312914f,   0.f,           -0.0318094f,
    0.f,           -0.0904461f,  0.f,           -0.00101427f,  0.f,           -0.11897f,     0.f,
    -0.000339795f, 0.f,          -0.0439527f,   0.f,           -0.0898104f,   0.f,           -0.0858828f,
    0.f,           -0.0855445f,  0.f,           0.0620753f,    0.f,           -0.171493f,    0.f,
    -0.000188062f, 0.f,          0.0212877f,    0.f,           -0.0145934f,   0.f,           0.0540177f,
    0.f,           -0.0853577f,  0.f,           -0.0331499f,   0.f,           -0.0576895f,   0.f,
    -0.102089f,    0.f,          0.118826f,     0.f,           0.101996f,     0.f,           -0.0619701f,
    0.f,           0.0778892f,   0.f,           0.0511433f,    0.f,           0.152634f,     0.f,
    0.243362f,     0.f,          -0.250139f,    0.f,           -0.0497045f,   0.f,           0.0222614f,
    0.f,           0.0461958f,   0.f,           0.0529537f,    0.f,           0.0792528f,    0.f,
    0.00607676f,   0.f,          0.0309366f,    0.f,           -0.0244846f,   0.f,           0.0254936f,
    0.f,           -0.0233034f,  0.f,           0.0218497f,    0.00178712f,   -0.0909826f,   0.f,
    0.0101659f,    0.f,          -0.0910069f,   0.f,           0.0126669f,    0.f,           0.0446725f,
    0.f,           0.0190756f,   0.f,           0.0875838f,    0.00847127f,   -0.000327801f, 0.f,
    0.0202342f,    0.00834424f,  0.00171665f,   0.f,           0.00144388f,   0.f,           0.00130071f,
    0.0081582f,    -0.0217669f,  0.00809737f,   0.0520002f,    0.00334208f,   -0.00267352f,  0.f,
    -0.0466308f,   0.0316349f,   0.0196296f,    0.f,           -0.0102224f,   0.0110804f,    -0.0203151f,
    0.014272f,     -0.0452901f,  0.0306955f,    0.0199804f,    0.0033391f,    0.0203014f,    0.0378075f,
    1.20327e-05f,  0.00919682f,  -0.000187589f, 0.0148884f,    -0.0194139f,   0.0450996f,    -0.000138677f,
    0.0234252f,    0.f,          0.0349664f,    0.000180526f,  -0.00901646f,  0.000134743f,  0.0921694f,
    0.f,           -0.00839147f, 0.f,           0.0188936f,    0.0046144f,    0.0229792f,    0.f,
    0.0151445f,    0.f,          0.0510854f,    0.f,           0.0386782f,    0.f,           0.0446525f,
    0.f,           0.0271938f,   0.f,           5.37656e-05f,  0.f,           -0.00795533f,  0.f,
    0.0174213f,    0.f,          -0.052535f,    0.f,           0.0121265f,    0.f,           -0.0483149f,
    0.f,           -0.0580875f,  0.f,           0.0980542f,    0.f,           0.0388509f,    0.f,
    -0.0111209f,   0.f,          0.00922347f,   0.f,           0.0792631f,    0.f,           -0.0208712f,
    0.f,           -0.021813f,   0.f,           -0.012609f,    0.f,           -0.0563968f,   0.f,
    -0.00409674f,  0.f,          -0.0171831f,   0.f,           -0.0191984f,   -0.000367444f, -0.0167705f,
    0.f,           0.0788569f,   0.f,           0.0892947f,    0.f,           0.0234423f,    0.f,
    0.00990853f,   0.f,          0.0120061f,    0.f,           -0.0136267f,   -0.00208533f,  0.00289029f,
    0.f,           -0.0306474f,  -0.00205268f,  -0.00594793f,  0.f,           0.0385762f,    0.f,
    0.0512867f,    -0.00200487f, -0.037264f,    -0.00198924f,  -0.0437994f,   -0.00167326f,  -0.0411765f,
    0.f,           -0.00910363f, -0.00970624f,  0.00610563f,   0.f,           0.0371576f,    -0.00354275f,
    0.0374252f,    -0.00513656f, 0.0222755f,    -0.00940548f,  -0.000154214f, -0.00195187f,  -0.0586699f,
    -0.0111126f,   0.0133173f,   -0.0061925f,   0.0243676f,    -0.00455614f,  0.0217986f,    -0.0182902f,
    0.013219f,     -0.00693584f, 0.010714f,     -0.0137009f,   -0.0236538f,   -0.00182502f,  -0.0200311f,
    -0.0380476f,   0.00104748f,  0.00393212f,   -0.00217145f,  -0.0137527f,   -0.0101665f,   -0.0162537f,
    -0.000541676f, -0.0138703f,  0.00964431f,   -0.0246494f,   0.00645803f,   -0.0160107f,   0.015954f,
    -0.029367f,    -0.00369315f, -0.0075823f,   -1.9491e-05f,  -0.0220955f,   0.00701975f,   -0.00391465f,
    -0.00504502f,  -0.00186206f, -0.00580487f,  0.010679f,     0.00253697f,   -0.0264414f,   0.00159473f,
    0.0101204f,    0.00250243f,  0.00469231f,   0.00579324f,   -0.0263928f,   -0.00165901f,  -0.019161f,
    -0.00317786f,  0.00647453f,  -0.00162748f,  0.000591387f,  0.f,           -0.0465854f,   0.000828331f,
    0.0269042f,    0.000813087f, 0.0429356f,    0.f,           -0.00871258f,  0.f,           0.0275369f,
    0.000768222f,  -0.0033276f,  0.f,           0.0368241f,    0.f,           0.0147087f,    7.55492e-05f,
    0.0326539f,    0.f,          -0.00394627f,  0.f,           -0.0262359f,   0.f,           0.00409446f,
    0.f,           0.023681f,
};

TEST(FdnReverb8, checkReference)
{
    constexpr size_t BlockSize{16};
    FdnTankRef<4096, 8, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setOrder(8);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes.size(); ++i)
    {
        sut.setDirectSize(i, primes[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 50; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f);
        }
    }
}

TEST(FdnReverb8, checkBlockDelay)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelay<4096, 8, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes.size(); ++i)
    {
        sut.setDirectSize(i, primes[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
        // std::cout << "0\n";
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        // std::cout << j << ":" << std::endl;
        for (float value : out)
        {
            // std::cout << value << "\n";

            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

TEST(FdnReverb8, checkSimd)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelaySIMD<4096, 8, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);


    for (size_t i = 0; i < primes.size(); ++i)
    {
        sut.setDirectSize(i, primes[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
        // std::cout << "0\n";
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

TEST(FdnReverb8, checkSimdWalsh)
{
    constexpr size_t BlockSize{16};
    FdnTankBlockDelaySIMDWalsh<4096, 8, BlockSize> sut{48000.f};
    sut.setDecay(30.f);
    sut.setMinSize(0.1);
    sut.setMaxSize(1.0);
    sut.setSpreadBulge(-0.4f);

    for (size_t i = 0; i < primes.size(); ++i)
    {
        sut.setDirectSize(i, primes[i]);
    }

    constexpr std::array<float, BlockSize> in{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    constexpr std::array<float, BlockSize> empty{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    std::array<float, BlockSize> out{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    size_t indexExpected{0};

    for (size_t j = 0; j < 16 * 2 - 1; ++j)
    {
        indexExpected++;
    }
    for (size_t j = 0; j < 48; ++j)
    {
        if (j == 0)
        {
            sut.processBlock(in.data(), out.data());
        }
        else
        {
            sut.processBlock(empty.data(), out.data());
        }
        for (float value : out)
        {
            EXPECT_NEAR(expected[indexExpected++], value, 1E-5f) << j;
        }
    }
}

}
