// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

//a typical stream block: pointer access with as<T>(), calls, registry
#include <Pothos/Framework.hpp>
#include <algorithm>

class Scale : public Pothos::Block
{
public:
    static Pothos::Block *make(const double factor)
    {
        return new Scale(factor);
    }

    Scale(const double factor):
        _factor(factor)
    {
        this->setupInput(0, typeid(float));
        this->setupOutput(0, typeid(float));
        this->registerCall(this, POTHOS_FCN_TUPLE(Scale, factor));
        this->registerCall(this, POTHOS_FCN_TUPLE(Scale, setFactor));
    }

    double factor(void) const
    {
        return _factor;
    }

    void setFactor(const double factor)
    {
        _factor = factor;
    }

    void work(void)
    {
        const size_t N = this->workInfo().minElements;
        if (N == 0) return;
        auto inPort = this->input(0);
        auto outPort = this->output(0);
        const float *in = inPort->buffer();
        float *out = outPort->buffer().as<float *>();
        for (size_t i = 0; i < N; i++) out[i] = float(in[i]*_factor);
        inPort->consume(N);
        outPort->produce(N);
    }

private:
    double _factor;
};

static Pothos::BlockRegistry registerScale(
    "/api_compat/scale", &Scale::make);
