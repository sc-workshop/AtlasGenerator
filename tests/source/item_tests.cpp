#include <gtest/gtest.h>

#include <vector>

#include "atlas_generator/Item/Item.h"
#include "atlas_generator/Item/Iterator.h"
#include "atlas_generator/Item/Vertex.h"

#include "core/image/raw_image.h"
#include "core/math/color_rgba.h"

using namespace wk;
using namespace wk::AtlasGenerator;

namespace
{
	RawImage filled_image(uint16_t width, uint16_t height, uint8_t value) {
		RawImage image(width, height, Image::PixelDepth::RGBA8);
		for (uint16_t y = 0; height > y; ++y) {
			for (uint16_t x = 0; width > x; ++x) {
				image.at<ColorRGBA>(x, y) = ColorRGBA{ value, value, value, 255 };
			}
		}
		return image;
	}
}

TEST(Item, SmallItemsUseRectangleFallback) {
	RawImage image = filled_image(10, 10, 255);
	Item item(image);

	EXPECT_TRUE(item.is_rectangle());
	EXPECT_FALSE(item.is_sliced());
}

TEST(Item, LargeItemsDoNotUseRectangleFallback) {
	RawImage image = filled_image(60, 60, 255);
	Item item(image);

	EXPECT_FALSE(item.is_rectangle());
}

TEST(Item, SlicedItemsUseRectangleFallback) {
	RawImage image = filled_image(60, 60, 255);
	Item item(image, true);

	EXPECT_TRUE(item.is_rectangle());
	EXPECT_TRUE(item.is_sliced());
}

TEST(Item, IdenticalImagesCompareEqual) {
	RawImage first = filled_image(8, 8, 123);
	RawImage second = filled_image(8, 8, 123);
	Item a(first);
	Item b(second);

	EXPECT_TRUE(a == b);
}

TEST(Item, DifferentImagesCompareNotEqual) {
	RawImage first = filled_image(8, 8, 123);
	RawImage second = filled_image(8, 8, 45);
	Item a(first);
	Item b(second);

	EXPECT_FALSE(a == b);
}

TEST(Vertex, DefaultAndExplicitConstruction) {
	Vertex empty;
	EXPECT_EQ(empty.xy.x, 0);
	EXPECT_EQ(empty.xy.y, 0);
	EXPECT_EQ(empty.uv.x, 0);
	EXPECT_EQ(empty.uv.y, 0);

	Vertex vertex(3, 4, 5, 6);
	EXPECT_EQ(vertex.xy.x, 3);
	EXPECT_EQ(vertex.xy.y, 4);
	EXPECT_EQ(vertex.uv.x, 5);
	EXPECT_EQ(vertex.uv.y, 6);
}

TEST(ItemIterator, WalksIndicesWithoutIndirection) {
	ItemIterator<size_t> iterator(2, 5);
	std::vector<size_t> values;

	for (size_t value : iterator) {
		values.push_back(value);
	}

	EXPECT_EQ(values, (std::vector<size_t>{ 2, 3, 4 }));
}

TEST(ItemIterator, WalksIndirectIndices) {
	std::vector<size_t> indices{ 7, 11, 13 };
	ItemIterator<size_t> iterator(0, indices.size(), indices);
	std::vector<size_t> values;

	for (size_t value : iterator) {
		values.push_back(value);
	}

	EXPECT_EQ(values, (std::vector<size_t>{ 7, 11, 13 }));
}

TEST(Transformation, RotatesAndTranslatesPoints) {
	Item::Transformation transform(0.0, Point(10, 20));
	Point_t<int> point{ 1, 2 };

	transform.transform_point(point);

	EXPECT_EQ(point.x, 11);
	EXPECT_EQ(point.y, 22);
}
