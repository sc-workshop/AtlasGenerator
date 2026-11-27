#include "atlas_generator/Item/Item.h"
#include "atlas_generator/Item/Iterator.h"
#include "atlas_generator/Item/Vertex.h"
#include "core/image/raw_image.h"
#include "core/math/color_rgba.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <numbers>
#include <vector>

using namespace wk;
using namespace wk::AtlasGenerator;

namespace {
    RawImage filled_image(uint16_t width, uint16_t height, uint8_t value) {
        RawImage image(width, height, Image::PixelDepth::RGBA8);
        for (uint16_t y = 0; height > y; ++y) {
            for (uint16_t x = 0; width > x; ++x) {
                image.at<ColorRGBA>(x, y) = ColorRGBA {value, value, value, 255};
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

TEST(Item, NineSlicesClippedByGuide) {
    RawImage image = filled_image(60, 60, 255);
    Item item(image, true);
    Config config(4096, 4096, 1.0f, 2, false);
    item.generate_image_polygon(config);

    Container<Container<Vertex>> result;
    Item::Transformation<float> transform(0.0, PointF(-30.f, -30.f));
    item.get_9slice(RectF(-10.f, 10.f, 10.f, -10.f), result, transform);

    const std::vector<std::vector<std::array<int, 4>>> expected = {
        {{-10, -10, 20, 20}, {-30, -10, 0, 20}, {-30, -30, 0, 0}, {-10, -30, 20, 0}},
        {{10, -10, 40, 20}, {-10, -10, 20, 20}, {-10, -30, 20, 0}, {10, -30, 40, 0}},
        {{30, -10, 60, 20}, {10, -10, 40, 20}, {10, -30, 40, 0}, {30, -30, 60, 0}},
        {{-10, 10, 20, 40}, {-30, 10, 0, 40}, {-30, -10, 0, 20}, {-10, -10, 20, 20}},
        {{10, 10, 40, 40}, {-10, 10, 20, 40}, {-10, -10, 20, 20}, {10, -10, 40, 20}},
        {{30, 10, 60, 40}, {10, 10, 40, 40}, {10, -10, 40, 20}, {30, -10, 60, 20}},
        {{-10, 10, 20, 40}, {-10, 30, 20, 60}, {-30, 30, 0, 60}, {-30, 10, 0, 40}},
        {{10, 10, 40, 40}, {10, 30, 40, 60}, {-10, 30, 20, 60}, {-10, 10, 20, 40}},
        {{30, 30, 60, 60}, {10, 30, 40, 60}, {10, 10, 40, 40}, {30, 10, 60, 40}},
    };

    ASSERT_EQ(result.size(), expected.size());
    for (size_t path_index = 0; path_index < expected.size(); path_index++) {
        ASSERT_EQ(result[path_index].size(), expected[path_index].size());
        for (size_t vertex_index = 0; vertex_index < expected[path_index].size(); vertex_index++) {
            const Vertex& vertex = result[path_index][vertex_index];
            EXPECT_EQ((std::array<int, 4> {vertex.xy.x, vertex.xy.y, vertex.uv.x, vertex.uv.y}),
                      expected[path_index][vertex_index]);
        }
    }
}

TEST(Item, Generate9SliceUsesAsymmetricGuideAxes) {
    const Container<PointF> points = {{-30.f, -30.f}, {30.f, -30.f}, {30.f, 30.f}, {-30.f, 30.f}};

    Container<Container<PointF>> regions;
    Item::Generate9Slice(RectF(-20.f, 15.f, 5.f, -5.f), regions, points);

    const std::vector<std::array<float, 4>> expected = {
        {-30.f, -30.f, -20.f, -5.f}, {-20.f, -30.f, 5.f, -5.f}, {5.f, -30.f, 30.f, -5.f},
        {-30.f, -5.f, -20.f, 15.f},  {-20.f, -5.f, 5.f, 15.f},  {5.f, -5.f, 30.f, 15.f},
        {-30.f, 15.f, -20.f, 30.f},  {-20.f, 15.f, 5.f, 30.f},  {5.f, 15.f, 30.f, 30.f},
    };

    ASSERT_EQ(regions.size(), expected.size());
    for (size_t region_index = 0; region_index < expected.size(); region_index++) {
        ASSERT_FALSE(regions[region_index].empty());
        float min_x = regions[region_index].front().x;
        float max_x = min_x;
        float min_y = regions[region_index].front().y;
        float max_y = min_y;
        for (const PointF& point : regions[region_index]) {
            min_x = std::min(min_x, point.x);
            max_x = std::max(max_x, point.x);
            min_y = std::min(min_y, point.y);
            max_y = std::max(max_y, point.y);
        }

        EXPECT_FLOAT_EQ(min_x, expected[region_index][0]);
        EXPECT_FLOAT_EQ(min_y, expected[region_index][1]);
        EXPECT_FLOAT_EQ(max_x, expected[region_index][2]);
        EXPECT_FLOAT_EQ(max_y, expected[region_index][3]);
    }
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

    EXPECT_EQ(values, (std::vector<size_t> {2, 3, 4}));
}

TEST(ItemIterator, WalksIndirectIndices) {
    std::vector<size_t> indices {7, 11, 13};
    ItemIterator<size_t> iterator(0, indices.size(), indices);
    std::vector<size_t> values;

    for (size_t value : iterator) {
        values.push_back(value);
    }

    EXPECT_EQ(values, (std::vector<size_t> {7, 11, 13}));
}

TEST(Transformation, RotatesAndTranslatesPoints) {
    Item::Transformation<int32_t> transform(0.0, Point(10, 20));
    Point_t<int> point {1, 2};

    transform.transform_point(point);

    EXPECT_EQ(point.x, 11);
    EXPECT_EQ(point.y, 22);
}

TEST(Transformation, UsesExactQuarterTurns) {
    constexpr double pi = std::numbers::pi_v<double>;

    {
        Item::Transformation<int32_t> transform(pi / 2.0, Point(0, 20));
        Point_t<int> point {7, 0};

        transform.transform_point(point);

        EXPECT_EQ(point.x, 0);
        EXPECT_EQ(point.y, 27);
    }

    {
        Item::Transformation<int32_t> transform(pi, Point(20, 0));
        Point_t<int> point {7, 0};

        transform.transform_point(point);

        EXPECT_EQ(point.x, 13);
        EXPECT_EQ(point.y, 0);
    }

    {
        Item::Transformation<int32_t> transform(3.0 * pi / 2.0, Point(0, 20));
        Point_t<int> point {7, 0};

        transform.transform_point(point);

        EXPECT_EQ(point.x, 0);
        EXPECT_EQ(point.y, 13);
    }
}
