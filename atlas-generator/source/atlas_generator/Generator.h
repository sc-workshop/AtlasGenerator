#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <numeric>
#include <execution>
#include <cmath>

#include "Config.h"
#include "Item/Item.h"
#include "PackagingException.h"
#include "Item/Iterator.h"
#include "core/parallel/enumerate.h"

namespace wk {
	namespace AtlasGenerator
	{
		class Generator
		{
		public:
			Generator(const Config& config);
			~Generator() = default;

		public:
			template<typename T = Item>
			size_t generate(Container<T>& items)
			{
				if (items.empty()) return 0;

				m_item_counter = 0;
				m_duplicate_item_counter = 0;

				std::map<Image::PixelDepth, size_t> texture_variants;
				for (size_t i = 0; items.size() > i; i++)
				{
					Item& item = items[i];

					if (!Generator::validate_image(item.image()))
					{
						throw PackagingException(PackagingException::Reason::UnsupportedImage, i);
					}

					texture_variants[item.image().depth()]++;
				}

				if (texture_variants.size() == 1)
				{
					const Item& first_item = items[0];
					auto type = first_item.image().depth();

					// iterate just by items vector
					auto it = ItemIterator<size_t>(0, items.size());
					return generate<T>(items, it, type);
				}

				size_t bin_count = 0;
				for (auto it = texture_variants.begin(); it != texture_variants.end(); ++it)
				{
					auto depth = it->first;
					Container<size_t> item_indices;
					item_indices.reserve(it->second);

					for (size_t i = 0; items.size() > i; i++)
					{
						Item& item = items[i];
						if (item.image().depth() == depth)
						{
							item_indices.push_back(i);
						}
					}

					m_items.reserve(item_indices.size());
					m_duplicate_indices.reserve(item_indices.size());

					// iterate only by current type items
					auto item_it = ItemIterator<size_t>(0, item_indices.size(), item_indices);
					bin_count += generate<T>(items, item_it, depth);
				}

				return bin_count;
			}

			RawImage& get_atlas(size_t atlas);

		private:
			using Iterator = ItemIterator<size_t>::iterator;

			template<typename T = Item>
			size_t generate(Container<T>& items, ItemIterator<size_t>& item_iterator, Image::PixelDepth depth)
			{
				Container<size_t> inverse_duplicate_indices;
				inverse_duplicate_indices.reserve(items.size());

				for (auto it = item_iterator.begin(); it != item_iterator.end(); ++it)
				{
					const size_t i = *it;
					Item& item = items[i];

					// Searching for duplicates
					{
						size_t item_index = SIZE_MAX;

						auto item_it = std::find_if(std::execution::par_unseq, m_items.begin(), m_items.end(),
							[&item](const Item& other)
							{
								return item == other;
							}
						);

						if (item_it != m_items.end())
						{
							item_index = std::distance(m_items.begin(), item_it);
							m_duplicate_indices[i] = item_index;
							m_duplicate_item_counter++;
						}

						if (item_index != SIZE_MAX) continue;
					}
					inverse_duplicate_indices.push_back(i);
					m_items.push_back(item);
				}

				std::launch policy = std::launch::deferred;
#if !WK_DEBUG
				policy |= std::launch::async;
#endif // !WK_DEBUG

				parallel::enumerate(
					m_items.begin(), m_items.end(), [&](Item& item, size_t)
					{
						if (item.status() == Item::Status::Unset)
						{
							item.generate_image_polygon(m_config);
						}
					}, policy
				);

				for (size_t i = 0; m_items.size() > i; i++)
				{
					Item& item = m_items[i];

					if (item.vertices.empty())
					{
						throw PackagingException(PackagingException::Reason::InvalidPolygon, inverse_duplicate_indices[i]);
					}

					if (item.width() > m_config.width() || item.height() > m_config.height())
					{
						throw PackagingException(PackagingException::Reason::TooBigImage, inverse_duplicate_indices[i]);
					}
				}

				size_t current_atlas_count = m_atlases.size();
				if (!pack_items(depth))
				{
					throw PackagingException(PackagingException::Reason::Unknown);
				};

				for (auto iter = m_duplicate_indices.begin(); iter != m_duplicate_indices.end(); ++iter)
				{
					size_t desination_index = iter->first;
					size_t source_index = iter->second;

					Item& destination = items[desination_index];
					Item& source = m_items[source_index];

					destination.texture_index = source.texture_index;
					destination.vertices = source.vertices;
					destination.transform = source.transform;
				}

				m_duplicate_indices.clear();
				m_items.clear();

				return m_atlases.size() - current_atlas_count;
			}

			bool pack_items(Image::PixelDepth atlas_type);

		public:
			void place_image_to(RawImageRef src, size_t atlas_index, uint16_t x, uint16_t y, Item::FixedRotation rotation);

			static bool validate_image(const RawImage& image);

		private:
			const Config m_config;

			Container<std::reference_wrapper<Item>> m_items;
			std::unordered_map<size_t, size_t> m_duplicate_indices;

			Container<RawImage> m_atlases;

			size_t m_item_counter = 0;
			size_t m_duplicate_item_counter = 0;
		};
	}
}