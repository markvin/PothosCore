// Copyright (c) 2026 Pothos contributors
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>

/***********************************************************************
 * |PothosDoc Sibling Block
 * |category /Sibling
 * |factory /sibling/block()
 **********************************************************************/
class SiblingBlock : public Pothos::Block
{
public:
    static Pothos::Block *make(void)
    {
        return new SiblingBlock();
    }
};

static Pothos::BlockRegistry registerSiblingBlock(
    "/sibling/block", &SiblingBlock::make);
