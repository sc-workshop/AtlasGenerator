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
				uint16_t width = (uint16_t)(size.x + m_config.extrude());
				uint16_t height = (uint16_t)(size.y + m_config.extrude());

				m_atlases.emplace_back(
					std::clamp<uint16_t>(width, 0, m_config.width()), 
					std::clamp<uint16_t>(height, 0, m_config.height()),
					atlas_type
				);
			}

			for (size_t i = 0; m_items.size() > i; i++) {
				libnest2d::Item packer_item = packer_items[i];
				Item& item = m_items[i];

				auto rotation = packer_item.rotation();
				int rotation_degree = ((int)rotation.toDegrees()) % 360;

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

		void Generator::place_image_to(RawImageRef input, size_t atlas_index, uint16_t x, uint16_t y, Item::FixedRotation rotation)
		{
			//bool colorfill = src->width() == 1 && src->height() == 1;
			//if (colorfill)
			//{
			//	x -= m_config.extrude() / 2;
			//	y -= m_config.extrude() / 2;
			//
			//	ColorRGBA& color = src->at<ColorRGBA>(0, 0);
			//	src = CreateRef<RawImage>(
			//		(uint16_t)(m_config.extrude() + 1),
			//		(uint16_t)(m_config.extrude() + 1),
			//		Image::PixelDepth::RGBA8
			//	);
			//
			//	for (uint16_t h = 0; src->height() > h; h++)
			//	{
			//		for (uint16_t w = 0; src->width() > w; w++)
			//		{
			//			ColorRGBA& pixel = src->at<ColorRGBA>(w, h);
			//			pixel = color;
			//		}
			//	}
			//}

			const uint8_t extrude = m_config.extrude();
			RawImageRef image = CreateRef<RawImage>(
				input->width() + (extrude * 2),
				input->height() + (extrude * 2),
				input->depth(), input->colorspace()
			);
			x -= extrude;
			y -= extrude;
			
			for (uint16_t h = 0; input->height() > h; h++)
			{
				for (uint16_t w = 0; input->width() > w; w++)
				{
					Memory::copy(input->at(w, h), image->at(w + extrude, h + extrude), input->pixel_size());
				}
			}

			{
				const uint16_t dstX = extrude;
				const uint16_t dstY = image->width() - extrude - 1;
				// left
				for (uint16_t h = 0; image->height() > h; h++)
				{
					uint8_t* pixel = image->at(dstX, h);
				
					for (uint16_t i = 0; extrude > i; i++)
					{
						Memory::copy(pixel, image->at(i, h), image->pixel_size());
					}
				}

				// right
				for (uint16_t h = 0; image->height() > h; h++)
				{
					uint8_t* pixel = image->at(dstY, h);
				
					for (uint16_t i = 0; extrude > i; i++)
					{
						Memory::copy(pixel, image->at(i + dstY + 1, h), image->pixel_size());
					}
				}
			}

			{
				const uint16_t dstX = extrude;
				const uint16_t dstY = image->height() - extrude - 1;
				// bottom
				for (uint16_t w = 0; image->width() > w; w++)
				{
					uint8_t* pixel = image->at(w, dstX);

					for (uint16_t i = 0; extrude > i; i++)
					{
						Memory::copy(pixel, image->at(w, i), image->pixel_size());
					}
				}

				// top
				for (uint16_t w = 0; image->width() > w; w++)
				{
					uint8_t* pixel = image->at(w, dstY);
				
					for (uint16_t i = 0; extrude > i; i++)
					{
						Memory::copy(pixel, image->at(w, i + dstY + 1), image->pixel_size());
					}
				}
			}
			

			uint16_t width = image->width() - 1;
			uint16_t height = image->height() - 1;

			auto rotate_pixel = [rotation, width, height](uint16_t x, uint16_t y) -> std::pair<uint16_t, uint16_t>
				{
					int16_t rotatedX = width - x;
					int16_t rotatedY = height - y;

					switch (rotation)
					{
					case wk::AtlasGenerator::Item::Rotation90:
						return { rotatedY, x };
					case wk::AtlasGenerator::Item::Rotation180:
						return { rotatedX, rotatedY };
					case wk::AtlasGenerator::Item::Rotation270:
						return { y, rotatedX };
					default:
						return { x, y };
					}
				};

			switch (rotation)
			{
			case wk::AtlasGenerator::Item::Rotation90:
			case wk::AtlasGenerator::Item::Rotation270:
				width = image->height() - 1;
				height = image->width() - 1;
				break;
			default:
				break;
			}

			auto& atlas = m_atlases[atlas_index];

			for (uint16_t h = 0; image->height() > h; h++)
			{
				for (uint16_t w = 0; image->width() > w; w++)
				{
					uint16_t srcW = w;
					uint16_t srcH = h;

					if (rotation != Item::NoRotation)
					{
						auto [rotatedX, rotatedY] = rotate_pixel(w, h);
						srcW = rotatedX;
						srcH = rotatedY;
					}

					uint16_t dstW = srcW + x;
					uint16_t dstH = srcH + y;

					if (0 > dstW || dstW >= atlas.width()) continue;
					if (0 > dstH || dstH >= atlas.height()) continue;

					uint8_t* pixel = image->at(w, h);
					uint8_t alpha = 255;

					switch (image->base_type())
					{
					case Image::BasePixelType::RGBA:
						alpha = (*(ColorRGBA*)pixel).a;
						break;
					case Image::BasePixelType::LA:
						alpha = (*(ColorLA*)pixel).a;
						break;
					default:
						break;
					}

					if (m_config.alpha_threshold() > alpha)
					{
						continue;
					}

					Memory::copy(pixel, atlas.at(dstW, dstH), input->pixel_size());
				}
			}
		};
	}
}