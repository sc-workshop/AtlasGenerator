#include <gtest/gtest.h>

#include "atlas_generator/Config.h"
#include "atlas_generator/limits.h"

using namespace wk::AtlasGenerator;

TEST(Config, AppliesClamps) {
	Config config(100, 9000, 0.1f, 99);

	EXPECT_EQ(config.width(), MinTextureDimension);
	EXPECT_EQ(config.height(), MaxTextureDimension);
	EXPECT_FLOAT_EQ(config.scale(), MinScaleFactor);
	EXPECT_EQ(config.extrude(), MaxExtrude);
}

TEST(Config, KeepsValuesInsideRange) {
	Config config(1024, 2048, 1.0f, 2);

	EXPECT_EQ(config.width(), 1024);
	EXPECT_EQ(config.height(), 2048);
	EXPECT_FLOAT_EQ(config.scale(), 1.0f);
	EXPECT_EQ(config.extrude(), 2);
	EXPECT_TRUE(config.parallel());
	EXPECT_EQ(config.alpha_threshold(), 3);
}

TEST(Config, ParallelCanBeDisabled) {
	Config config(512, 512, 1.0f, 0, false);

	EXPECT_FALSE(config.parallel());
}
