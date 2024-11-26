#pragma once

#include <vector>
#include <type_traits>
#include <iterator>
#include <cstddef>
#include <optional>

#include <core/memory/ref.h>

namespace wk::AtlasGenerator
{
	template <typename T = size_t>
	struct ItemIterator {
		static_assert(std::is_integral<T>::value, "Integral type required.");

		using IndicesValue_t = std::vector<T>;
		using Indices_t = std::optional<std::reference_wrapper<IndicesValue_t>>;

		struct iterator
		{
			using iterator_category = std::forward_iterator_tag;    // c++17 style, c++20 would use std::forward_iterator
			using difference_type = std::ptrdiff_t;
			using value_type = T;
			using pointer = value_type*;
			using reference = value_type&;

			iterator(value_type val = 0, Indices_t indices = std::nullopt) : m_value(val), m_indices(indices) {}

			reference operator*() {
				if (m_indices.has_value())
				{
					return m_indices.value().get().at(m_value);
				}

				return m_value;
			}
			pointer operator->() { 
				if (m_indices.has_value())
				{
					return &m_indices.value().get().at(m_value);
				}

				return &m_value;
			}

			iterator& operator++() { m_value++; return *this; }
			iterator  operator++(int) { iterator tmp = *this; ++(*this); return tmp; }

			friend bool operator== (const iterator& a, const iterator& b) { return a.m_value == b.m_value; };
			friend bool operator!= (const iterator& a, const iterator& b) { return a.m_value != b.m_value; };

		protected:
			value_type m_value;
			Indices_t m_indices;
		};

		ItemIterator(T begin, T end, Indices_t indices = std::nullopt) : m_begin(begin, indices), m_end(end, indices) {
			if (end < begin) m_end = m_begin;
		}

		iterator begin() { return m_begin; }
		iterator end() { return m_end; }

		const char* typeinfo() const { return typeid(T).name(); }

	private:
		iterator m_begin;
		iterator m_end;
	};
}