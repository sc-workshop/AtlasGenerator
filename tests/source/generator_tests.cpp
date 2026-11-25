#include <gtest/gtest.h>

#include <vector>

#include "atlas_generator/Config.h"
#include "atlas_generator/Generator.h"
#include "atlas_generator/Item/Item.h"
#include "atlas_generator/PackagingException.h"

#include "core/image/raw_image.h"

using namespace wk;
using namespace wk::AtlasGenerator;

TEST(Generator, ValidateImageAcceptsSimpleImage) {
	RawImage image(4, 4, Image::PixelDepth::RGBA8);

	EXPECT_TRUE(Generator::validate_image(image));
}

TEST(Generator, GenerateReturnsZeroForEmptyInput) {
	Config config(512, 512, 1.0f, 2);
	Generator generator(config);
	std::vector<Item> items;

	EXPECT_EQ(generator.generate(items), 0u);
}

TEST(PackagingException, CarriesReasonAndItemIndex) {
	PackagingException exception(PackagingException::Reason::TooBigImage, 7);

	EXPECT_EQ(exception.reason(), PackagingException::Reason::TooBigImage);
	EXPECT_EQ(exception.index(), 7u);
	EXPECT_STREQ(exception.what(), "Image is too big for packaging");
}

TEST(PackagingException, UsesSyntheticIndexWhenUnset) {
	PackagingException exception(PackagingException::Reason::Unknown);

	EXPECT_EQ(exception.index(), SIZE_MAX);
	EXPECT_STREQ(exception.what(), "Unknown exception");
}
