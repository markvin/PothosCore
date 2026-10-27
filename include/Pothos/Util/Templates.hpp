///
/// \file Util/Templates.hpp
///
/// Utility templates and metaprograming classes.
///
/// \copyright
/// Copyright (c) 2017-2017 Josh Blum
///                    2020 Nicholas Corgan
/// SPDX-License-Identifier: BSL-1.0
///

#pragma once
#include <Pothos/Config.hpp>
#include <complex> // std::complex
#include <type_traits> //std::decay, std::unwrap_ref_decay_t
#include <functional> //std::reference_wrapper
#include <utility> //std::index_sequence

namespace Pothos {
namespace Util {

/*!*********************************************************************
 * \defgroup unwrapref special_decay_t
 * special_decay_t = decay + unwrapping a reference wrapper
 * \{
 **********************************************************************/

/*!
 * Extract the type held within a reference wrapper (T& for std::reference_wrapper<T>, else T).
 * Same as std::unwrap_reference, kept for toolkits.
 * \tparam T the input type to the template
 */
template <typename T>
using unwrap_refwrapper = std::unwrap_reference<T>;

/*!
 * Strip const and reference from a type, and remove optional reference wrapper.
 * Same as std::unwrap_ref_decay_t, kept for toolkits.
 * \tparam T the input type to the template
 */
template <typename T>
using special_decay_t = std::unwrap_ref_decay_t<T>;
//! \}

/*!*********************************************************************
 * \defgroup intseq Integer sequences
 * Aliases of the standard integer sequences, kept for toolkits.
 * \see http://en.cppreference.com/w/cpp/utility/integer_sequence
 * \{
 **********************************************************************/

//! A parameter pack of incrementing numbers 0 to N-1
template<typename T, T... Ints>
using integer_sequence = std::integer_sequence<T, Ints...>;

//! Specialize the integer sequence for size_t
template<std::size_t... Ints>
using index_sequence = std::index_sequence<Ints...>;

//! Make an integer sequence from the length
template<typename T, T N>
using make_integer_sequence = std::make_integer_sequence<T, N>;

//! Make an index sequence from the length
template<std::size_t N>
using make_index_sequence = std::make_index_sequence<N>;

//! Make an index sequence from a parameter pack
template<typename... T>
using index_sequence_for = std::index_sequence_for<T...>;
//! \}

/*!*********************************************************************
 * Disable SFINAE type used with universal reference constructors
 * \see http://ericniebler.com/2013/08/07/universal-references-and-the-copy-constructo/
 **********************************************************************/
template<typename BaseType, typename OtherType>
using disable_if_same = typename std::enable_if<
    not std::is_same<BaseType, typename std::decay<OtherType>::type>::value>::type;

/*!*********************************************************************
 * \defgroup complexsfinae Use when functions require different behaviors
 * for scalar vs. complex types.
 * \{
 **********************************************************************/

template <typename T>
struct is_complex: std::false_type {};

template <typename T>
struct is_complex<std::complex<T>>: std::true_type {};

template <typename T, typename ReturnType>
using disable_if_complex = typename std::enable_if<
    not is_complex<typename std::decay<T>::type>::value,
    ReturnType>::type;

template <typename T, typename ReturnType>
using enable_if_complex = typename std::enable_if<
    is_complex<typename std::decay<T>::type>::value,
    ReturnType>::type;

//! \}

} //namespace Util
} //namespace Pothos
