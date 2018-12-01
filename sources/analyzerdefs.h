//          Copyright Jean Pierre Cimalando 2018.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace Analysis {

enum {
    freq_range_min = 10,
    freq_range_max = 21000,
};

enum {
    db_range_min = -40,
    db_range_max = +40,
};

enum {
    sweep_length = 128,
};

extern float sample_rate;

[[gnu::unused]] static constexpr float amp_generator = 0.1f;
[[gnu::unused]] static constexpr float silence_threshold = 1e-4f;

}  // namespace Analysis
