/**
 * Copyright(c) 2020-present, Odysseas Georgoudis & quill contributors.
 * Distributed under the MIT License (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <string>
#include <type_traits>

#include <chrono>
#include <string>
#include <utility>

#ifdef _LIBCPP_VERSION
  // libc++ does not forward declares tuple.
  #include <tuple>
#endif

/**
 * Below are type traits to determine whether an object is marked as copyable.
 * The traits are used to decide about
 *
 * a) it is safe to copy the object in the queue and safe to format it later
 * b) it could be unsafe to copy the object in the queue and we first have to format it on the caller thread
 *
 * Since with libfmt it is not possible to format pointers (except void*) we don't handle every
 * case with pointers.
 *
 * A safe to copy object meets one of the following criteria :
 * a) built arithmetic types
 * b) trivial types
 * c) strings
 * d) explictly tagged by the user as copy_loggable
 * e) std::duration types
 * f) containers of the above types
 * g) std::pairs of the above types
 * h) std::tuples of the above types
 */

// clang-format off
namespace quill
{
namespace detail
{

/**
 * C++14 implementation of C++17's std::bool_constant.
 */
template <bool B>
using bool_constant = std::integral_constant<bool, B>;

/**
 * C++14 implementation of C++17's std::conjunction
 */
template <typename...>
struct conjunction : std::true_type
{
};

template <typename B1>
struct conjunction<B1> : B1
{
};

template <typename B1, typename... Bn>
struct conjunction<B1, Bn...> : std::conditional_t<static_cast<bool>(B1::value),
                                                   conjunction<Bn...>,
                                                   B1
                                                   >
{
};

template <typename... B1>
using conjunction_t = typename conjunction<B1...>::type;

template <typename... B1>
constexpr bool conjunction_v = conjunction<B1...>::value;

/**
 * C++14 implementation of C++17's std::disjunction.
 */
template <typename...>
struct disjunction : std::false_type
{
};

template <typename B1>
struct disjunction<B1> : B1
{
};

template <typename B1, typename... Bn>
struct disjunction<B1, Bn...> : std::conditional_t<static_cast<bool>(B1::value),
                                                   B1,
                                                   disjunction<Bn...>
                                                   >
{
};

template <typename... B1>
using disjunction_t = typename disjunction<B1...>::type;

template <typename... B1>
constexpr bool disjunction_v = disjunction<B1...>::value;

/**
 * C++14 implementation of C++17's std::negation.
 */
template <typename B>
struct negation : bool_constant<!static_cast<bool>(B::value)>
{
};

template <typename B>
using negation_t = typename negation<B>::type;

template <typename B>
constexpr bool negation_v = negation<B>::value;

/**************************************************************************************************/
/* Type Traits for copyable object detection */
/**************************************************************************************************/

/**
 * Used to detect if an object has a specific member.
 * This is different than enable_if_t which accepts a bool not a type
 */
template <typename T, typename R = void>
struct enable_if_type
{
  typedef R type;
};

template <typename T, typename R = void>
using enable_if_type_t = typename enable_if_type<T, R>::type;

/**
 * A trait to determine if the object is safe be copied as it is
 * Specialisations follow later
 * A copyable object is either a basic type, an stl container or a tagged one
 */
template <typename T, typename T2 = void>
struct is_copyable : std::false_type
{
};

template <typename T>
using is_copyable_t = typename is_copyable<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_copyable_v = is_copyable<std::remove_cv_t<T>>::value;

/**
 * A trait to detect an object was tagged as copy_loggable
 * @tparam T
 * @tparam T2
 */
template <typename T, typename T2 = void>
struct is_copy_loggable : std::false_type
{
};

template <typename T>
struct is_copy_loggable<T, enable_if_type_t<typename T::copy_loggable>> : std::true_type
{
};

template <typename T>
using is_copy_loggable_t = typename is_copy_loggable<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_copy_loggable_v = is_copy_loggable<std::remove_cv_t<T>>::value;

/**
 * is std::string ?
 */
template <typename T>
struct is_string : std::false_type
{
};

template <typename CharT, typename Traits, typename Allocator>
struct is_string<std::basic_string<CharT, Traits, Allocator>> : std::true_type
{
};

template <typename T>
using is_string_t = typename is_string<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_string_v = is_string<std::remove_cv_t<T>>::value;

/**
 * is std::pair ?
 */
template <typename T>
struct is_pair : std::false_type
{
};

template <typename T1, typename T2>
struct is_pair<std::pair<T1, T2>> : std::true_type
{
};

template <typename T>
using is_pair_t = typename is_pair<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_pair_v = is_pair<std::remove_cv_t<T>>::value;

/**
 * Check if each element of the pair is copyable
 */
template <typename T, typename T2 = void>
struct is_copyable_pair : std::false_type
{
};

/**
 * Enable only in the case of a std::pair and do the check
 */
template <typename T>
struct is_copyable_pair<T, std::enable_if_t<is_pair_v<T>>>
  : conjunction<is_copyable<std::remove_cv_t<typename T::first_type>>,
                is_copyable<std::remove_cv_t<typename T::second_type>>
                >
{
};

template <typename T>
using is_copyable_pair_t = typename is_copyable_pair<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_copyable_pair_v = is_copyable_pair<std::remove_cv_t<T>>::value;

/**
 * is it a container ?
 */
template <typename T, typename T2 = void>
struct is_container_helper : std::false_type
{
};

template <typename T>
struct is_container_helper<T, enable_if_type_t<typename T::iterator>> : std::true_type
{
};

/**
 * Check for a container but ignoring std::string
 */
template <typename T, typename T2 = void>
struct is_container : std::false_type
{
};

/**
 * Enable only when not a std::string as they also are like containers
 */
template <typename T>
struct is_container<T, std::enable_if_t<negation_v<is_string<T>>>>
  : is_container_helper<T>
{
};

template <typename T>
using is_container_t = typename is_container<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_container_v = is_container<std::remove_cv_t<T>>::value;

/**
 * Check if a container is copyable
 */
template <typename T, typename T2 = void>
struct is_copyable_container : std::false_type
{
};

/**
 * Enable the check only for containers. is_copyable is going to recursively call other if
 * for example we have std::vector<std::vector<int>>
 */
template <typename T>
struct is_copyable_container<T, std::enable_if_t<is_container_v<T>>>
  : is_copyable<std::remove_cv_t<typename T::value_type>>
{
};

template <typename T>
using is_copyable_container_t = typename is_copyable_container<std::remove_cv_t<T>>::type;

template <typename T>
constexpr bool is_copyable_container_v = is_copyable_container<std::remove_cv_t<T>>::value;

/**
 * A user defined object that was tagged by the user to be copied
 */
template <typename T>
struct is_user_defined_copyable : conjunction<std::is_class<T>,
                                              negation<std::is_trivial<T>>,
                                              is_copy_loggable<T>
                                             >
{};

/**
 * An object is copyable if it meets one of the following criteria
 */
template <typename T>
struct filter_copyable : disjunction<std::is_trivial<T>,
                                     std::is_arithmetic<T>,
                                     is_string<T>,
                                     is_copyable_pair<T>,
                                     is_copyable_container<T>,
                                     is_user_defined_copyable<T>
                                     >
{};

/**
 * Specialisation of is_copyable we defined on top to apply our copyable filter
 */
template <typename T>
struct is_copyable<T, std::enable_if_t<filter_copyable<T>::value>> : std::true_type
{
};

// clang-format on

} // namespace detail
} // namespace quill