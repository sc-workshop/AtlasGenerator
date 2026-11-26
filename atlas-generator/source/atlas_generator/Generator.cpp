#include "Generator.h"

#include "packing/ItemSelector.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wk::AtlasGenerator {
    Generator::Generator(const Config& config) :
        m_config(config) {
    }

    RawImage& Generator::get_atlas(size_t atlas) {
        return m_atlases[atlas];
    }

    bool Generator::validate_image(const RawImage& image) {
        if (1 > image.width() || 1 > image.height()) {
            return false;
        }

        if (image.is_complex())
            return false;

        return true;
    }

    bool Generator::pack_items(Image::PixelDepth atlas_type) {
        std::vector<packing::PackingItem> packer_items;
        packer_items.reserve(m_items.size());

        for (const Item& item : m_items) {
            packing::Path contour;
            contour.reserve(item.vertices.size() + 1);
            for (const auto& vertex : item.vertices) {
                contour.emplace_back(vertex.uv.x, vertex.uv.y);
            }
            contour.emplace_back(contour.front());
            packer_items.emplace_back(contour);
        }

        packing::PackingConfig config;
        config.parallel = m_config.parallel();
        config.sort_by_max_dimension = m_config.packing_priority() == PackingPriority::Density;
        if (m_config.progress) {
            config.progress = [&]() { m_config.progress(m_duplicate_item_counter + m_item_counter++); };
        }

        const packing::Box bin(m_config.width(),
                               m_config.height(),
                               {static_cast<packing::Coord>(std::ceil(m_config.width() / 2)),
                                static_cast<packing::Coord>(std::ceil(m_config.height() / 2))});

        size_t bin_offset = m_atlases.size();
        size_t bin_count = packing::pack_items(packer_items, bin, static_cast<int>(m_config.extrude()) * 2, config);

        // Gathering texture size info
        std::vector<Image::Size> sheet_size(bin_count);
        for (const packing::PackingItem& item : packer_items) {
            if (item.bin_id() == packing::BIN_ID_UNSET) {
                return false;
            }

            const packing::Box box = item.bounding_box();
            auto& size = sheet_size[static_cast<size_t>(item.bin_id())];

            const auto x = box.max.X;
            const auto y = box.max.Y;

            if (x > size.x) {
                size.x = static_cast<Image::SizeT>(x);
            }
            if (y > size.y) {
                size.y = static_cast<Image::SizeT>(y);
            }
        }

        m_atlases.reserve(sheet_size.size());
        for (const auto& size : sheet_size) {
            uint16_t width = static_cast<uint16_t>(size.x + m_config.extrude());
            uint16_t height = static_cast<uint16_t>(size.y + m_config.extrude());

            m_atlases.emplace_back(std::clamp<uint16_t>(width, 0, m_config.width()),
                                   std::clamp<uint16_t>(height, 0, m_config.height()),
                                   atlas_type);
        }

        for (size_t i = 0; m_items.size() > i; i++) {
            const packing::PackingItem& packer_item = packer_items[i];
            Item& item = m_items[i];

            const double rotation = packer_item.rotation();
            const int rotation_degree = static_cast<int>(rotation * 180.0 / std::numbers::pi_v<double>) % 360;

            const packing::Box box = packer_item.bounding_box();

            // Item Data
            item.texture_index = bin_offset + static_cast<size_t>(packer_item.bin_id());
            item.transform.rotation = rotation;
            item.transform.translation.x = static_cast<int32_t>(packer_item.translation().X);
            item.transform.translation.y = static_cast<int32_t>(packer_item.translation().Y);

            const auto x = static_cast<uint16_t>(box.min.X);
            const auto y = static_cast<uint16_t>(box.min.Y);

            place_image_to(m_config,
                           *item.image_ref(),
                           x,
                           y,
                           static_cast<Item::FixedRotation>(rotation_degree),
                           m_atlases[item.texture_index]);
        }

        return true;
    }

    void Generator::place_image_to(const Config& config,
                                   const RawImage& input,
                                   uint16_t x,
                                   uint16_t y,
                                   Item::FixedRotation rotation,
                                   RawImage& atlas) {
        const uint8_t extrude = config.extrude();
        RawImageRef image = CreateRef<RawImage>(input.width() + (extrude * 2),
                                                input.height() + (extrude * 2),
                                                input.depth(),
                                                input.colorspace());
        const int32_t dst_x = static_cast<int32_t>(x) - static_cast<int32_t>(extrude);
        const int32_t dst_y = static_cast<int32_t>(y) - static_cast<int32_t>(extrude);

        for (uint16_t h = 0; input.height() > h; h++) {
            for (uint16_t w = 0; input.width() > w; w++) {
                Memory::copy(input.at(w, h), image->at(w + extrude, h + extrude), input.pixel_size());
            }
        }

        {
            const uint16_t dstX = extrude;
            const uint16_t dstY = image->width() - extrude - 1;
            // left
            for (uint16_t h = 0; image->height() > h; h++) {
                uint8_t* pixel = image->at(dstX, h);

                for (uint16_t i = 0; extrude > i; i++) {
                    Memory::copy(pixel, image->at(i, h), image->pixel_size());
                }
            }

            // right
            for (uint16_t h = 0; image->height() > h; h++) {
                uint8_t* pixel = image->at(dstY, h);

                for (uint16_t i = 0; extrude > i; i++) {
                    Memory::copy(pixel, image->at(i + dstY + 1, h), image->pixel_size());
                }
            }
        }

        {
            const uint16_t dstX = extrude;
            const uint16_t dstY = image->height() - extrude - 1;
            // bottom
            for (uint16_t w = 0; image->width() > w; w++) {
                uint8_t* pixel = image->at(w, dstX);

                for (uint16_t i = 0; extrude > i; i++) {
                    Memory::copy(pixel, image->at(w, i), image->pixel_size());
                }
            }

            // top
            for (uint16_t w = 0; image->width() > w; w++) {
                uint8_t* pixel = image->at(w, dstY);

                for (uint16_t i = 0; extrude > i; i++) {
                    Memory::copy(pixel, image->at(w, i + dstY + 1), image->pixel_size());
                }
            }
        }

        uint16_t width = image->width() - 1;
        uint16_t height = image->height() - 1;

        auto rotate_pixel = [rotation, width, height](uint16_t x, uint16_t y) -> std::pair<uint16_t, uint16_t> {
            int16_t rotatedX = width - x;
            int16_t rotatedY = height - y;

            switch (rotation) {
                case wk::AtlasGenerator::Item::Rotation90:
                    return {rotatedY, x};
                case wk::AtlasGenerator::Item::Rotation180:
                    return {rotatedX, rotatedY};
                case wk::AtlasGenerator::Item::Rotation270:
                    return {y, rotatedX};
                default:
                    return {x, y};
            }
        };

        switch (rotation) {
            case wk::AtlasGenerator::Item::Rotation90:
            case wk::AtlasGenerator::Item::Rotation270:
                width = image->height() - 1;
                height = image->width() - 1;
                break;
            default:
                break;
        }

        for (uint16_t h = 0; image->height() > h; h++) {
            for (uint16_t w = 0; image->width() > w; w++) {
                uint16_t srcW = w;
                uint16_t srcH = h;

                if (rotation != Item::NoRotation) {
                    auto [rotatedX, rotatedY] = rotate_pixel(w, h);
                    srcW = rotatedX;
                    srcH = rotatedY;
                }

                const int32_t dstW = static_cast<int32_t>(srcW) + dst_x;
                const int32_t dstH = static_cast<int32_t>(srcH) + dst_y;

                if (dstW < 0 || dstW >= atlas.width())
                    continue;
                if (dstH < 0 || dstH >= atlas.height())
                    continue;

                uint8_t* pixel = image->at(w, h);
                uint8_t alpha = 255;

                switch (image->base_type()) {
                    case Image::BasePixelType::RGBA:
                        alpha = (*(ColorRGBA*) pixel).a;
                        break;
                    case Image::BasePixelType::LA:
                        alpha = (*(ColorLA*) pixel).a;
                        break;
                    default:
                        break;
                }

                if (config.alpha_threshold() > alpha) {
                    continue;
                }

                Memory::copy(pixel, atlas.at(dstW, dstH), input.pixel_size());
            }
        }
    };
}
