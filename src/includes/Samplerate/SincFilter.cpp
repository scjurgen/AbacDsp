#include "SincFilter.h"

#include "sinc_69_768.h"
#include "sinc_33_512.h"
#include "sinc_21_512.h"
#include "sinc_13_256.h"
#include "sinc_11_128.h"
#include "sinc_8_128.h"
#include "sinc_7_128.h"
#include "sinc_6_128.h"
#include "sinc_5_128.h"
#include "sinc_4_128.h"
#include "sinc_3_128.h"
#include "sinc_2_128.h"

namespace AbacadDsp
{
const std::vector<SincFilter> sincFilterSet{
    SincFilter{init_69_768},
    SincFilter{init_33_512},
    SincFilter{init_21_512},
    SincFilter{init_13_256},
    SincFilter{init_11_128},
    SincFilter{init_8_128},
    SincFilter{init_7_128},
    SincFilter{init_6_128},
    SincFilter{init_5_128},
    SincFilter{init_4_128},
    SincFilter{init_3_128},
    SincFilter{init_2_128},
};
}