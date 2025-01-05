#include "Generator.h"
#include "limits.h"

#include <libnest2d/libnest2d.hpp>

namespace wk
{
	namespace AtlasGenerator
	{
		Generator::Generator(const Config& config) : m_config(config)
		{
		}

		RawImage& Generator::get_atlas(size_t atlas)
		{
			return m_atlases[atlas];
		}

		bool Generator::validate_image(const RawImage& image)
		{
			if (1 > image.width() || 1 > image.height())
			{
				return false;
			}

			if (image.is_complex()) return false;

			return true;
		}

		bool Generator::pack_items(Image::PixelDepth atlas_type)
		{
			// Vector with polygons for libnest2d
			std::vector<libnest2d::Item> packer_items;
			packer_items.reserve(m_items.size());

			for (const Item& item : m_items)
			{
				libnest2d::Item& packer_item = packer_items.emplace_back(
					std::vector<libnest2d::Point>(item.vertices.size() + 1)
				);

				for (uint16_t i = 0; packer_item.vertexCount() > i; i++) {
					if (i == item.vertices.size()) { // End point for libnest2d
						packer_item.setVertex(i, { item.vertices[0].uv.x, item.vertices[0].uv.y });
					}
					else {
						packer_item.setVertex(i, { item.vertices[i].uv.x, item.vertices[i].uv.y });
					}
				}
			}

			libnest2d::NestConfig<libnest2d::NfpPlacer, libnest2d::FirstFitSelection> cfg;
			cfg.placer_config.alignment = libnest2d::NestConfig<>::Placement::Alignment::DONT_ALIGN;
			cfg.placer_config.starting_point = libnest2d::NestConfig<>::Placement::Alignment::BOTTOM_LEFT;
			cfg.placer_config.parallel = m_config.parallel();
#if WK_DEBUG
			cfg.placer_config.accuracy = 1.0f;
#else
			cfg.placer_config.accuracy = 0.6f;
#endif
			cfg.selector_config.verify_items = false;
			//cfg.selector_config.texture_parallel_hard = m_config.parallel();

			libnest2d::NestControl control;
			if (m_config.progress)
			{
				control.progressfn = [&](unsigned)
					{
						m_config.progress(m_duplicate_item_counter + m_item_counter++);
					};
			}

			size_t bin_offset = m_atlases.size();
			size_t bin_count = nest(
				packer_items,
				libnest2d::Box(
					m_config.width(), m_config.height(),
					{ (int)ceil(m_config.width() / 2), (int)ceil(m_config.height() / 2) }),
				m_config.extrude() * 2, cfg, control
			);

			// Gathering texture size info
			std::vector <Image::Size> sheet_size(bin_count);
			for (libnest2d::Item item : packer_items) {
				if (item.binId() == libnest2d::BIN_ID_UNSET)
				{
					return false;
				};

				auto box = item.boundingBox();
				auto& size = sheet_size[item.binId()];

				auto x = libnest2d::getX(box.maxCorner());
				auto y = libnest2d::getY(box.maxCorner());

				if (x > size.x) {
					size.x = (Image::SizeT)x;
				}
				if (y > size.y) {
					size.y = (Image::SizeT)y;
				}
			}

			m_atlases.reserve(sheet_size.size());
			for (const auto& size : sheet_size)
			{
				m_atlases.emplace_back(
					size.x + m_config.extrude(), size.y + m_config.extrude(),
					atlas_type
				);
			}

			for (size_t i = 0; m_items.size() > i; i++) {
				libnest2d::Item packer_item = packer_items[i];
				Item& item = m_items[i];

				auto rotation = packer_item.rotation();
				int rotation_degree = ((int)rotation.toDegrees()) % 360;
				if (rotation_degree < 0) {
					rotation_degree += 360;
				}

				auto box = packer_item.boundingBox();

				// Item Data
				item.texture_index = bin_offset + packer_item.binId();
				item.transform.rotation = rotation;
				item.transform.translation.x = (int32_t)libnest2d::getX(packer_item.translation());
				item.transform.translation.y = (int32_t)libnest2d::getY(packer_item.translation());

				auto index = item.texture_index;
				auto x = (uint16_t)(libnest2d::getX(box.minCorner()));
				auto y = (uint16_t)(libnest2d::getY(box.minCorner()));
				


				place_image_to(
					item.image_ref(),
					index,
					x,
					y,
					(Item::FixedRotation)rotation_degree
				);
			}

			return true;
		}
	}
}