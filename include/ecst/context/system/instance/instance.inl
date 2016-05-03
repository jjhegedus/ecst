// Copyright (c) 2015-2016 Vittorio Romeo
// License: Academic Free License ("AFL") v. 3.0
// AFL License page: http://opensource.org/licenses/AFL-3.0
// http://vittorioromeo.info | vittorio.romeo@outlook.com

#pragma once

#include <ecst/hardware.hpp>
#include "./instance.hpp"

ECST_CONTEXT_SYSTEM_NAMESPACE
{
    template <typename TSettings, typename TSystemSignature>
    instance<TSettings, TSystemSignature>::instance()
        : _bitset{
              bitset::make_from_system_signature<TSystemSignature>(TSettings{})}
    {
        ELOG(                                                          // .
            debug::lo_system_bitset() << "(" << system_id()            // .
                                      << ") bitset: " << _bitset.str() // .
                                      << "\n";                         // .
            );
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TF>
    decltype(auto) instance<TSettings, TSystemSignature>::for_states(TF && f)
    {
        return _sm.for_states(FWD(f));
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TProxy>
    void instance<TSettings, TSystemSignature>::execute_deferred_fns(
        TProxy & proxy)
    {
        for_states([&proxy](auto& state)
            {
                state._state.execute_deferred_fns(proxy);
            });
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TF>
    decltype(auto) instance<TSettings, TSystemSignature>::for_outputs(TF && f)
    {
        return for_states([this, &f](auto& state)
            {
                f(_system, state.as_data());
            });
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TAcc, typename TF>
    auto instance<TSettings, TSystemSignature>::foldl_outputs(TAcc acc, TF && f)
    {
        for_outputs([&acc, xf = FWD(f) ](auto& system, auto& state_data)
            {
                xf(system, acc, state_data);
            });

        return acc;
    }


    template <typename TSettings, typename TSystemSignature>
    auto instance<TSettings, TSystemSignature>::is_subscribed(entity_id eid)
        const noexcept
    {
        return subscribed().has(eid);
    }

    template <typename TSettings, typename TSystemSignature>
    auto instance<TSettings, TSystemSignature>::subscribe(entity_id eid)
    {
        // TODO: callback, optional subscription function that returns bool to
        // ignore
        return subscribed().add(eid);
    }

    template <typename TSettings, typename TSystemSignature>
    auto instance<TSettings, TSystemSignature>::unsubscribe(entity_id eid)
    {
        // TODO: callback, optional subscription function that returns bool to
        // ignore
        return subscribed().erase(eid);
    }

    template <typename TSettings, typename TSystemSignature>
    template <                          // .
        typename TFEntityProvider,      // .
        typename TFAllEntityProvider,   // .
        typename TFOtherEntityProvider, // .
        typename TContext,              // .
        typename TFStateGetter          // .
        >
    auto instance<TSettings, TSystemSignature>::make_data( // .
        TFEntityProvider && f_ep,                          // .
        TFAllEntityProvider && f_aep,                      // .
        TFOtherEntityProvider && f_oep,                    // .
        TContext & ctx,                                    // .
        TFStateGetter && sg                                // .
        )
    {
        auto make_entity_id_adapter = [](auto&& f)
        {
            return [f = FWD(f)](auto&& g)
            {
                return f([g = FWD(g)](auto id)
                    {
                        return g(entity_id(id));
                    });
            };
        };

        return impl::make_execute_data<TSystemSignature> // .
            (                                            // .
                ctx,                                     // .
                make_entity_id_adapter(FWD(f_ep)),       // .
                make_entity_id_adapter(FWD(f_aep)),      // .
                make_entity_id_adapter(FWD(f_oep)),      // .
                FWD(sg)                                  // .
                );
    }


    template <typename TSettings, typename TSystemSignature>
    template <typename TF>
    void instance<TSettings, TSystemSignature>::prepare_and_wait_n_subtasks(
        sz_t n, TF && f)
    {
        _sm.clear_and_prepare(n);
        counter_blocker b{n};

        // Runs the parallel executor and waits until the remaining subtasks
        // counter is zero.
        execute_and_wait_until_counter_zero(b, [this, &b, &f]
            {
                f(b);
            });
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TCounterBlocker, typename TData, typename TF>
    void instance<TSettings,
        TSystemSignature>::execute_subtask_and_decrement_counter( // .
        TCounterBlocker & cb, TData & data, TF && f               // .
        )
    {
        f(_system, data);
        decrement_cv_counter_and_notify_all(cb);
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TContext, typename TF>
    void instance<TSettings, TSystemSignature>::execute_single( // .
        TContext & ctx, TF && f                                 // .
        )
    {
        _sm.clear_and_prepare(1);

        // TODO: refactor
        auto data = make_data(          // .
            make_all_entity_provider(), // .
            make_all_entity_provider(), // .
            [](auto&&...)
            {
            },
            ctx,
            [this]() -> decltype(auto)
            {
                return _sm.get(0);
            });

        f(_system, data);
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TContext, typename TF>
    void instance<TSettings, TSystemSignature>::execute_in_parallel( // .
        TContext & ctx, TF && f                                      // .
        )
    {
        // Aggregates synchronization primitives.
        _parallel_executor.execute(*this, ctx, f);
    }

    template <typename TSettings, typename TSystemSignature>
    template <typename TContext, typename TF>
    void instance<TSettings, TSystemSignature>::execute( // .
        TContext & ctx, TF && f                          // .
        )
    {
        static_if(settings::inner_parallelism_allowed<TSettings>())
            .then([this, &f](auto& xsp)
                {
                    execute_in_parallel(xsp, f);
                })
            .else_([this, &f](auto& xsp)
                {
                    execute_single(xsp, f);
                })(ctx);
    }
}
ECST_CONTEXT_SYSTEM_NAMESPACE_END