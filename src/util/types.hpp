#pragma once
#include <string_view>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <span>

namespace cstd
{
	typedef signed char        int8_t;
	typedef short              int16_t;
	typedef int                int32_t;
	typedef long long          int64_t;
	typedef unsigned char      uint8_t;
	typedef unsigned short     uint16_t;
	typedef unsigned int       uint32_t;
	typedef unsigned long long uint64_t;

	typedef unsigned long long size_t;
}

template <class T, cstd::size_t Count>
using array_t = std::array<T, Count>;

template <class T>
using vector_t = std::vector<T>;

template <class T>
using span_t = std::span<T>;

template <class T>
using shared_ptr_t = std::shared_ptr<T>;

template <class T>
using unique_ptr_t = std::unique_ptr<T>;

using string_t = std::string;
using wstring_t = std::wstring;

using string_view_t = std::string_view;
using wstring_view_t = std::wstring_view;
