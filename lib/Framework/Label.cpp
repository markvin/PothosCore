// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework/Label.hpp>

Pothos::Label::Label(void):
    index(0),
    width(1)
{
    return;
}

Pothos::LabelIteratorRange::LabelIteratorRange(void):
    _begin(nullptr), _end(nullptr)
{
    return;
}

#include <Pothos/Managed.hpp>

static auto managedLabel = Pothos::ManagedClass()
    .registerConstructor<Pothos::Label>()
    .registerConstructor<Pothos::Label, const std::string &, const Pothos::Object &, const unsigned long long>()
    .registerMethod("toAdjusted", &Pothos::Label::toAdjusted<double, double>)
    .registerMethod("adjust", &Pothos::Label::adjust<double, double>)
    .registerField(POTHOS_FCN_TUPLE(Pothos::Label, id))
    .registerField(POTHOS_FCN_TUPLE(Pothos::Label, data))
    .registerField(POTHOS_FCN_TUPLE(Pothos::Label, index))
    .registerField(POTHOS_FCN_TUPLE(Pothos::Label, width))
    .commit("Pothos/Label");

static Pothos::LabelIteratorRange::const_iterator labelIterRangeAt(const Pothos::LabelIteratorRange &iter, const size_t index)
{
    return iter.begin() + index;
}

static auto managedLabelIteratorRange = Pothos::ManagedClass()
    .registerClass<Pothos::LabelIteratorRange>()
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::LabelIteratorRange, begin))
    .registerMethod(POTHOS_FCN_TUPLE(Pothos::LabelIteratorRange, end))
    .registerMethod("at", labelIterRangeAt)
    .commit("Pothos/LabelIteratorRange");

static const Pothos::Label &labelIterDeref(const Pothos::LabelIteratorRange::const_iterator &iter)
{
    return *iter;
}

static auto managedLabelConstIterator = Pothos::ManagedClass()
    .registerClass<Pothos::LabelIteratorRange::const_iterator>()
    .registerMethod("deref", labelIterDeref)
    .commit("Pothos/LabelConstIterator");

#include <Pothos/Plugin.hpp>
#include <Pothos/Util/CompareTo.hpp>

static int compareLabelIteratorRanges(const Pothos::LabelIteratorRange::const_iterator &lhs, const Pothos::LabelIteratorRange::const_iterator &rhs)
{
    return Pothos::Util::compareTo(lhs, rhs);
}

pothos_static_block(pothosLabelIteratorRegisterCompare)
{
    Pothos::PluginRegistry::addCall("/object/compare/labeliterator_to_labeliterator", &compareLabelIteratorRanges);
}

#include <Pothos/Object/Serialize.hpp>

template<class Archive>
void Pothos::Label::serialize(Archive & ar, const unsigned int)
{
    ar & this->id;
    ar & this->data;
    ar & this->index;
    ar & this->width;
}

template void Pothos::Label::serialize<Pothos::Archive::IStreamArchiver>(Pothos::Archive::IStreamArchiver &, const unsigned int);
template void Pothos::Label::serialize<Pothos::Archive::OStreamArchiver>(Pothos::Archive::OStreamArchiver &, const unsigned int);

POTHOS_OBJECT_SERIALIZE(Pothos::Label)
POTHOS_OBJECT_SERIALIZE(std::vector<Pothos::Label>)

//
// Register toString functions
//

#include <Pothos/Object.hpp>

#include <format>
#include <string>

static std::string labelToString(const Pothos::Label& label)
{
    return std::format("Pothos::Label (id: {}, data: {}, index: {}, width: {})",
                        label.id,
                        label.data.toString(),
                        std::to_string((unsigned long)label.index),
                        std::to_string(label.width));
}

static std::string labelIteratorRangeToString(const Pothos::LabelIteratorRange& labelIteratorRange)
{
    std::string output = "Pothos::LabelIteratorRange [";
    for(const auto& label: labelIteratorRange)
    {
        if(&label != labelIteratorRange.begin()) output += ", ";
        output += labelToString(label);
    }
    output += "]";

    return output;
}

pothos_static_block(pothosRegisterPothosLabelToString)
{
    Pothos::registerToStringFunc<Pothos::Label>(
        "Pothos/Label",
        &labelToString,
        false /*registerPointerTypes*/);
    Pothos::registerToStringFunc<Pothos::LabelIteratorRange>(
        "Pothos/LabelIteratorRange",
        &labelIteratorRangeToString,
        false /*registerPointerTypes*/);

}
