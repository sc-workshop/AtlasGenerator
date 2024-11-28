#pragma once

#include <vector>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <numeric>

#include "Config.h"
#include "Item/Item.h"
#include "PackagingException.h"
#include "Item/Iterator.h"

namespace wk {
	namespace AtlasGenerator
	{
		class Generator
		{
		public:

		public:
			Generator(const Config& config);
			~Generator() = default;

		public:
			size_t generate(Container<Item>& items);
			cv::Mat& get_atlas(uint8_t atlas);

		private:
			using Iterator = ItemIterator<size_t>::iterator;

			size_t generate(Container<Item>& items, ItemIterator<size_t>& it, int type);

			bool pack_items(int atlas_type);

		public:
			template<typename T, bool has_alpha, int alpha_channel = -1>
			void place_image_to(cv::Mat& src, uint8_t atlas_index, uint16_t x, uint16_t y)
			{
				using namespace cv;

				cv::Mat& dst = m_atlases[atlas_index];

				Size srcSize = src.size();
				Size dstSize = dst.size();

				for (uint16_t h = 0; srcSize.height > h; h++) {
					uint16_t dstH = h + y;
					if (dstH >= dstSize.height) continue;

					for (uint16_t w = 0; srcSize.width > w; w++) {
						uint16_t dstW = w + x;
						if (dstW >= dstSize.width) continue;

						T pixel = src.at<T>(h, w);

						if (has_alpha && pixel[alpha_channel] == 0) {
							continue;
						}

						dst.at<T>(dstH, dstW) = pixel;
					}
				}
			};
			static bool validate_image(cv::Mat& image);

		private:
			const Config m_config;

			Container<Item*> m_items;
			Container<size_t> m_duplicate_indices;

			Container<cv::Mat> m_atlases;

			size_t m_item_counter = 0;
		};
	}
}