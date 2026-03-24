#include "SincFilter.h"

#include "sinc_69.h"
#include "sinc_33.h"
#include "sinc_21.h"
#include "sinc_13.h"
#include "sinc_11.h"
#include "sinc_8.h"
#include "sinc_7.h"
#include "sinc_6.h"
#include "sinc_5.h"
#include "sinc_4.h"
#include "sinc_3.h"
#include "sinc_2.h"

namespace AbacDsp
{
const std::vector sincFilterSet{
    SincFilter{init_69},
    SincFilter{init_33},
    SincFilter{init_21},
    SincFilter{init_13},
    SincFilter{init_11},
    SincFilter{init_8},
    SincFilter{init_7},
    SincFilter{init_6},
    SincFilter{init_5},
    SincFilter{init_4},
    SincFilter{init_3},
    SincFilter{init_2},
};
}