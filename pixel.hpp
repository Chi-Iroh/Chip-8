#pragma once

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif

#include <array>
#include <SFML/Graphics.hpp>

#ifdef _MSC_VER
#pragma warning(pop, 0)
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4820) // x bytes padding after data member y
#pragma warning(disable : 4514) // unreferenced inline function has been removed
#pragma warning(disable : 5045) // Spectre mitigation
#endif

class Pixel : public sf::Drawable {
public:
	static constexpr auto size{ 0x8 };

private:
	sf::Vector2f coords_{};
	bool white_{ false };

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const {
		sf::RectangleShape pixel{};
		pixel.setSize({ size, size });
		pixel.setPosition(coords_);
		pixel.setFillColor(white_ ? sf::Color::White : sf::Color::Black);
		target.draw(std::move(pixel), states);
	}

public:
	inline void flip() noexcept {
		white_ = !white_;
	}

	inline bool isWhite() const noexcept {
		return white_;
	}

	inline void setPosition(const sf::Vector2f& newCoords) noexcept {
		coords_ = newCoords;
	}

	/*inline auto getPosition() const noexcept {
		return coords_;
	}*/
};

class Screen : public sf::Drawable {
public:
	static constexpr auto widthInPixels{ 0x40 };
	static constexpr auto width{ widthInPixels * Pixel::size };

	static constexpr auto heightInPixels{ 0x20 };
	static constexpr auto height{ heightInPixels * Pixel::size };

	// number of pixels
	static constexpr auto size{ heightInPixels * widthInPixels };

private:
	// 1st element (index 0) is at the top left hand corner
	std::array<Pixel, size> pixels_{}; 

	virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const {
		for (auto& pixel : pixels_) {
			target.draw(pixel, states);
		}
	}

public:
	static constexpr inline std::pair<std::size_t, std::size_t> indexToCoords(std::size_t index) noexcept {
		return { (index % widthInPixels) * Pixel::size, (index / widthInPixels) * Pixel::size };
	}

	static constexpr inline std::size_t coordsToIndex(std::size_t x, std::size_t y) noexcept {
		return y * widthInPixels + x;
	}

	Screen() {
		for (std::size_t i{ 0u }; auto & pixel : pixels_) {
			const auto coords{ indexToCoords(i) };
			pixel.setPosition({ static_cast<float>(coords.first), static_cast<float>(coords.second)});
			// just for the fun
			/*if ((coords.first / Pixel::size) % (coords.second / Pixel::size + 1) != 0) {
				pixel.flip();
			}*/
			// remove until here to initialize normally (all pixels black)
			i++;
		}
	}

	void erase() {
		for (auto& pixel : pixels_) {
			if (pixel.isWhite()) {
				pixel.flip();
			}
		}
	}

	Pixel& operator[](std::size_t index) noexcept {
		return pixels_[index];
	}
};