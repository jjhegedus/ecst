// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#pragma once

#include <random>
#include <iostream>
#include <chrono>
#include <ecst/mp.hpp>
#include <ecst/settings.hpp>
#include <ecst/context.hpp>

namespace test
{
    namespace impl
    {
        template <typename TEntityCount, typename TCSL, typename TSSL>
        auto make_settings_list(TEntityCount ec, TCSL csl, TSSL ssl)
        {
            namespace cs = ecst::settings;
            namespace ss = ecst::scheduler;
            namespace mp = ecst::mp;

            (void)csl;
            (void)ssl;

            // List of threading policies.
            constexpr auto l_threading = mp::list::make( // .
                cs::multithreaded(                       // .
                    cs::allow_inner_parallelism          // .
                    ),                                   // .
                cs::multithreaded(                       // .
                    cs::disallow_inner_parallelism       // .
                    )                                    // .
                );

            // List of storage policies.
            constexpr auto l_storage = mp::list::make( // .
                cs::dynamic<500>,                      // .
                cs::fixed<decltype(ec){}>              // .
                );

            (void)l_threading;
            (void)l_storage;

            return mp::list::fold_l(mp::list::empty_v,
                [=](auto xacc, auto x_threading)
                {
                    auto fold2 = mp::list::fold_l(mp::list::empty_v,
                        [=](auto yacc, auto y_storage)
                        {
                            auto zsettings = cs::make(              // .
                                x_threading,                        // .
                                y_storage,                          // .
                                csl,                                // .
                                ssl,                                // .
                                cs::scheduler<ss::s_atomic_counter> // .
                                );

                            return mp::list::append(yacc, zsettings);
                        },
                        l_storage);

                    return mp::list::cat(xacc, fold2);
                },
                l_threading);
        }

        template <typename TSettings>
        auto make_ecst_context(TSettings)
        {
            return ecst::context::make(TSettings{});
        }

        template <typename TSettings, typename TF>
        void do_test(TSettings, TF&& f)
        {
            // Create context.
            using context_type = decltype(make_ecst_context(TSettings{}));
            auto ctx_uptr = std::make_unique<context_type>();
            auto& ctx = *ctx_uptr;

            f(ctx);
        }
    }

    template <typename TF, typename TEntityCount, typename TCSL, typename TSSL>
    void run_tests(TF&& f, TEntityCount ec, TCSL csl, TSSL ssl)
    {
        using vrm::core::sz_t;
        constexpr sz_t times = 2;

        using hr = std::chrono::high_resolution_clock;
        using d_type = std::chrono::duration<float, std::milli>;


        for(sz_t t = 0; t < times; ++t)
        {
            vrm::core::for_tuple(
                [f](auto s)
                {
                    std::cout
                        << ecst::settings::str::entity_storage<decltype(s)>()
                        << "\n"
                        << ecst::settings::str::multithreading<decltype(s)>()
                        << "\n";

                    auto last(hr::now());
                    impl::do_test(s, f);

                    auto time(hr::now() - last);
                    auto timeMs(
                        std::chrono::duration_cast<d_type>(time).count());

                    std::cout << "time: " << timeMs << "ms\n\n";
                },
                impl::make_settings_list(ec, csl, ssl));
        }
    }
}