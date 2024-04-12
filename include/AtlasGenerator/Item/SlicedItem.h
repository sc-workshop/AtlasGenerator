#pragma once

#include "Item.h"

// SC CORE
#include "math/rect.h"

namespace sc
{
	namespace AtlasGenerator
	{
		// Represantaion of 9-slice sprite
		class SlicedItem : public Item
		{
		public:
			enum class Area : uint8_t
			{
				BottomLeft = 1,
				BottomMiddle,
				BottomRight,
				MiddleLeft,
				Center,
				MiddleRight,
				TopLeft,
				TopMiddle,
				TopRight
			};
		public:
			SlicedItem(cv::Mat& image);
			SlicedItem(std::filesystem::path path);
			SlicedItem(cv::Scalar color);

		public:
			virtual bool is_rectangle() const;
			virtual bool is_sliced() const;

		public:
			void get_slice(Area area, Rect<int32_t>& guide, Rect<int32_t>& xy, Rect<uint16_t>& uv, Transformation xy_transform = Transformation());
		};
	}
}