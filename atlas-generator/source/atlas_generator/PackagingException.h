#pragma once

#include "core/exception/exception.h"

namespace wk
{
	namespace AtlasGenerator
	{
		class PackagingException : public std::exception
		{
		public:
			enum class Reason
			{
				Unknown = 0,
				TooBigImage,
				UnsupportedImage,
				InvalidPolygon
			};

		public:
			PackagingException(Reason reason, size_t item_index = SIZE_MAX)
			{
				m_reason = reason;
				m_item_index = item_index;

				switch (reason)
				{
				case Reason::TooBigImage:
					m_message = "Image is too big for packaging";
					break;
				case Reason::UnsupportedImage:
					m_message = "Image has unsupported type";
					break;
				case Reason::InvalidPolygon:
					m_message = "Failed to generate polygon for image";
					break;
				default:
					m_message = "Unknown exception";
					break;
				}
			}
		public:
			Reason reason() const
			{
				return m_reason;
			};

			size_t index() const
			{
				return m_item_index;
			};

			const char* what() const noexcept override { return m_message.c_str(); }

		private:
			std::string m_message;
			Reason m_reason;
			size_t m_item_index;
		};
	}
}