#pragma once

#include <stdint.h>
#include <functional>
#include <algorithm>

namespace wk
{
	namespace AtlasGenerator
	{
		class Config {
		public:
			Config(uint16_t width, uint16_t height, float scale, uint8_t extrude, bool parallel = true, uint8_t alpha_threshold = 3);
			virtual ~Config() = default;

		public:
			virtual uint16_t width() const { return m_max_width; };
			virtual uint16_t height() const { return m_max_height; };
			virtual float scale() const { return m_scale_factor; };
			virtual uint8_t extrude() const { return m_extrude; };
			virtual bool parallel() const { return m_parallel; };
			virtual uint8_t alpha_threshold() const { return m_alpha_threshold; };
			//virtual bool try_use_gpu() const { return m_try_use_gpu; };

		private:
			const uint16_t m_max_width;
			const uint16_t m_max_height;
			const float m_scale_factor;
			const uint8_t m_extrude;
			const bool m_parallel;
			const uint8_t m_alpha_threshold;
			//const bool m_try_use_gpu = true;

		public:
			std::function<void(size_t)> progress;
		};
	}
}